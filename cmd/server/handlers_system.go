package main

import (
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
)

// getSystemTimeHandler returns the current server time
// GET /sys/time
// Used for simple NTP-like sync via HTTP (Christian's Algorithm support)
func getSystemTimeHandler(c *gin.Context) {
	now := time.Now()

	c.JSON(http.StatusOK, gin.H{
		"unix":      now.Unix(),
		"unix_ms":   now.UnixMilli(),
		"iso8601":   now.Format(time.RFC3339Nano),
		"timestamp": now.UnixNano(),
	})
}
