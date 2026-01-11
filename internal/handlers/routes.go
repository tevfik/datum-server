package handlers

import (
	"datum-go/internal/auth"

	"github.com/gin-gonic/gin"
)

// RegisterRoutes configures all admin-related routes
func (h *AdminHandler) RegisterRoutes(r *gin.Engine) {
	// System status (public - no auth needed)
	// Note: These used to be in setupAdminRoutes but access global store.
	// We map them to our struct handlers now.
	r.GET("/sys/status", h.GetSystemStatusHandler)
	r.POST("/sys/setup", h.SetupSystemHandler)

	// Admin routes (require admin role)
	admin := r.Group("/admin")
	admin.Use(auth.AdminMiddleware(h.Store))
	{
		// User management
		admin.POST("/users", h.CreateUserHandler)
		admin.GET("/users", h.ListUsersHandler)
		admin.GET("/users/:user_id", h.GetUserHandler)
		admin.PUT("/users/:user_id", h.UpdateUserHandler)
		admin.DELETE("/users/:user_id", h.DeleteUserHandler)
		admin.POST("/users/:username/reset-password", h.ResetPasswordHandler)

		// Device management (all devices across users)
		admin.POST("/dev", h.ProvisionDeviceHandler)
		admin.GET("/dev", h.ListAllDevicesHandler)
		admin.GET("/dev/:device_id", h.GetDeviceAdminHandler)
		admin.PUT("/dev/:device_id", h.UpdateDeviceHandler)
		admin.DELETE("/dev/:device_id", h.ForceDeleteDeviceHandler)

		// Key management (token rotation and revocation)
		admin.POST("/dev/:device_id/rotate-key", h.RotateDeviceKeyHandler)
		admin.POST("/dev/:device_id/revoke-key", h.RevokeDeviceKeyHandler)

		// Database operations
		admin.GET("/database/stats", h.GetDatabaseStatsHandler)
		admin.POST("/database/export", h.ExportDatabaseHandler)
		admin.POST("/database/cleanup", h.ForceCleanupHandler)
		admin.DELETE("/database/reset", h.ResetDatabaseHandler)

		// System Configuration
		// Note: The previous code had `admin.PUT("/config", updateRegistrationConfigHandler)`
		// and also line 74 `admin.PUT("/config/registration", updateRegistrationConfigHandler)`
		// We'll keep both for compatibility if needed, but clean it up.
		admin.PUT("/config", h.UpdateRegistrationConfigHandler)

		admin.GET("/config", h.GetSystemConfigHandler)
		admin.PUT("/config/retention", h.UpdateRetentionPolicyHandler)
		admin.PUT("/config/rate-limit", h.UpdateRateLimitHandler)
		admin.PUT("/config/alerts", h.UpdateAlertConfigHandler)
		admin.PUT("/config/registration", h.UpdateRegistrationConfigHandler)

		// Logs management
		admin.GET("/logs", h.GetLogsHandler)
		admin.GET("/logs/stream", h.StreamLogsHandler) // WebSocket for real-time streaming
		admin.DELETE("/logs", h.ClearLogsHandler)

		// MQTT Management
		admin.GET("/mqtt/stats", h.GetMQTTStatsHandler)
		admin.GET("/mqtt/clients", h.GetMQTTClientsHandler)
		admin.POST("/mqtt/publish", h.PostMQTTPublishHandler)

		// Firmware Management
		admin.POST("/firmware", h.UploadFirmwareHandler)
	}
}
