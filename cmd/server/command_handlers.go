package main

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"net/http"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/logger"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// Command handlers

type SendCommandRequest struct {
	Action    string                 `json:"action" binding:"required"`
	Params    map[string]interface{} `json:"params"`
	ExpiresIn int                    `json:"expires_in"` // Duration in seconds
}

func sendCommandHandler(c *gin.Context) {
	deviceID := c.Param("device_id")
	userID, _ := auth.GetUserID(c)

	// Verify device ownership
	device, err := store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}
	if device.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Access denied"})
		return
	}

	var req SendCommandRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Calculate expiration
	expiresAt := time.Now().Add(24 * time.Hour) // Default 24h
	if req.ExpiresIn > 0 {
		expiresAt = time.Now().Add(time.Duration(req.ExpiresIn) * time.Second)
	}

	// Create command
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

	if err := store.CreateCommand(cmd); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	// SMART ROUTING: Check if device is connected via MQTT
	// default status
	status := "pending"
	message := "Command queued for device (offline)"

	isConnected := false
	if mqttBroker != nil {
		isConnected = mqttBroker.IsDeviceConnected(deviceID)
		// Debug log
		logger.GetLogger().Info().Str("device_id", deviceID).Bool("connected", isConnected).Msg("Checking MQTT connection for command delivery")
	}

	if isConnected {
		// Construct payload
		payload := map[string]interface{}{
			"command_id": cmdID,
			"action":     cmd.Action,
			"params":     cmd.Params,
			"timestamp":  cmd.CreatedAt.Unix(),
		}

		// Marshal JSON
		if jsonBytes, err := json.Marshal(payload); err == nil {
			if err := mqttBroker.PublishCommand(deviceID, jsonBytes); err == nil {
				status = "sent"
				message = "Command sent to device via MQTT"
				logger.GetLogger().Info().Str("command_id", cmdID).Msg("Command successfully published to MQTT")
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

func listCommandsHandler(c *gin.Context) {
	deviceID := c.Param("device_id")
	userID, _ := auth.GetUserID(c)

	// Verify device ownership
	device, err := store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}
	if device.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Access denied"})
		return
	}

	commands, _ := store.GetPendingCommands(deviceID)
	c.JSON(http.StatusOK, gin.H{"commands": commands})
}

func pollCommandsHandler(c *gin.Context) {
	deviceID := c.Param("device_id")
	apiKey, _ := c.Get("api_key")

	// Verify device
	device, err := store.GetDeviceByAPIKey(apiKey.(string))
	if err != nil || device.ID != deviceID {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid device credentials"})
		return
	}

	commands, _ := store.GetPendingCommands(deviceID)

	// Mark commands as delivered
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

type AckCommandRequest struct {
	Status string                 `json:"status" binding:"required"`
	Result map[string]interface{} `json:"result"`
}

func ackCommandHandler(c *gin.Context) {
	deviceID := c.Param("device_id")
	commandID := c.Param("command_id")
	apiKey, _ := c.Get("api_key")

	// Verify device
	device, err := store.GetDeviceByAPIKey(apiKey.(string))
	if err != nil || device.ID != deviceID {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid device credentials"})
		return
	}

	var req AckCommandRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if err := store.AcknowledgeCommand(commandID, req.Result); err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":  "acknowledged",
		"message": "Command execution confirmed",
	})
}

func generateCommandID() string {
	bytes := make([]byte, 8)
	rand.Read(bytes)
	return "cmd_" + hex.EncodeToString(bytes)
}
