package main

import (
	"net/http"
	"runtime"

	"github.com/gin-gonic/gin"
)

// SystemInfoHandler handles system information requests
type SystemInfoHandler struct{}

// NewSystemInfoHandler creates a new system info handler
func NewSystemInfoHandler() *SystemInfoHandler {
	return &SystemInfoHandler{}
}

// RegisterRoutes registers system routes
func (h *SystemInfoHandler) RegisterRoutes(r *gin.Engine) {
	sysGroup := r.Group("/sys")
	{
		sysGroup.GET("/info", h.getSystemInfo)
	}
}

// getSystemInfo returns version and build information
// @Summary Get system information
// @Description Returns server version, build date, and runtime info
// @Tags System
// @Produce json
// @Success 200 {object} map[string]interface{}
// @Router /sys/info [get]
func (h *SystemInfoHandler) getSystemInfo(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"version":    Version,
		"build_date": BuildDate,
		"go_version": runtime.Version(),
		"os":         runtime.GOOS,
		"arch":       runtime.GOARCH,
	})
}
