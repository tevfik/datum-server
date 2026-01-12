package handlers

import (
	"datum-go/internal/mqtt"
	"datum-go/internal/storage"
	"time"
)

// AdminHandler holds dependencies for admin-related handlers
type AdminHandler struct {
	Store           storage.Provider
	MqttBroker      *mqtt.Broker
	ServerStartTime time.Time // Server start time for uptime calculation
}

// NewAdminHandler creates a new AdminHandler instance
func NewAdminHandler(store storage.Provider, broker *mqtt.Broker, startTime time.Time) *AdminHandler {
	return &AdminHandler{
		Store:           store,
		MqttBroker:      broker,
		ServerStartTime: startTime,
	}
}
