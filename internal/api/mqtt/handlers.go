package mqtt_api

import (
	"net/http"

	"datum-go/internal/mqtt"

	"github.com/gin-gonic/gin"
)

type Handler struct {
	Broker *mqtt.Broker
}

func NewHandler(broker *mqtt.Broker) *Handler {
	return &Handler{Broker: broker}
}

// GetMQTTStatsHandler returns broker statistics
// GET /admin/mqtt/stats
func (h *Handler) GetMQTTStatsHandler(c *gin.Context) {
	if h.Broker == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "MQTT broker not initialized"})
		return
	}
	c.JSON(http.StatusOK, h.Broker.GetStats())
}

// GetMQTTClientsHandler returns connected clients
// GET /admin/mqtt/clients
func (h *Handler) GetMQTTClientsHandler(c *gin.Context) {
	if h.Broker == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "MQTT broker not initialized"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"clients": h.Broker.GetConnectedClients()})
}

// PublishRequest represents a publish command
type PublishRequest struct {
	Topic   string `json:"topic" binding:"required"`
	Message string `json:"message" binding:"required"`
	Retain  bool   `json:"retain"`
}

// PostMQTTPublishHandler publishes a message to a topic
// POST /admin/mqtt/publish
func (h *Handler) PostMQTTPublishHandler(c *gin.Context) {
	if h.Broker == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "MQTT broker not initialized"})
		return
	}

	var req PublishRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	err := h.Broker.Publish(req.Topic, []byte(req.Message), req.Retain)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to publish"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "published"})
}

// RegisterRoutes registers MQTT admin routes
func (h *Handler) RegisterRoutes(r *gin.RouterGroup) {
	r.GET("/stats", h.GetMQTTStatsHandler)
	r.GET("/clients", h.GetMQTTClientsHandler)
	r.POST("/publish", h.PostMQTTPublishHandler)
}
