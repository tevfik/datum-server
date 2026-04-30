// Package commands provides HTTP handlers for device command endpoints.
package commands

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"net/http"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/logger"
	"datum-go/internal/metrics"
	"datum-go/internal/mqtt"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// Handler provides command HTTP handlers.
type Handler struct {
	Store      storage.Provider
	MQTTBroker *mqtt.Broker
}

// NewHandler creates a new command handler with dependencies.
func NewHandler(store storage.Provider, broker *mqtt.Broker) *Handler {
	return &Handler{
		Store:      store,
		MQTTBroker: broker,
	}
}

// RegisterUserRoutes registers command routes that require user auth.
func (h *Handler) RegisterUserRoutes(r *gin.RouterGroup) {
	r.POST("/:device_id/cmd", h.SendCommand)
	r.GET("/:device_id/cmd", h.ListCommands)
}

// RegisterDeviceRoutes registers command routes that require device auth.
func (h *Handler) RegisterDeviceRoutes(r *gin.RouterGroup) {
	r.GET("/:device_id/cmd/pending", h.PollCommands)
	r.POST("/:device_id/cmd/:command_id/ack", h.AckCommand)
}

// ============ Request/Response types ============

// SendCommandRequest holds command creation data.
type SendCommandRequest struct {
	Action    string                 `json:"action" binding:"required"`
	Params    map[string]interface{} `json:"params"`
	ExpiresIn int                    `json:"expires_in"`
}

// AckCommandRequest holds command acknowledgment data.
type AckCommandRequest struct {
	Status string                 `json:"status" binding:"required"`
	Result map[string]interface{} `json:"result"`
}

// ============ Handlers ============

// SendCommand sends a command to a device.
// POST /dev/:device_id/cmd
func (h *Handler) SendCommand(c *gin.Context) {
	deviceID := c.Param("device_id")
	userID, _ := auth.GetUserID(c)
	role, _ := auth.GetUserRole(c)

	device, err := h.Store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}
	if role != "admin" && device.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Access denied"})
		return
	}

	var req SendCommandRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	expiresAt := time.Now().Add(24 * time.Hour)
	if req.ExpiresIn > 0 {
		expiresAt = time.Now().Add(time.Duration(req.ExpiresIn) * time.Second)
	}

	cmdID := generateCommandID()
	cmd := &storage.Command{
		ID:        cmdID,
		DeviceID:  deviceID,
		Action:    req.Action,
		Params:    req.Params,
		Status:    "pending",
		CreatedAt: time.Now(),
		ExpiresAt: expiresAt,
	}

	if err := h.Store.CreateCommand(cmd); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	metrics.GetGlobalMetrics().IncrementCommands()

	status := "pending"
	message := "Command queued for device (offline)"

	if h.MQTTBroker != nil {
		isConnected := h.MQTTBroker.IsDeviceConnected(deviceID)
		logger.GetLogger().Info().Str("device_id", deviceID).Bool("connected", isConnected).Msg("MQTT Connection State Check")

		payload := map[string]interface{}{
			"command_id": cmdID,
			"action":     cmd.Action,
			"params":     cmd.Params,
			"timestamp":  cmd.CreatedAt.Unix(),
		}

		if jsonBytes, err := json.Marshal(payload); err == nil {
			if err := h.MQTTBroker.PublishCommand(deviceID, jsonBytes); err == nil {
				status = "sent"
				message = "Command sent to MQTT broker"
				logger.GetLogger().Info().Str("command_id", cmdID).Msg("Command published to MQTT (Optimistic)")
			} else {
				logger.GetLogger().Error().Err(err).Str("command_id", cmdID).Msg("Failed to publish command to MQTT")
			}
		}
	}

	c.JSON(http.StatusAccepted, gin.H{
		"command_id": cmdID,
		"status":     status,
		"message":    message,
		"expires_at": expiresAt,
	})
}

// ListCommands lists commands for a device.
// GET /dev/:device_id/cmd
func (h *Handler) ListCommands(c *gin.Context) {
	deviceID := c.Param("device_id")
	userID, _ := auth.GetUserID(c)

	device, err := h.Store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}
	if device.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Access denied"})
		return
	}

	commands, _ := h.Store.GetPendingCommands(deviceID)
	c.JSON(http.StatusOK, gin.H{"commands": commands})
}

// PollCommands returns pending commands for device polling.
// GET /dev/:device_id/cmd/pending
func (h *Handler) PollCommands(c *gin.Context) {
	deviceID := c.Param("device_id")
	apiKey, _ := c.Get("api_key")

	device, err := h.Store.GetDeviceByAPIKey(apiKey.(string))
	if err != nil || device.ID != deviceID {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid device credentials"})
		return
	}

	commands, _ := h.Store.GetPendingCommands(deviceID)

	var cmdList []gin.H
	for _, cmd := range commands {
		cmdList = append(cmdList, gin.H{
			"command_id": cmd.ID,
			"action":     cmd.Action,
			"params":     cmd.Params,
			"created_at": cmd.CreatedAt.Format(time.RFC3339),
		})
	}

	c.JSON(http.StatusOK, gin.H{"commands": cmdList})
}

// AckCommand acknowledges command execution.
// POST /dev/:device_id/cmd/:command_id/ack
func (h *Handler) AckCommand(c *gin.Context) {
	deviceID := c.Param("device_id")
	commandID := c.Param("command_id")
	apiKey, _ := c.Get("api_key")

	device, err := h.Store.GetDeviceByAPIKey(apiKey.(string))
	if err != nil || device.ID != deviceID {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid device credentials"})
		return
	}

	var req AckCommandRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if err := h.Store.AcknowledgeCommand(commandID, req.Result); err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":  "acknowledged",
		"message": "Command execution confirmed",
	})
}

// ============ Helper Functions ============

func generateCommandID() string {
	bytes := make([]byte, 8)
	rand.Read(bytes)
	return "cmd_" + hex.EncodeToString(bytes)
}
