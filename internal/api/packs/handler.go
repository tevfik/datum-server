// Package packsapi is datum-server's gateway in front of gleann's
// /api/packs/* surface.
//
// Why a gateway and not direct access? Three reasons:
//
//  1. **Auth**: gleann is an internal service with no notion of users.
//     datum-server adds the JWT/ak_ middleware before forwarding.
//  2. **Quota / plan**: per-user pack access can be metered (premium packs
//     in the future) without modifying gleann.
//  3. **Cache**: a small in-memory cache (with ETag round-trip) shields
//     gleann from request stampedes; clients still benefit from HTTP
//     caching headers.
//
// Routes registered (all require user auth via UserAuthMiddleware):
//
//	GET /packs                          → list manifests
//	GET /packs/:id                      → single manifest
//	GET /packs/:id/data                 → full pack body
//	GET /packs/:id/items/:slug          → single item
//	GET /packs/:id/search?q=...&n=...   → substring search
package packsapi

import (
	"io"
	"net/http"
	"net/url"
	"os"
	"strings"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
)

// DefaultGleannURL is used when GLEANN_URL is unset.
const DefaultGleannURL = "http://localhost:8080"

// Handler implements the gateway.
type Handler struct {
	gleannURL string
	client    *http.Client
	cache     *cache
}

// New builds a Handler reading the upstream URL from GLEANN_URL.
// The upstream value is locked at construction time; restart datum-server
// to pick up changes.
func New() *Handler {
	base := strings.TrimRight(os.Getenv("GLEANN_URL"), "/")
	if base == "" {
		base = DefaultGleannURL
	}
	return &Handler{
		gleannURL: base,
		client:    &http.Client{Timeout: 10 * time.Second},
		cache:     newCache(5 * time.Minute),
	}
}

// RegisterRoutes mounts the gateway routes on the given group. The group
// is expected to already have the user-auth middleware applied.
func (h *Handler) RegisterRoutes(g *gin.RouterGroup) {
	g.GET("/packs", h.list)
	g.GET("/packs/:id", h.proxy("/api/packs/:id"))
	g.GET("/packs/:id/data", h.proxy("/api/packs/:id/data"))
	g.GET("/packs/:id/items/:slug", h.proxy("/api/packs/:id/items/:slug"))
	g.GET("/packs/:id/search", h.proxy("/api/packs/:id/search"))
}

func (h *Handler) list(c *gin.Context) {
	target := h.gleannURL + "/api/packs"
	if app := c.Query("app"); app != "" {
		target += "?app=" + url.QueryEscape(app)
	}
	h.forward(c, target, c.Request.URL.RawQuery)
}

// proxy returns a Gin handler that substitutes :id (and :slug) into the
// gleann URL template and forwards the request.
func (h *Handler) proxy(template string) gin.HandlerFunc {
	return func(c *gin.Context) {
		path := template
		path = strings.ReplaceAll(path, ":id", url.PathEscape(c.Param("id")))
		if slug := c.Param("slug"); slug != "" {
			path = strings.ReplaceAll(path, ":slug", url.PathEscape(slug))
		}
		target := h.gleannURL + path
		h.forward(c, target, c.Request.URL.RawQuery)
	}
}

// forward issues the upstream request, copies the response, and applies
// in-memory caching keyed by full target URL+query.
func (h *Handler) forward(c *gin.Context, target, rawQuery string) {
	if rawQuery != "" && !strings.Contains(target, "?") {
		target += "?" + rawQuery
	}
	cacheKey := target
	ifNoneMatch := c.GetHeader("If-None-Match")

	// Serve from cache when possible.
	if entry, ok := h.cache.get(cacheKey); ok {
		if ifNoneMatch != "" && ifNoneMatch == entry.etag {
			c.Status(http.StatusNotModified)
			return
		}
		writeEntry(c, entry)
		return
	}

	req, err := http.NewRequestWithContext(c.Request.Context(), http.MethodGet, target, nil)
	if err != nil {
		c.JSON(http.StatusBadGateway, gin.H{"error": err.Error()})
		return
	}
	if ifNoneMatch != "" {
		req.Header.Set("If-None-Match", ifNoneMatch)
	}
	res, err := h.client.Do(req)
	if err != nil {
		c.JSON(http.StatusBadGateway, gin.H{"error": "gleann unreachable: " + err.Error()})
		return
	}
	defer res.Body.Close()

	if res.StatusCode == http.StatusNotModified {
		c.Status(http.StatusNotModified)
		return
	}
	body, err := io.ReadAll(res.Body)
	if err != nil {
		c.JSON(http.StatusBadGateway, gin.H{"error": err.Error()})
		return
	}
	entry := &cacheEntry{
		status:      res.StatusCode,
		body:        body,
		etag:        res.Header.Get("ETag"),
		contentType: res.Header.Get("Content-Type"),
	}
	if res.StatusCode == http.StatusOK && entry.etag != "" {
		h.cache.set(cacheKey, entry)
	}
	writeEntry(c, entry)
}

func writeEntry(c *gin.Context, e *cacheEntry) {
	if e.contentType != "" {
		c.Header("Content-Type", e.contentType)
	}
	if e.etag != "" {
		c.Header("ETag", e.etag)
		c.Header("Cache-Control", "private, max-age=300")
	}
	c.Status(e.status)
	_, _ = c.Writer.Write(e.body)
}

// ── cache ─────────────────────────────────────────────────────────────────

type cacheEntry struct {
	status      int
	body        []byte
	etag        string
	contentType string
	expiresAt   time.Time
}

type cache struct {
	ttl time.Duration
	mu  sync.RWMutex
	m   map[string]*cacheEntry
}

func newCache(ttl time.Duration) *cache {
	return &cache{ttl: ttl, m: map[string]*cacheEntry{}}
}

func (c *cache) get(k string) (*cacheEntry, bool) {
	c.mu.RLock()
	defer c.mu.RUnlock()
	e, ok := c.m[k]
	if !ok || time.Now().After(e.expiresAt) {
		return nil, false
	}
	return e, true
}

func (c *cache) set(k string, e *cacheEntry) {
	e.expiresAt = time.Now().Add(c.ttl)
	c.mu.Lock()
	c.m[k] = e
	c.mu.Unlock()
}
