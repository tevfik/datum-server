// Package api provides the central router for all API endpoints.
package api

import (
	"datum-go/internal/api/auth"
	"datum-go/internal/api/commands"
	"datum-go/internal/api/data"
	"datum-go/internal/api/devices"
	internalauth "datum-go/internal/auth"
	"datum-go/internal/email"
	"datum-go/internal/mqtt"
	"datum-go/internal/processing"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// Config holds configuration for the API router.
type Config struct {
	Store        storage.Provider
	Processor    *processing.TelemetryProcessor
	MQTTBroker   *mqtt.Broker
	EmailService *email.EmailSender
	PublicURL    string
}

// RegisterAuthRoutes registers authentication routes.
func RegisterAuthRoutes(r *gin.Engine, cfg Config) {
	authHandler := auth.NewHandler(cfg.Store, cfg.EmailService, cfg.PublicURL)
	authHandler.RegisterRoutes(r, internalauth.UserAuthMiddleware(cfg.Store))
}

// RegisterDeviceRoutes registers device management routes.
func RegisterDeviceRoutes(r *gin.Engine, cfg Config) {
	devGroup := r.Group("/dev")
	devGroup.Use(internalauth.UserAuthMiddleware(cfg.Store))
	{
		devicesHandler := devices.NewHandler(cfg.Store, cfg.MQTTBroker)
		devicesHandler.RegisterRoutes(devGroup)
	}
}

// RegisterDataRoutes registers data ingestion and retrieval routes.
func RegisterDataRoutes(r *gin.Engine, cfg Config) {
	dataHandler := data.NewHandler(cfg.Store, cfg.Processor, cfg.MQTTBroker)

	dataDeviceGroup := r.Group("/data")
	dataDeviceGroup.Use(internalauth.DeviceAuthMiddleware())
	dataHandler.RegisterRoutes(dataDeviceGroup)

	dataUserGroup := r.Group("/data")
	dataUserGroup.Use(internalauth.UserAuthMiddleware(cfg.Store))
	dataHandler.RegisterUserRoutes(dataUserGroup)
}

// RegisterAllRoutes registers all API routes (auth, devices, data).
// Use this for a full migration, or call individual Register*Routes for incremental migration.
func RegisterAllRoutes(r *gin.Engine, cfg Config) {
	RegisterAuthRoutes(r, cfg)
	RegisterDeviceRoutes(r, cfg)
	RegisterDataRoutes(r, cfg)
	RegisterCommandRoutes(r, cfg)
}

// RegisterCommandRoutes registers device command routes.
func RegisterCommandRoutes(r *gin.Engine, cfg Config) {
	cmdHandler := commands.NewHandler(cfg.Store, cfg.MQTTBroker)

	// User routes (send/list commands)
	userCmdGroup := r.Group("/dev")
	userCmdGroup.Use(internalauth.UserAuthMiddleware(cfg.Store))
	cmdHandler.RegisterUserRoutes(userCmdGroup)

	// Device routes (poll/ack commands)
	deviceCmdGroup := r.Group("/dev")
	deviceCmdGroup.Use(internalauth.DeviceAuthMiddleware())
	cmdHandler.RegisterDeviceRoutes(deviceCmdGroup)
}
