// Package system provides HTTP handlers for system information.
package system

import (
	"net/http"
	"runtime"
	"time"

	"github.com/gin-gonic/gin"
)

// Handler provides system HTTP handlers.
type Handler struct {
	Version   string
	BuildDate string
}

// NewHandler creates a new system handler.
func NewHandler(version, buildDate string) *Handler {
	return &Handler{
		Version:   version,
		BuildDate: buildDate,
	}
}

// RegisterRoutes registers system routes.
func (h *Handler) RegisterRoutes(r *gin.RouterGroup) {
	r.GET("/info", h.GetSystemInfo)
	r.GET("/time", h.GetSystemTime)
	r.GET("/ip", h.GetSystemIP)
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
