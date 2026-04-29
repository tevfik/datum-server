package admin

import (
	"datum-go/internal/auth"

	"github.com/gin-gonic/gin"
)

// deprecatedConfigPath emits RFC-8594 Deprecation + Sunset headers (and a
// Link to the canonical /admin/sys/* equivalent) on every response. The
// underlying handler still runs unchanged, so existing automation keeps
// working until the sunset date.
func deprecatedConfigPath(replacement string) gin.HandlerFunc {
	return func(c *gin.Context) {
		c.Header("Deprecation", "true")
		c.Header("Sunset", "Wed, 31 Dec 2026 23:59:59 GMT")
		c.Header("Link", "<"+replacement+">; rel=\"successor-version\"")
		c.Next()
	}
}

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
		// admin.GET("/users", h.ListUsersHandler)        // Handled by api/auth
		admin.GET("/users/:user_id", h.GetUserHandler) // Keep: Not in api/auth
		// admin.PUT("/users/:user_id", h.UpdateUserHandler)   // Handled by api/auth
		// admin.DELETE("/users/:user_id", h.DeleteUserHandler) // Handled by api/auth
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
		// admin.GET("/database/stats", h.GetDatabaseStatsHandler) // Handled by api/db
		admin.POST("/database/export", h.ExportDatabaseHandler) // Keep: Check if in api/db
		admin.POST("/database/cleanup", h.ForceCleanupHandler)  // Keep: Check if in api/db
		admin.DELETE("/database/reset", h.ResetDatabaseHandler) // Keep: Check if in api/db

		// System Configuration — DEPRECATED aliases.
		//
		// Canonical paths live under /admin/sys/* (see internal/api/system).
		// These /admin/config/* routes are kept until 2026-12-31 to avoid
		// breaking older tooling, but every response carries Deprecation +
		// Sunset + Link headers pointing at the new location.
		//
		// The previously-registered bare `PUT /admin/config` was removed: it
		// pretended to be a generic config update but actually only flipped
		// the registration toggle, which surprised callers. Use
		// `/admin/sys/registration` (or the deprecated alias
		// `/admin/config/registration`) instead.
		admin.GET("/config", deprecatedConfigPath("/admin/sys/config"), h.GetSystemConfigHandler)
		admin.PUT("/config/retention", deprecatedConfigPath("/admin/sys/retention"), h.UpdateRetentionPolicyHandler)
		admin.PUT("/config/rate-limit", deprecatedConfigPath("/admin/sys/rate-limit"), h.UpdateRateLimitHandler)
		admin.PUT("/config/alerts", deprecatedConfigPath("/admin/sys/alerts"), h.UpdateAlertConfigHandler)
		admin.PUT("/config/registration", deprecatedConfigPath("/admin/sys/registration"), h.UpdateRegistrationConfigHandler)

		// Logs management
		admin.GET("/logs", h.GetLogsHandler)
		admin.GET("/logs/stream", h.StreamLogsHandler) // WebSocket for real-time streaming
		admin.DELETE("/logs", h.ClearLogsHandler)

		// MQTT Management (Moved to api.RegisterMQTTRoutes)
		// admin.GET("/mqtt/stats", h.GetMQTTStatsHandler)
		// admin.GET("/mqtt/clients", h.GetMQTTClientsHandler)
		// admin.POST("/mqtt/publish", h.PostMQTTPublishHandler)

		// Firmware Management
		admin.POST("/firmware", h.UploadFirmwareHandler)
	}
}
