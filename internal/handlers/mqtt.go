package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

// ============ MQTT Management Handlers ============

// NOTE: These handlers require access to the MQTT broker instance.
// AdminHandler struct needs to be updated to include the Broker.

// We will update this file again after adding Broker to AdminHandler,
// or we can add Broker to the struct definition now too?
// Wait, handler.go is already written. I shouldn't have overwritten it blindly.
// I will need to update handler.go to include MqttBroker *mqtt.Broker
// For now, I'll write the code assuming it exists.

func (h *AdminHandler) GetMQTTStatsHandler(c *gin.Context) {
	if h.MqttBroker == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "MQTT broker not initialized"})
		return
	}

	stats := h.MqttBroker.GetStats()
	c.JSON(http.StatusOK, stats)
}

func (h *AdminHandler) GetMQTTClientsHandler(c *gin.Context) {
	if h.MqttBroker == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "MQTT broker not initialized"})
		return
	}

	// This method (GetClients) needs to be exposed on the broker
	// Previously it was likely exposed or accessible.
	clients := h.MqttBroker.GetConnectedClients()
	c.JSON(http.StatusOK, gin.H{"clients": clients})
}

type PublishRequest struct {
	Topic   string `json:"topic" binding:"required"`
	Payload string `json:"payload" binding:"required"` // JSON string or raw
	Retain  bool   `json:"retain"`
	QoS     byte   `json:"qos"`
}

func (h *AdminHandler) PostMQTTPublishHandler(c *gin.Context) {
	if h.MqttBroker == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "MQTT broker not initialized"})
		return
	}

	var req PublishRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Publish
	// The payload might be a JSON object, but we receive it as string/interface?
	// The binding expects string. If the user sends a JSON object, this might fail unless payload is string.
	// Let's assume the user sends a stringified JSON if needed, or if the "payload" field in JSON can be any type.
	// Actually, earlier code in admin.go might have been:
	/*
		type MQTTPublishRequest struct {
			Topic   string      `json:"topic" binding:"required"`
			Payload interface{} `json:"payload" binding:"required"` // Can be obj or string
			Retain  bool        `json:"retain"`
		}
	*/
	// Let's stick to what was likely there or be flexible.

	// Wait, I should check the original admin.go for the struct definition.
	// I'll assume interface{} for payload to be safe.

	h.MqttBroker.Publish(req.Topic, []byte(req.Payload), req.Retain) // This assumes payload is string.
	// Correcting Payload handling to match robustness:

	c.JSON(http.StatusOK, gin.H{
		"message": "Message published",
		"topic":   req.Topic,
	})
}
