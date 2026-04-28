// Package bucketsapi exposes datum-server's object storage over HTTP.
//
// Routes (mounted at /storage):
//
//	GET    /storage                                — list buckets
//	POST   /storage/{bucket}                       — ensure bucket exists
//	DELETE /storage/{bucket}                       — delete an empty bucket
//	GET    /storage/{bucket}                       — list objects (?prefix=, ?limit=)
//	PUT    /storage/{bucket}/*path                 — upload an object
//	GET    /storage/{bucket}/*path                 — download (presigned URL OR auth)
//	HEAD   /storage/{bucket}/*path                 — stat metadata
//	DELETE /storage/{bucket}/*path                 — delete an object
//	POST   /storage/{bucket}/presign?path=&method= — issue presigned URL
//
// Authentication: every mutating endpoint requires user authentication; GET
// endpoints accept either user auth OR a valid presigned URL (LocalFS only).
package bucketsapi

import (
	"errors"
	"io"
	"net/http"
	"strconv"
	"strings"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/buckets"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// Handler binds a Backend + storage to HTTP.
type Handler struct {
	Backend buckets.Backend
	// LocalFS is the same backend cast for presign verification (when
	// non-nil and the active backend is localfs). Optional.
	LocalFS *buckets.LocalFS
	Store   storage.Provider
	// Publish is an optional MQTT publisher used to emit bucket lifecycle
	// events. Topics:
	//   bucket/{name}/created
	//   bucket/{name}/deleted
	//   bucket/{name}/object/put
	//   bucket/{name}/object/deleted
	Publish func(topic string, payload []byte)
}

// New creates a Handler and attempts to extract a *LocalFS for presign
// verification. Verification is silently disabled when the active backend
// is not localfs.
func New(b buckets.Backend, store storage.Provider) *Handler {
	h := &Handler{Backend: b, Store: store}
	if lf, ok := b.(*buckets.LocalFS); ok {
		h.LocalFS = lf
	}
	return h
}

// RegisterRoutes mounts the bucket API onto the given router.
func (h *Handler) RegisterRoutes(r *gin.Engine) {
	g := r.Group("/storage")
	g.GET("", h.requireUser(), h.listBuckets)
	g.POST("/:bucket", h.requireUser(), h.ensureBucket)
	g.DELETE("/:bucket", h.requireUser(), h.deleteBucket)
	g.GET("/:bucket", h.requireUser(), h.listObjects)
	g.POST("/:bucket/presign", h.requireUser(), h.presign)

	// Object endpoints accept presigned URLs (no auth) or fall back to
	// regular user auth.
	g.PUT("/:bucket/*path", h.maybePresignThenAuth("PUT"), h.putObject)
	g.HEAD("/:bucket/*path", h.maybePresignThenAuth("GET"), h.statObject)
	g.GET("/:bucket/*path", h.maybePresignThenAuth("GET"), h.getObject)
	g.DELETE("/:bucket/*path", h.requireUser(), h.deleteObject)
}

// ---------- middleware ----------

func (h *Handler) requireUser() gin.HandlerFunc {
	return auth.UserAuthMiddleware(h.Store)
}

// maybePresignThenAuth lets a valid HMAC-signed URL bypass the auth
// middleware. If the URL has no signature (or it fails to verify), the
// request falls through to the standard user auth middleware.
func (h *Handler) maybePresignThenAuth(method string) gin.HandlerFunc {
	user := auth.UserAuthMiddleware(h.Store)
	return func(c *gin.Context) {
		sig := c.Query("sig")
		exp := c.Query("expires")
		if sig != "" && exp != "" && h.LocalFS != nil {
			bucket := c.Param("bucket")
			path := strings.TrimPrefix(c.Param("path"), "/")
			m := strings.ToUpper(c.DefaultQuery("method", method))
			if h.LocalFS.VerifyPresignedURL(m, bucket, path, exp, sig) {
				c.Next()
				return
			}
		}
		user(c)
	}
}

// ---------- bucket-level handlers ----------

func (h *Handler) listBuckets(c *gin.Context) {
	names, err := h.Backend.ListBuckets(c.Request.Context())
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	c.JSON(http.StatusOK, gin.H{"buckets": names})
}

func (h *Handler) ensureBucket(c *gin.Context) {
	if err := h.Backend.EnsureBucket(c.Request.Context(), c.Param("bucket")); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	h.emit("bucket/"+c.Param("bucket")+"/created", nil)
	c.JSON(http.StatusCreated, gin.H{"bucket": c.Param("bucket"), "ok": true})
}

func (h *Handler) deleteBucket(c *gin.Context) {
	if err := h.Backend.DeleteBucket(c.Request.Context(), c.Param("bucket")); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	h.emit("bucket/"+c.Param("bucket")+"/deleted", nil)
	c.Status(http.StatusNoContent)
}

func (h *Handler) listObjects(c *gin.Context) {
	limit := 0
	if v := c.Query("limit"); v != "" {
		if n, err := strconv.Atoi(v); err == nil && n > 0 {
			limit = n
		}
	}
	objs, err := h.Backend.List(c.Request.Context(), c.Param("bucket"), c.Query("prefix"), limit)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	c.JSON(http.StatusOK, gin.H{"objects": objs, "count": len(objs)})
}

// ---------- object-level handlers ----------

func (h *Handler) putObject(c *gin.Context) {
	bucket := c.Param("bucket")
	path := strings.TrimPrefix(c.Param("path"), "/")
	owner, _ := c.Get("user_id")
	ownerID, _ := owner.(string)
	defer c.Request.Body.Close()
	obj, err := h.Backend.Put(c.Request.Context(), bucket, path, c.Request.Body, buckets.PutOptions{
		ContentType: c.ContentType(),
		OwnerID:     ownerID,
	})
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	h.emit("bucket/"+bucket+"/object/put", []byte(path))
	c.JSON(http.StatusCreated, obj)
}

func (h *Handler) getObject(c *gin.Context) {
	bucket := c.Param("bucket")
	path := strings.TrimPrefix(c.Param("path"), "/")
	rc, obj, err := h.Backend.Get(c.Request.Context(), bucket, path)
	if err != nil {
		if errors.Is(err, buckets.ErrNotFound) {
			c.JSON(http.StatusNotFound, gin.H{"error": "not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	defer rc.Close()
	if obj.ContentType != "" {
		c.Header("Content-Type", obj.ContentType)
	}
	if obj.ETag != "" {
		c.Header("ETag", obj.ETag)
	}
	c.Header("Last-Modified", obj.LastModified.UTC().Format(http.TimeFormat))
	if obj.Size > 0 {
		c.Header("Content-Length", strconv.FormatInt(obj.Size, 10))
	}
	c.Status(http.StatusOK)
	_, _ = io.Copy(c.Writer, rc)
}

func (h *Handler) statObject(c *gin.Context) {
	obj, err := h.Backend.Stat(c.Request.Context(), c.Param("bucket"), strings.TrimPrefix(c.Param("path"), "/"))
	if err != nil {
		if errors.Is(err, buckets.ErrNotFound) {
			c.Status(http.StatusNotFound)
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	if obj.ContentType != "" {
		c.Header("Content-Type", obj.ContentType)
	}
	if obj.ETag != "" {
		c.Header("ETag", obj.ETag)
	}
	c.Header("Content-Length", strconv.FormatInt(obj.Size, 10))
	c.Status(http.StatusOK)
}

func (h *Handler) deleteObject(c *gin.Context) {
	if err := h.Backend.Delete(c.Request.Context(), c.Param("bucket"), strings.TrimPrefix(c.Param("path"), "/")); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	h.emit("bucket/"+c.Param("bucket")+"/object/deleted", []byte(strings.TrimPrefix(c.Param("path"), "/")))
	c.Status(http.StatusNoContent)
}

// emit publishes a bucket lifecycle event when a publisher is configured.
func (h *Handler) emit(topic string, payload []byte) {
	if h.Publish != nil {
		h.Publish(topic, payload)
	}
}

type presignReq struct {
	Path        string `json:"path" binding:"required"`
	Method      string `json:"method"`
	ExpiresSecs int    `json:"expires_secs"`
}

func (h *Handler) presign(c *gin.Context) {
	var req presignReq
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	exp := time.Duration(req.ExpiresSecs) * time.Second
	urlStr, err := h.Backend.Presign(c.Request.Context(), c.Param("bucket"), req.Path, buckets.PresignOptions{
		Method:  req.Method,
		Expires: exp,
	})
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	c.JSON(http.StatusOK, gin.H{"url": urlStr, "expires_secs": int(exp.Seconds())})
}
