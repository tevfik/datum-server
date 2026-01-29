// Package system provides HTTP handlers for system information.
package system

import (
	"datum-go/internal/storage"
	"net/http"
	"runtime"
	"time"

	"github.com/gin-gonic/gin"
)

// Handler provides system HTTP handlers.
type Handler struct {
	Version   string
	BuildDate string
	Store     storage.Provider
}

// NewHandler creates a new system handler.
func NewHandler(version, buildDate string, store storage.Provider) *Handler {
	return &Handler{
		Version:   version,
		BuildDate: buildDate,
		Store:     store,
	}
}

// RegisterRoutes registers system routes.
func (h *Handler) RegisterRoutes(r *gin.RouterGroup) {
	r.GET("/info", h.GetSystemInfo)
	r.GET("/time", h.GetSystemTime)
	r.GET("/ip", h.GetSystemIP)
}

// RegisterAdminRoutes registers admin system routes.
func (h *Handler) RegisterAdminRoutes(r *gin.RouterGroup) {
	r.GET("/config", h.GetSystemConfig)
	r.PUT("/retention", h.UpdateRetention)
	r.PUT("/registration", h.UpdateRegistration)
	r.GET("/logs", h.GetLogs)
	r.DELETE("/logs", h.ClearLogs)
}

// GetSystemIP returns the client's public IP.
// GET /sys/ip
func (h *Handler) GetSystemIP(c *gin.Context) {
	c.String(http.StatusOK, c.ClientIP())
}

// GetSystemInfo returns version and build information.
// GET /sys/info
func (h *Handler) GetSystemInfo(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"version":    h.Version,
		"build_date": h.BuildDate,
		"go_version": runtime.Version(),
		"os":         runtime.GOOS,
		"arch":       runtime.GOARCH,
	})
}

// GetSystemTime returns the current server time.
// GET /sys/time
func (h *Handler) GetSystemTime(c *gin.Context) {
	now := time.Now()

	c.JSON(http.StatusOK, gin.H{
		"unix":      now.Unix(),
		"unix_ms":   now.UnixMilli(),
		"iso8601":   now.Format(time.RFC3339Nano),
		"timestamp": now.UnixNano(),
	})
}

// GetSystemConfig returns system configuration.
// GET /admin/sys/config
func (h *Handler) GetSystemConfig(c *gin.Context) {
	if h.Store == nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Storage not initialized"})
		return
	}

	config, err := h.Store.GetSystemConfig()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to get system config"})
		return
	}
	c.JSON(http.StatusOK, config)
}

// UpdateRetention updates data retention policy.
// PUT /admin/sys/retention
func (h *Handler) UpdateRetention(c *gin.Context) {
	var req struct {
		Days               int `json:"days" binding:"required,min=1"`
		CheckIntervalHours int `json:"check_interval_hours" binding:"required,min=1"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if err := h.Store.UpdateRetentionPolicy(req.Days, req.CheckIntervalHours); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update retention policy"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "updated"})
}

// UpdateRegistration updates user registration settings.
// PUT /admin/sys/registration
func (h *Handler) UpdateRegistration(c *gin.Context) {
	var req struct {
		AllowRegister bool `json:"allow_register"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if err := h.Store.UpdateRegistrationConfig(req.AllowRegister); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update registration config"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "updated"})
}

// GetLogs returns system logs.
// GET /admin/sys/logs
func (h *Handler) GetLogs(c *gin.Context) {
	logs, err := h.Store.GetSystemLogs(500) // Default limit
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to get logs"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"logs": logs})
}

// ClearLogs clears system logs.
// DELETE /admin/sys/logs
func (h *Handler) ClearLogs(c *gin.Context) {
	if err := h.Store.ClearSystemLogs(); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to clear logs"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"status": "cleared"})
}
