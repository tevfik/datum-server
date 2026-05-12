package rules

import (
	"encoding/json"
	"net/http"
	"time"

	"datum-go/internal/rules"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

// Handler manages rule engine API endpoints.
type Handler struct {
	engine *rules.Engine
	store  storage.Provider
}

// NewHandler creates a new rules API handler.
func NewHandler(engine *rules.Engine, store storage.Provider) *Handler {
	return &Handler{
		engine: engine,
		store:  store,
	}
}

// RegisterRoutes registers rule management routes on the given group.
func (h *Handler) RegisterRoutes(rg *gin.RouterGroup) {
	rg.GET("", h.ListRules)
	rg.POST("", h.CreateRule)
	rg.GET("/blocks", h.GetBlockDefinitions)     // Blockly block metadata
	rg.GET("/discovery", h.DiscoverDevices)       // Device property discovery
	rg.GET("/:rule_id", h.GetRule)
	rg.PUT("/:rule_id", h.UpdateRule)
	rg.DELETE("/:rule_id", h.DeleteRule)
	rg.PUT("/:rule_id/enable", h.EnableRule)
	rg.PUT("/:rule_id/disable", h.DisableRule)
	rg.POST("/:rule_id/trigger", h.TriggerRule)   // Manual trigger
}

func (h *Handler) ListRules(c *gin.Context) {
	userID := c.GetString("user_id")
	if userID == "" {
		// Admin mode: return all rules from engine
		c.JSON(http.StatusOK, gin.H{"rules": h.engine.ListRules()})
		return
	}

	docs, err := h.store.ListDocuments(userID, "rules")
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to fetch rules"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"rules": docs})
}

func (h *Handler) CreateRule(c *gin.Context) {
	userID := c.GetString("user_id")
	var r rules.Rule
	if err := c.ShouldBindJSON(&r); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if r.ID == "" {
		r.ID = "rule_" + uuid.New().String()[:8]
	}
	r.OwnerID = userID
	r.CreatedAt = time.Now()

	// 1. Verify device ownership (check both legacy DeviceID and Trigger.DeviceID)
	effDeviceID := r.DeviceID
	if r.Trigger.DeviceID != "" {
		effDeviceID = r.Trigger.DeviceID
	}
	if effDeviceID != "" && userID != "" && h.store != nil {
		dev, err := h.store.GetDevice(effDeviceID)
		if err != nil || dev.UserID != userID {
			c.JSON(http.StatusForbidden, gin.H{"error": "You do not own this device"})
			return
		}
	}

	// 2. Persist to storage
	if userID != "" && h.store != nil {
		doc := make(map[string]interface{})
		data, _ := json.Marshal(r)
		json.Unmarshal(data, &doc)
		if _, err := h.store.CreateDocument(userID, "rules", doc); err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to save rule"})
			return
		}
	}

	// 3. Add to engine
	h.engine.AddRule(&r)
	c.JSON(http.StatusCreated, r)
}

func (h *Handler) GetRule(c *gin.Context) {
	id := c.Param("rule_id")
	r, ok := h.engine.GetRule(id)
	if !ok {
		c.JSON(http.StatusNotFound, gin.H{"error": "rule not found"})
		return
	}
	c.JSON(http.StatusOK, r)
}

func (h *Handler) UpdateRule(c *gin.Context) {
	userID := c.GetString("user_id")
	id := c.Param("rule_id")

	var r rules.Rule
	if err := c.ShouldBindJSON(&r); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	r.ID = id
	r.OwnerID = userID

	// 1. Ownership check (if user mode)
	if userID != "" {
		existing, err := h.store.GetDocument(userID, "rules", id)
		if err != nil {
			c.JSON(http.StatusNotFound, gin.H{"error": "rule not found or access denied"})
			return
		}
		if existing["_owner_id"] != userID {
			c.JSON(http.StatusForbidden, gin.H{"error": "access denied"})
			return
		}

		// 2. Persist
		doc := make(map[string]interface{})
		data, _ := json.Marshal(r)
		json.Unmarshal(data, &doc)
		if err := h.store.UpdateDocument(userID, "rules", id, doc); err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update rule storage"})
			return
		}
	}

	// 3. Update engine
	h.engine.AddRule(&r)
	c.JSON(http.StatusOK, r)
}

func (h *Handler) DeleteRule(c *gin.Context) {
	userID := c.GetString("user_id")
	id := c.Param("rule_id")

	if userID != "" {
		if err := h.store.DeleteDocument(userID, "rules", id); err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to delete rule from storage"})
			return
		}
	}

	h.engine.RemoveRule(id)
	c.JSON(http.StatusOK, gin.H{"status": "deleted"})
}

func (h *Handler) EnableRule(c *gin.Context) {
	userID := c.GetString("user_id")
	id := c.Param("rule_id")

	r, ok := h.engine.GetRule(id)
	if !ok {
		c.JSON(http.StatusNotFound, gin.H{"error": "rule not found"})
		return
	}

	if userID != "" && r.OwnerID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "access denied"})
		return
	}

	r.Enabled = true
	if userID != "" {
		h.store.UpdateDocument(userID, "rules", id, map[string]interface{}{"enabled": true})
	}

	c.JSON(http.StatusOK, r)
}

func (h *Handler) DisableRule(c *gin.Context) {
	userID := c.GetString("user_id")
	id := c.Param("rule_id")

	r, ok := h.engine.GetRule(id)
	if !ok {
		c.JSON(http.StatusNotFound, gin.H{"error": "rule not found"})
		return
	}

	if userID != "" && r.OwnerID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "access denied"})
		return
	}

	r.Enabled = false
	if userID != "" {
		h.store.UpdateDocument(userID, "rules", id, map[string]interface{}{"enabled": false})
	}

	c.JSON(http.StatusOK, r)
}

// TriggerRule manually triggers a specific rule.
// POST /api/v1/rules/:rule_id/trigger
func (h *Handler) TriggerRule(c *gin.Context) {
	userID := c.GetString("user_id")
	id := c.Param("rule_id")

	r, ok := h.engine.GetRule(id)
	if !ok {
		c.JSON(http.StatusNotFound, gin.H{"error": "rule not found"})
		return
	}

	// Ownership check
	if userID != "" && r.OwnerID != "" && r.OwnerID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "access denied"})
		return
	}

	deviceID := r.DeviceID
	if r.Trigger.DeviceID != "" {
		deviceID = r.Trigger.DeviceID
	}

	if deviceID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "rule has no device_id to trigger against"})
		return
	}

	// Fetch latest data for the device
	if h.store == nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "storage not available"})
		return
	}

	latest, err := h.store.GetLatestData(deviceID)
	if err != nil || latest == nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "no data available for device"})
		return
	}

	fired := h.engine.EvaluateManual(id, deviceID, latest.Data)
	c.JSON(http.StatusOK, gin.H{
		"triggered": true,
		"fired":     fired,
		"device_id": deviceID,
	})
}
