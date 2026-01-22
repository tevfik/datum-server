package main

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

// getMQTTStatsHandler returns broker statistics
// GET /admin/mqtt/stats
func getMQTTStatsHandler(c *gin.Context) {
	if mqttBroker == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "MQTT broker not initialized"})
		return
	}
	c.JSON(http.StatusOK, mqttBroker.GetStats())
}

// getMQTTClientsHandler returns connected clients
// GET /admin/mqtt/clients
func getMQTTClientsHandler(c *gin.Context) {
	if mqttBroker == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "MQTT broker not initialized"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"clients": mqttBroker.GetConnectedClients()})
}

// PublishRequest represents a publish command
type PublishRequest struct {
	Topic   string `json:"topic" binding:"required"`
	Message string `json:"message" binding:"required"`
	Retain  bool   `json:"retain"`
}

// postMQTTPublishHandler publishes a message to a topic
// POST /admin/mqtt/publish
func postMQTTPublishHandler(c *gin.Context) {
	if mqttBroker == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "MQTT broker not initialized"})
		return
	}

	var req PublishRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Internal publish via broker server instance (bypassing ACLs as Admin)
	err := mqttBroker.Publish(req.Topic, []byte(req.Message), req.Retain)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to publish"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "published"})
}
