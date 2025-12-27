package main

import (
	"datum-go/internal/storage"
	"net/http"
	"strconv"
	"time"

	"github.com/gin-gonic/gin"
)

// SSE handler for long polling commands
// Device connects and waits for commands (battery-optimized)
func sseCommandsHandler(c *gin.Context) {
	deviceID := c.Param("device_id")
	apiKey, _ := c.Get("api_key")

	// Verify device
	device, err := store.GetDeviceByAPIKey(apiKey.(string))
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
	ticker := time.NewTicker(2 * time.Second)
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
			commands, _ := store.GetPendingCommands(deviceID)
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
			commands, _ := store.GetPendingCommands(deviceID)
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

// Simple webhook for command delivery (alternative to SSE)
func webhookPollHandler(c *gin.Context) {
	deviceID := c.Param("device_id")
	apiKey, _ := c.Get("api_key")

	device, err := store.GetDeviceByAPIKey(apiKey.(string))
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
		for time.Now().Before(deadline) {
			commands, _ := store.GetPendingCommands(deviceID)
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
	commands, _ := store.GetPendingCommands(deviceID)
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
