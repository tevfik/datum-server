// Package public provides HTTP handlers for public endpoints (no authentication).
package public

import (
	"fmt"
	"net/http"
	"time"

	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// Handler provides public HTTP handlers.
type Handler struct {
	Store storage.Provider
}

// NewHandler creates a new public handler with dependencies.
func NewHandler(store storage.Provider) *Handler {
	return &Handler{
		Store: store,
	}
}

// RegisterRoutes registers public routes.
func (h *Handler) RegisterRoutes(r *gin.RouterGroup) {
	r.POST("/:device_id", h.PostPublicData)
	r.GET("/:device_id", h.GetPublicData)
	// r.GET("/:device_id/rec", ...) // Deprecated
}

// PostPublicData handles public data submission.
// POST /pub/:device_id
func (h *Handler) PostPublicData(c *gin.Context) {
	deviceID := c.Param("device_id")

	// Parse data
	var data map[string]interface{}
	if err := c.ShouldBindJSON(&data); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Store data (public namespace)
	point := &storage.DataPoint{
		DeviceID:  "public_" + deviceID, // Prefix to separate from authenticated devices
		Timestamp: time.Now(),
		Data:      data,
	}

	if err := h.Store.StoreData(point); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":    "ok",
		"timestamp": point.Timestamp.Format(time.RFC3339),
		"mode":      "public",
	})
}

// GetPublicData handles public data retrieval.
// GET /pub/:device_id
func (h *Handler) GetPublicData(c *gin.Context) {
	deviceID := "public_" + c.Param("device_id")

	// Get system config for public retention
	config, err := h.Store.GetSystemConfig()
	retentionDays := 1 // Default
	if err == nil && config != nil && config.PublicDataRetention > 0 {
		retentionDays = config.PublicDataRetention
	}
	cutoffTime := time.Now().Add(-time.Duration(retentionDays) * 24 * time.Hour)

	// Check for history params
	hasHistoryParams := false
	for _, param := range []string{"limit"} {
		if c.Query(param) != "" {
			hasHistoryParams = true
			break
		}
	}

	// === LATEST DATA ===
	if !hasHistoryParams {
		point, err := h.Store.GetLatestData(deviceID)
		if err != nil {
			c.JSON(http.StatusNotFound, gin.H{"error": "No data found"})
			return
		}

		// Enforce Soft Retention
		if point.Timestamp.Before(cutoffTime) {
			c.JSON(http.StatusNotFound, gin.H{"error": "No data found (expired)"})
			return
		}

		c.JSON(http.StatusOK, gin.H{
			"device_id": c.Param("device_id"),
			"timestamp": point.Timestamp.Format(time.RFC3339),
			"data":      point.Data,
			"mode":      "public",
		})
		return
	}

	// === HISTORY ===
	limit := 100
	if limitStr := c.Query("limit"); limitStr != "" {
		fmt.Sscanf(limitStr, "%d", &limit)
	}

	points, err := h.Store.GetDataHistory(deviceID, limit)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	type DataResponse struct {
		Timestamp string                 `json:"timestamp"`
		Data      map[string]interface{} `json:"data"`
	}

	var response []DataResponse
	for _, p := range points {
		// Enforce Soft Retention
		if p.Timestamp.After(cutoffTime) {
			response = append(response, DataResponse{
				Timestamp: p.Timestamp.Format(time.RFC3339),
				Data:      p.Data,
			})
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"device_id": c.Param("device_id"),
		"data":      response,
		"mode":      "public",
	})
}
