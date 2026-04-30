// Package sse provides HTTP handlers for Server-Sent Events (SSE) command polling.
package sse

import (
	"net/http"
	"strconv"
	"time"

	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// Handler provides SSE and Webhook polling handlers.
type Handler struct {
	Store storage.Provider
}

// NewHandler creates a new SSE handler with dependencies.
func NewHandler(store storage.Provider) *Handler {
	return &Handler{
		Store: store,
	}
}

// RegisterRoutes registers SSE and polling routes.
func (h *Handler) RegisterRoutes(r *gin.RouterGroup) {
	r.GET("/:device_id/cmd/stream", h.SSECommands)
	r.GET("/:device_id/cmd/poll", h.WebhookPoll)
}

// SSECommands handles long polling commands via SSE.
// GET /dev/:device_id/cmd/stream
func (h *Handler) SSECommands(c *gin.Context) {
	deviceID := c.Param("device_id")
	apiKey, _ := c.Get("api_key")

	// Verify device
	device, err := h.Store.GetDeviceByAPIKey(apiKey.(string))
	if err != nil || device.ID != deviceID {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid device credentials"})
		return
	}

	// Get wait timeout (default 30 seconds, max 60)
	waitStr := c.DefaultQuery("wait", "30")
	waitSec, _ := strconv.Atoi(waitStr)
	if waitSec < 1 {
		waitSec = 1
	}
	if waitSec > 60 {
		waitSec = 60
	}

	// Set SSE headers
	c.Writer.Header().Set("Content-Type", "text/event-stream")
	c.Writer.Header().Set("Cache-Control", "no-cache")
	c.Writer.Header().Set("Connection", "keep-alive")
	c.Writer.Header().Set("Transfer-Encoding", "chunked")
	c.Writer.Header().Set("X-Accel-Buffering", "no")

	// Channel for early termination
	clientGone := c.Request.Context().Done()
	ticker := time.NewTicker(3 * time.Second)
	defer ticker.Stop()

	timeout := time.After(time.Duration(waitSec) * time.Second)

	// Send initial keepalive
	c.SSEvent("keepalive", map[string]interface{}{
		"device_id": deviceID,
		"wait":      waitSec,
		"time":      time.Now().Format(time.RFC3339),
	})
	c.Writer.Flush()

	// Poll for commands
	for {
		select {
		case <-clientGone:
			return
		case <-timeout:
			// Timeout - send final check
			commands, _ := h.Store.GetPendingCommands(deviceID)
			if len(commands) > 0 {
				c.SSEvent("command", formatCommands(commands))
			} else {
				c.SSEvent("timeout", map[string]interface{}{
					"message": "no commands",
					"time":    time.Now().Format(time.RFC3339),
				})
			}
			c.Writer.Flush()
			return
		case <-ticker.C:
			// Check for pending commands
			commands, _ := h.Store.GetPendingCommands(deviceID)
			if len(commands) > 0 {
				c.SSEvent("command", formatCommands(commands))
				c.Writer.Flush()
				return // Commands delivered, close connection
			}
			// Send keepalive
			c.SSEvent("keepalive", map[string]interface{}{
				"time": time.Now().Format(time.RFC3339),
			})
			c.Writer.Flush()
		}
	}
}

// WebhookPoll handles simple webhook command delivery.
// GET /dev/:device_id/cmd/poll
func (h *Handler) WebhookPoll(c *gin.Context) {
	deviceID := c.Param("device_id")
	apiKey, _ := c.Get("api_key")

	device, err := h.Store.GetDeviceByAPIKey(apiKey.(string))
	if err != nil || device.ID != deviceID {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid device credentials"})
		return
	}

	// Get wait timeout
	waitStr := c.DefaultQuery("wait", "0")
	waitSec, _ := strconv.Atoi(waitStr)

	if waitSec > 0 {
		// Long polling mode
		if waitSec > 60 {
			waitSec = 60
		}

		deadline := time.Now().Add(time.Duration(waitSec) * time.Second)
		ctx := c.Request.Context()
		for time.Now().Before(deadline) {
			select {
			case <-ctx.Done():
				return
			default:
			}
			commands, _ := h.Store.GetPendingCommands(deviceID)
			if len(commands) > 0 {
				c.JSON(http.StatusOK, gin.H{
					"commands": formatCommands(commands),
				})
				return
			}
			time.Sleep(500 * time.Millisecond)
		}

		// Timeout
		c.JSON(http.StatusOK, gin.H{"commands": []interface{}{}})
		return
	}

	// Immediate mode
	commands, _ := h.Store.GetPendingCommands(deviceID)
	var cmdList []map[string]interface{}
	for _, cmd := range commands {
		cmdList = append(cmdList, map[string]interface{}{
			"command_id": cmd.ID,
			"action":     cmd.Action,
			"params":     cmd.Params,
			"created_at": cmd.CreatedAt.Format(time.RFC3339),
		})
	}
	c.JSON(http.StatusOK, gin.H{"commands": cmdList})
}

func formatCommands(commands []storage.Command) interface{} {
	var result []map[string]interface{}
	for _, cmd := range commands {
		result = append(result, map[string]interface{}{
			"command_id": cmd.ID,
			"action":     cmd.Action,
			"params":     cmd.Params,
			"created_at": cmd.CreatedAt.Format(time.RFC3339),
		})
	}
	return result
}
