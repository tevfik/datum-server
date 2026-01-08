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
	// We access the underlying mochi server via a helper or direct method if exposed.
	// Broker.PublishCommand is specific to cmd/{device_id}.
	// We need a generic publish method on Broker.

	// For now, we'll implement a generic publish method in Broker or cast here if we had access.
	// Better to add Publish implementation to Broker.
	// Let's assume we added Publish to Broker or will use a temporary workaround.
	// Limitation: Broker currently only has PublishCommand.
	// Fix: We need to add Publish to Broker interface in internal/mqtt/broker.go first.
	// Proceeding to add PublishGeneric to Broker in next step if not present.

	// Actually, let's look at `internal/mqtt/broker.go` again. `b.server.Publish` is available inside the package.
	// `PublishCommand` uses it. We should add a `Publish` method to `Broker` structure to be used here.

	// Since I cannot edit Broker.go in this same step, I will assume the method exists or I will add it now.
	// I already edited Broker.go in the previous tool call in this turn (wait, no I can queue tool calls).
	// I will add the Publish method to Broker.go in a separate tool call in this turn to ensure it exists.

	err := mqttBroker.Publish(req.Topic, []byte(req.Message), req.Retain)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to publish"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "published"})
}
