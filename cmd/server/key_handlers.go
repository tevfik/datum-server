package main

import (
	"net/http"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/logger"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// ============ Key Rotation Handlers ============

// RotateKeyRequest is the request body for rotating a device key
type RotateKeyRequest struct {
	GracePeriodDays int  `json:"grace_period_days,omitempty"` // Default: 7
	NotifyDevice    bool `json:"notify_device,omitempty"`     // Send via command channel
}

// rotateDeviceKeyHandler rotates the API key for a specific device
// POST /admin/devices/:device_id/rotate-key
func rotateDeviceKeyHandler(c *gin.Context) {
	log := logger.GetLogger()
	deviceID := c.Param("device_id")

	var req RotateKeyRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		// Use defaults if no body provided
		req.GracePeriodDays = auth.DefaultGracePeriodDays
	}

	if req.GracePeriodDays <= 0 {
		req.GracePeriodDays = auth.DefaultGracePeriodDays
	}

	// Get the device first
	device, err := store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}

	// Check if device is revoked
	if device.Status == "revoked" {
		c.JSON(http.StatusBadRequest, gin.H{
			"error": "Device keys are revoked. Device must be re-provisioned.",
		})
		return
	}

	// Generate new master secret if not exists
	masterSecret := device.MasterSecret
	if masterSecret == "" {
		var err error
		masterSecret, err = auth.GenerateMasterSecret()
		if err != nil {
			log.Error().Err(err).Str("device_id", deviceID).Msg("Failed to generate master secret")
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate master secret"})
			return
		}
	}

	// Generate new token
	newToken, tokenExpiresAt, err := auth.GenerateDeviceToken(deviceID, masterSecret, auth.DefaultTokenValidityDays)
	if err != nil {
		log.Error().Err(err).Str("device_id", deviceID).Msg("Failed to generate new token")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate new token"})
		return
	}

	// Rotate the key in storage
	updatedDevice, err := store.RotateDeviceKey(deviceID, newToken, tokenExpiresAt, req.GracePeriodDays)
	if err != nil {
		log.Error().Err(err).Str("device_id", deviceID).Msg("Failed to rotate device key")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to rotate key: " + err.Error()})
		return
	}

	// If device didn't have a master secret, initialize it
	if device.MasterSecret == "" {
		store.InitializeDeviceToken(deviceID, masterSecret, newToken, tokenExpiresAt)
	}

	log.Info().
		Str("device_id", deviceID).
		Time("token_expires_at", tokenExpiresAt).
		Time("grace_period_end", updatedDevice.GracePeriodEnd).
		Msg("Device key rotated successfully")

	// Optionally notify device via command channel
	deviceNotified := false
	if req.NotifyDevice {
		// Queue a key_rotation command for the device
		cmd := &storage.Command{
			ID:       generateID("cmd"),
			DeviceID: deviceID,
			Action:   "key_rotation",
			Params: map[string]interface{}{
				"new_token":  newToken,
				"expires_at": tokenExpiresAt.Format(time.RFC3339),
				"deadline":   updatedDevice.GracePeriodEnd.Format(time.RFC3339),
			},
			Status:    "pending",
			CreatedAt: time.Now(),
		}
		// Store command for delivery
		if err := store.CreateCommand(cmd); err == nil {
			deviceNotified = true
			log.Info().Str("device_id", deviceID).Msg("Key rotation command queued for device")
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"device_id":        deviceID,
		"new_token":        newToken,
		"token_expires_at": tokenExpiresAt.Format(time.RFC3339),
		"grace_period_end": updatedDevice.GracePeriodEnd.Format(time.RFC3339),
		"device_notified":  deviceNotified,
		"message":          "Key rotation successful. Old key valid until grace period ends.",
	})
}

// RevokeKeyRequest is the request body for revoking a device key
type RevokeKeyRequest struct {
	Immediate bool `json:"immediate"` // Immediately revoke (default: true)
}

// revokeDeviceKeyHandler immediately revokes all tokens for a device
// POST /admin/devices/:device_id/revoke-key
func revokeDeviceKeyHandler(c *gin.Context) {
	log := logger.GetLogger()
	deviceID := c.Param("device_id")

	var req RevokeKeyRequest
	c.ShouldBindJSON(&req)

	// Revoke the key in storage
	device, err := store.RevokeDeviceKey(deviceID)
	if err != nil {
		if err.Error() == "device not found" {
			c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
			return
		}
		log.Error().Err(err).Str("device_id", deviceID).Msg("Failed to revoke device key")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to revoke key: " + err.Error()})
		return
	}

	log.Warn().
		Str("device_id", deviceID).
		Time("revoked_at", device.KeyRevokedAt).
		Msg("Device keys revoked (emergency)")

	c.JSON(http.StatusOK, gin.H{
		"revoked":    true,
		"device_id":  deviceID,
		"revoked_at": device.KeyRevokedAt.Format(time.RFC3339),
		"message":    "All tokens invalidated. Device requires re-provisioning.",
	})
}

// getDeviceTokenInfoHandler returns token status for a device
// GET /admin/devices/:device_id/token-info
func getDeviceTokenInfoHandler(c *gin.Context) {
	deviceID := c.Param("device_id")

	info, err := store.GetDeviceTokenInfo(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}

	c.JSON(http.StatusOK, info)
}

// ============ Device Token Refresh Handler ============

// refreshTokenHandler allows a device to refresh its own token
// POST /device/token/refresh
func refreshTokenHandler(c *gin.Context) {
	log := logger.GetLogger()

	// Get API key from auth middleware
	apiKey, exists := c.Get("api_key")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Authentication required"})
		return
	}

	// Get device by API key (legacy) or by token
	device, err := store.GetDeviceByAPIKey(apiKey.(string))
	if err != nil {
		// Try to find by token
		device, _, err = store.GetDeviceByToken(apiKey.(string))
		if err != nil {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid credentials"})
			return
		}
	}

	// Check if device is revoked
	if device.Status == "revoked" {
		c.JSON(http.StatusForbidden, gin.H{
			"error":  "key_revoked",
			"action": "reprovision",
		})
		return
	}

	// Get or generate master secret
	masterSecret := device.MasterSecret
	if masterSecret == "" {
		masterSecret, _ = auth.GenerateMasterSecret()
	}

	// Generate new token
	newToken, tokenExpiresAt, err := auth.GenerateDeviceToken(device.ID, masterSecret, auth.DefaultTokenValidityDays)
	if err != nil {
		log.Error().Err(err).Str("device_id", device.ID).Msg("Failed to generate new token")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate token"})
		return
	}

	// Update device
	_, err = store.RotateDeviceKey(device.ID, newToken, tokenExpiresAt, auth.DefaultGracePeriodDays)
	if err != nil {
		log.Error().Err(err).Str("device_id", device.ID).Msg("Failed to rotate key on refresh")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to refresh token"})
		return
	}

	// Initialize master secret if first token
	if device.MasterSecret == "" {
		store.InitializeDeviceToken(device.ID, masterSecret, newToken, tokenExpiresAt)
	}

	log.Info().Str("device_id", device.ID).Msg("Device token refreshed")

	c.JSON(http.StatusOK, gin.H{
		"token_accepted": true,
		"new_token":      newToken,
		"expires_at":     tokenExpiresAt.Format(time.RFC3339),
	})
}

// RegisterKeyManagementRoutes registers key management routes
func RegisterKeyManagementRoutes(adminGroup *gin.RouterGroup, deviceGroup *gin.RouterGroup) {
	// Admin key management (requires admin auth)
	adminGroup.POST("/devices/:device_id/rotate-key", rotateDeviceKeyHandler)
	adminGroup.POST("/devices/:device_id/revoke-key", revokeDeviceKeyHandler)
	adminGroup.GET("/devices/:device_id/token-info", getDeviceTokenInfoHandler)

	// Device self-service (requires device auth)
	deviceGroup.POST("/token/refresh", refreshTokenHandler)
}
