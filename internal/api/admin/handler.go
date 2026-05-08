package admin

import (
	"datum-go/internal/mqtt"
	"datum-go/internal/notify"
	"datum-go/internal/storage"
	"time"
)

// AdminHandler holds dependencies for admin-related handlers
type AdminHandler struct {
	Store           storage.Provider
	MqttBroker      *mqtt.Broker
	Dispatcher      *notify.Dispatcher
	ServerStartTime time.Time // Server start time for uptime calculation
}

// NewAdminHandler creates a new AdminHandler instance
func NewAdminHandler(store storage.Provider, broker *mqtt.Broker, dispatcher *notify.Dispatcher, startTime time.Time) *AdminHandler {
	return &AdminHandler{
		Store:           store,
		MqttBroker:      broker,
		Dispatcher:      dispatcher,
		ServerStartTime: startTime,
	}
}
