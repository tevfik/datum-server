// Package api provides the central router for all API endpoints.
package api

import (
	"datum-go/internal/api/auth"
	"datum-go/internal/api/commands"
	"datum-go/internal/api/data"
	"datum-go/internal/api/db"
	"datum-go/internal/api/devices"
	"datum-go/internal/api/keys"
	mqtt_api "datum-go/internal/api/mqtt"
	"datum-go/internal/api/provisioning"
	"datum-go/internal/api/public"
	"datum-go/internal/api/sse"
	"datum-go/internal/api/stream"
	"datum-go/internal/api/system"
	internalauth "datum-go/internal/auth"
	"datum-go/internal/email"
	"datum-go/internal/metrics"
	mqtt_internal "datum-go/internal/mqtt"
	"datum-go/internal/notify"
	"datum-go/internal/processing"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// Config holds configuration for the API router.
type Config struct {
	Store        storage.Provider
	Processor    *processing.TelemetryProcessor
	MQTTBroker   *mqtt_internal.Broker
	EmailService *email.EmailSender
	Notifier     *notify.NtfyClient
	Dispatcher   *notify.Dispatcher
	PublicURL    string
	Version      string
	BuildDate    string
}

// RegisterAuthRoutes registers authentication routes.
func RegisterAuthRoutes(r gin.IRouter, cfg Config) {
	authHandler := auth.NewHandler(cfg.Store, cfg.EmailService, cfg.Notifier, cfg.Dispatcher, cfg.PublicURL)
	authHandler.RegisterRoutes(r, internalauth.UserAuthMiddleware(cfg.Store))

	// Admin User Routes
	adminUserGroup := r.Group("/admin/users")
	adminUserGroup.Use(internalauth.UserAuthMiddleware(cfg.Store))
	adminUserGroup.Use(internalauth.AdminMiddleware(cfg.Store))
	authHandler.RegisterAdminRoutes(adminUserGroup)
}

// RegisterDeviceRoutes registers device management routes.
func RegisterDeviceRoutes(r gin.IRouter, cfg Config) {
	devGroup := r.Group("/dev")
	devGroup.Use(internalauth.UserAuthMiddleware(cfg.Store))
	{
		devicesHandler := devices.NewHandler(cfg.Store, cfg.MQTTBroker)
		devicesHandler.RegisterRoutes(devGroup)
	}
}

// RegisterDataRoutes registers data ingestion and retrieval routes.
func RegisterDataRoutes(r gin.IRouter, cfg Config) {
	dataHandler := data.NewHandler(cfg.Store, cfg.Processor, cfg.MQTTBroker)

	dataDeviceGroup := r.Group("/data")
	dataDeviceGroup.Use(internalauth.DeviceAuthMiddleware())
	dataHandler.RegisterRoutes(dataDeviceGroup)

	dataUserGroup := r.Group("/data")
	dataUserGroup.Use(internalauth.UserAuthMiddleware(cfg.Store))
	dataHandler.RegisterUserRoutes(dataUserGroup)
}

// RegisterKeyRoutes registers user API key management routes.
func RegisterKeyRoutes(r gin.IRouter, cfg Config) {
	keyHandler := keys.NewHandler(cfg.Store)
	keyGroup := r.Group("/auth")
	keyGroup.Use(internalauth.UserAuthMiddleware(cfg.Store))
	keyHandler.RegisterRoutes(keyGroup)
}

// RegisterMQTTRoutes registers MQTT admin routes.
func RegisterMQTTRoutes(r gin.IRouter, cfg Config) {
	mqttHandler := mqtt_api.NewHandler(cfg.MQTTBroker)
	mqttGroup := r.Group("/admin/mqtt")
	mqttGroup.Use(internalauth.UserAuthMiddleware(cfg.Store))
	mqttGroup.Use(internalauth.AdminMiddleware(cfg.Store))
	mqttHandler.RegisterRoutes(mqttGroup)
}

// RegisterMetricsRoutes registers metrics routes.
func RegisterMetricsRoutes(r gin.IRouter, cfg Config) {
	// Metrics are public or system restricted? Usually public or protected.
	// Legacy main.go didn't show restriction, but usually /sys/metrics.
	// We'll put it under /sys without auth for Prometheus scraping or require auth.
	// For now, open or /sys group which is public in current setup?
	// /sys/status is public.
	r.GET("/sys/metrics", metrics.Handler)
}

// RegisterCommandRoutes registers device command routes.
func RegisterCommandRoutes(r gin.IRouter, cfg Config) {
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

// RegisterStreamRoutes registers video streaming routes.
func RegisterStreamRoutes(r gin.IRouter, cfg Config) {
	streamHandler := stream.NewHandler(cfg.Store)

	// Device upload (Device Auth)
	devAuthGroup := r.Group("/dev")
	devAuthGroup.Use(internalauth.DeviceAuthMiddleware())
	streamHandler.RegisterDeviceRoutes(devAuthGroup)

	// Hybrid stream consumption (User or Device Auth)
	hybridGroup := r.Group("/dev")
	hybridGroup.Use(internalauth.HybridAuthMiddleware(cfg.Store))
	streamHandler.RegisterHybridRoutes(hybridGroup)
}

// RegisterSpecializedRoutes registers hybrid auth routes for devices and data.
// These cover /dev/:device_id and /dev/:device_id/data which support both User and Device auth.
func RegisterSpecializedRoutes(r gin.IRouter, cfg Config) {
	devicesHandler := devices.NewHandler(cfg.Store, cfg.MQTTBroker)
	dataHandler := data.NewHandler(cfg.Store, cfg.Processor, cfg.MQTTBroker)

	hybridGroup := r.Group("/dev")
	hybridGroup.Use(internalauth.HybridAuthMiddleware(cfg.Store))
	{
		// /dev/:device_id - Get Device Info
		devicesHandler.RegisterHybridRoutes(hybridGroup)
		// /dev/:device_id/data - Get Device Data
		dataHandler.RegisterHybridRoutes(hybridGroup)
		// /dev/:device_id/thing-description - Update Thing Description
		devicesHandler.RegisterThingDescriptionRoutes(hybridGroup)
	}
}

// RegisterSSERoutes registers SSE and polling routes.
func RegisterSSERoutes(r gin.IRouter, cfg Config) {
	sseHandler := sse.NewHandler(cfg.Store)

	// Device routes (Device Auth)
	devAuthGroup := r.Group("/dev")
	devAuthGroup.Use(internalauth.DeviceAuthMiddleware())
	sseHandler.RegisterRoutes(devAuthGroup)
}

// RegisterPublicRoutes registers public data routes.
func RegisterPublicRoutes(r gin.IRouter, cfg Config) {
	pubHandler := public.NewHandler(cfg.Store)
	pubGroup := r.Group("/pub")
	pubHandler.RegisterRoutes(pubGroup)
}

// RegisterDBRoutes registers database routes (User and Admin).
func RegisterDBRoutes(r gin.IRouter, cfg Config) {
	dbHandler := db.NewHandler(cfg.Store)

	// User DB routes
	userDBGroup := r.Group("/db")
	userDBGroup.Use(internalauth.UserAuthMiddleware(cfg.Store))
	dbHandler.RegisterRoutes(userDBGroup)

	// Admin DB routes
	// Frontend expects /admin/database/stats, so we map /admin/database
	adminDBGroup := r.Group("/admin/database")
	adminDBGroup.Use(internalauth.UserAuthMiddleware(cfg.Store))
	adminDBGroup.Use(internalauth.AdminMiddleware(cfg.Store))
	dbHandler.RegisterAdminRoutes(adminDBGroup)
}

// RegisterSystemRoutes registers system info routes.
func RegisterSystemRoutes(r gin.IRouter, cfg Config) {
	sysHandler := system.NewHandler(cfg.Version, cfg.BuildDate, cfg.Store)
	sysGroup := r.Group("/sys")
	sysHandler.RegisterRoutes(sysGroup)

	// Admin System Routes (/admin/sys/...)
	adminSysGroup := r.Group("/admin/sys")
	adminSysGroup.Use(internalauth.UserAuthMiddleware(cfg.Store))
	adminSysGroup.Use(internalauth.AdminMiddleware(cfg.Store))
	sysHandler.RegisterAdminRoutes(adminSysGroup)
}

// RegisterProvisioningRoutes registers provisioning routes.
func RegisterProvisioningRoutes(r gin.IRouter, cfg Config) {
	provHandler := provisioning.NewHandler(cfg.Store, provisioning.Config{
		ServerURL: cfg.PublicURL,
	})
	provHandler.MQTTBroker = cfg.MQTTBroker

	provHandler.RegisterRoutes(r, internalauth.UserAuthMiddleware(cfg.Store), internalauth.RateLimitMiddleware())
}

// RegisterV1Routes registers all API routes under /api/v1 prefix.
// This provides versioned endpoints alongside the unversioned ones for
// backward compatibility.
func RegisterV1Routes(r *gin.Engine, cfg Config) {
	v1 := r.Group("/api/v1")

	RegisterAuthRoutes(v1, cfg)
	RegisterDeviceRoutes(v1, cfg)
	RegisterDataRoutes(v1, cfg)
	RegisterKeyRoutes(v1, cfg)
	RegisterCommandRoutes(v1, cfg)
	RegisterPublicRoutes(v1, cfg)
	RegisterDBRoutes(v1, cfg)
	RegisterSystemRoutes(v1, cfg)
	RegisterMetricsRoutes(v1, cfg)
	RegisterSpecializedRoutes(v1, cfg)
	RegisterProvisioningRoutes(v1, cfg)
}
