package handlers

import (
	"datum-go/internal/mqtt"
	"datum-go/internal/storage"
)

// AdminHandler holds dependencies for admin-related handlers
type AdminHandler struct {
	Store      storage.Provider
	MqttBroker *mqtt.Broker
}

// NewAdminHandler creates a new AdminHandler instance
func NewAdminHandler(store storage.Provider, broker *mqtt.Broker) *AdminHandler {
	return &AdminHandler{
		Store:      store,
		MqttBroker: broker,
	}
}
