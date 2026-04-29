// Package analytics provides a minimal client-event ingestion endpoint.
//
// Mobile / web clients POST batches of events to /analytics/events.
// Events are stored in the user's `analytics_events` document collection
// (one doc per event, partitioned by user). Admins can later aggregate
// across users via /admin/analytics/*.
//
// This is deliberately lightweight: no schemas, no aggregations, no
// background workers. We piggy-back on the existing Generic Document
// Store so there is zero new persistence code. If volume grows we can
// migrate to a dedicated time-series table without changing the client.
package analytics

import (
	"net/http"
	"time"

	"github.com/gin-gonic/gin"

	"datum-go/internal/quota"
	"datum-go/internal/storage"
)

const Collection = "analytics_events"

// Event is one client-emitted record.
type Event struct {
	Name      string                 `json:"name" binding:"required"`
	Timestamp time.Time              `json:"timestamp"`
	Props     map[string]interface{} `json:"props,omitempty"`
	Session   string                 `json:"session,omitempty"`
	Platform  string                 `json:"platform,omitempty"`
	AppVer    string                 `json:"app_version,omitempty"`
}

// Handler exposes /analytics/events ingestion.
type Handler struct {
	Store storage.Provider
	Quota *quota.Manager // optional — if nil, no enforcement
}

func New(store storage.Provider, q *quota.Manager) *Handler {
	return &Handler{Store: store, Quota: q}
}

// RegisterUserRoutes wires the ingestion route under an authenticated group.
func (h *Handler) RegisterUserRoutes(g *gin.RouterGroup) {
	g.POST("/events", h.IngestBatch)
	g.GET("/events", h.ListMine)
}

// RegisterAdminRoutes wires admin query routes.
func (h *Handler) RegisterAdminRoutes(g *gin.RouterGroup) {
	g.GET("/analytics/users/:user_id/events", h.ListForUser)
	g.GET("/analytics/summary", h.Summary)
}

// IngestBatch accepts a JSON body of either {"events":[…]} or a bare array.
func (h *Handler) IngestBatch(c *gin.Context) {
	uid, _ := c.Get("user_id")
	uidStr, _ := uid.(string)
	if uidStr == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "no user"})
		return
	}

	var body struct {
		Events []Event `json:"events"`
	}
	if err := c.ShouldBindJSON(&body); err != nil {
		// Try bare array.
		var arr []Event
		if err2 := c.ShouldBindJSON(&arr); err2 == nil {
			body.Events = arr
		} else {
			c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
			return
		}
	}
	if len(body.Events) == 0 {
		c.JSON(http.StatusBadRequest, gin.H{"error": "no events"})
		return
	}
	if len(body.Events) > 500 {
		c.JSON(http.StatusBadRequest, gin.H{"error": "max 500 events per batch"})
		return
	}

	if h.Quota != nil {
		if err := h.Quota.Check(uidStr, quota.ResourceAnalyticsEvt, int64(len(body.Events))); err != nil {
			c.JSON(http.StatusTooManyRequests, gin.H{"error": err.Error()})
			return
		}
	}

	stored := 0
	for _, e := range body.Events {
		if e.Timestamp.IsZero() {
			e.Timestamp = time.Now().UTC()
		}
		doc := map[string]interface{}{
			"name":        e.Name,
			"timestamp":   e.Timestamp.Format(time.RFC3339Nano),
			"props":       e.Props,
			"session":     e.Session,
			"platform":    e.Platform,
			"app_version": e.AppVer,
			"client_ip":   c.ClientIP(),
		}
		if _, err := h.Store.CreateDocument(uidStr, Collection, doc); err == nil {
			stored++
		}
	}

	if h.Quota != nil {
		h.Quota.Increment(uidStr, quota.ResourceAnalyticsEvt, int64(stored))
	}

	c.JSON(http.StatusAccepted, gin.H{"stored": stored, "total": len(body.Events)})
}

// ListMine returns the caller's own events (capped, newest first).
func (h *Handler) ListMine(c *gin.Context) {
	uid, _ := c.Get("user_id")
	uidStr, _ := uid.(string)
	if uidStr == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "no user"})
		return
	}
	docs, err := h.Store.ListDocuments(uidStr, Collection)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	if len(docs) > 500 {
		docs = docs[:500]
	}
	c.JSON(http.StatusOK, gin.H{"events": docs, "count": len(docs)})
}

// ListForUser is the admin version.
func (h *Handler) ListForUser(c *gin.Context) {
	uid := c.Param("user_id")
	docs, err := h.Store.ListDocuments(uid, Collection)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	c.JSON(http.StatusOK, gin.H{"user_id": uid, "events": docs, "count": len(docs)})
}

// Summary returns event counts grouped by name across all users.
// (Admin-only; uses ListAllCollections for user discovery.)
func (h *Handler) Summary(c *gin.Context) {
	all, err := h.Store.ListAllCollections()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	counts := map[string]int{}
	totalUsers := 0
	for _, ci := range all {
		if ci.Collection != Collection {
			continue
		}
		totalUsers++
		docs, err := h.Store.ListDocuments(ci.UserID, Collection)
		if err != nil {
			continue
		}
		for _, d := range docs {
			name, _ := d["name"].(string)
			if name == "" {
				name = "(unknown)"
			}
			counts[name]++
		}
	}
	c.JSON(http.StatusOK, gin.H{
		"total_users":  totalUsers,
		"event_counts": counts,
	})
}
