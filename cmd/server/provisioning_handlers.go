package main

import (
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"net/http"
	"strings"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/logger"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// ProvisioningConfig holds provisioning configuration
type ProvisioningConfig struct {
	ServerURL          string        // Public server URL for devices
	DefaultExpiration  time.Duration // Default provisioning request expiration
	AllowedDeviceTypes []string      // Allowed device types (empty = all)
}

var provisioningConfig = ProvisioningConfig{
	ServerURL:         "http://localhost:8080",
	DefaultExpiration: 15 * time.Minute,
}

// SetProvisioningServerURL sets the public server URL for provisioning
func SetProvisioningServerURL(url string) {
	provisioningConfig.ServerURL = url
}

// ============ Request/Response types ============

type RegisterDeviceRequest struct {
	DeviceUID  string `json:"device_uid" binding:"required"`  // Hardware UID from device
	DeviceName string `json:"device_name" binding:"required"` // User-provided name
	DeviceType string `json:"device_type"`                    // Optional device type
	WiFiSSID   string `json:"wifi_ssid"`                      // Optional WiFi SSID
	WiFiPass   string `json:"wifi_pass"`                      // Optional WiFi password
}

type RegisterDeviceResponse struct {
	RequestID   string    `json:"request_id"`
	DeviceUID   string    `json:"device_uid"`
	DeviceID    string    `json:"device_id"`
	APIKey      string    `json:"api_key"`
	ServerURL   string    `json:"server_url"`
	WiFiSSID    string    `json:"wifi_ssid,omitempty"`
	WiFiPass    string    `json:"wifi_pass,omitempty"`
	ExpiresAt   time.Time `json:"expires_at"`
	Status      string    `json:"status"`
	ActivateURL string    `json:"activate_url"` // URL for device to call
}

type CheckUIDRequest struct {
	DeviceUID string `json:"device_uid" binding:"required"`
}

type CheckUIDResponse struct {
	Registered bool   `json:"registered"`
	DeviceID   string `json:"device_id,omitempty"`
	HasPending bool   `json:"has_pending"`
	RequestID  string `json:"request_id,omitempty"`
}

type DeviceActivateRequest struct {
	DeviceUID       string `json:"device_uid" binding:"required"`
	FirmwareVersion string `json:"firmware_version"`
	Model           string `json:"model"`
}

type DeviceActivateResponse struct {
	DeviceID  string `json:"device_id"`
	APIKey    string `json:"api_key"`
	ServerURL string `json:"server_url"`
	WiFiSSID  string `json:"wifi_ssid,omitempty"`
	WiFiPass  string `json:"wifi_pass,omitempty"`
	Message   string `json:"message"`
}

type DeviceInfoResponse struct {
	DeviceUID       string `json:"device_uid"`
	FirmwareVersion string `json:"firmware_version"`
	Model           string `json:"model"`
	Status          string `json:"status"` // "unconfigured", "pending", "configured"
}

// ============ Mobile App Endpoints (JWT Auth Required) ============

// registerDeviceHandler handles device registration from mobile app
// POST /devices/register
func registerDeviceHandler(c *gin.Context) {
	userID := c.GetString("user_id")
	if userID == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	var req RegisterDeviceRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid request: " + err.Error()})
		return
	}

	// Normalize UID (uppercase, remove separators)
	deviceUID := normalizeUID(req.DeviceUID)

	// Check if UID is already registered
	registered, existingDeviceID, _ := store.IsDeviceUIDRegistered(deviceUID)
	if registered {
		// Device already registered - delete old registration to allow re-onboarding
		logger.GetLogger().Info().
			Str("device_uid", deviceUID).
			Str("old_device_id", existingDeviceID).
			Msg("Re-registering existing device - deleting old record")

		if err := store.ForceDeleteDevice(existingDeviceID); err != nil {
			logger.GetLogger().Error().Err(err).Str("device_id", existingDeviceID).Msg("Failed to delete old device during re-registration")
			c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to clean up old device registration"})
			return
		}
	}

	// Generate IDs and keys
	requestID := generateProvisioningID("prov")
	deviceID := generateProvisioningID("dev")
	apiKey := generateProvisioningAPIKey()

	// Create provisioning request
	provReq := &storage.ProvisioningRequest{
		ID:         requestID,
		DeviceUID:  deviceUID,
		UserID:     userID,
		DeviceName: req.DeviceName,
		DeviceType: req.DeviceType,
		Status:     "pending",
		DeviceID:   deviceID,
		APIKey:     apiKey,
		ServerURL:  provisioningConfig.ServerURL,
		WiFiSSID:   req.WiFiSSID,
		WiFiPass:   req.WiFiPass,
		ExpiresAt:  time.Now().Add(provisioningConfig.DefaultExpiration),
		CreatedAt:  time.Now(),
	}

	if err := store.CreateProvisioningRequest(provReq); err != nil {
		logger.GetLogger().Warn().
			Str("user_id", userID).
			Str("device_uid", deviceUID).
			Str("ip", c.ClientIP()).
			Err(err).
			Msg("Failed to create provisioning request")

		if strings.Contains(err.Error(), "already exists") || strings.Contains(err.Error(), "already registered") {
			c.JSON(http.StatusConflict, gin.H{"error": err.Error()})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to create provisioning request"})
		return
	}

	// Log successful provisioning registration
	logger.GetLogger().Info().
		Str("event", "provisioning_registration").
		Str("user_id", userID).
		Str("device_uid", deviceUID).
		Str("request_id", requestID).
		Str("device_id", deviceID).
		Str("ip", c.ClientIP()).
		Time("expires_at", provReq.ExpiresAt).
		Msg("Provisioning request created")

	c.JSON(http.StatusCreated, RegisterDeviceResponse{
		RequestID:   requestID,
		DeviceUID:   deviceUID,
		DeviceID:    deviceID,
		APIKey:      apiKey,
		ServerURL:   provisioningConfig.ServerURL,
		WiFiSSID:    req.WiFiSSID,
		WiFiPass:    req.WiFiPass,
		ExpiresAt:   provReq.ExpiresAt,
		Status:      "pending",
		ActivateURL: fmt.Sprintf("%s/provisioning/activate", provisioningConfig.ServerURL),
	})
}

// checkDeviceUIDHandler checks if a device UID is already registered
// GET /devices/check-uid/:uid
func checkDeviceUIDHandler(c *gin.Context) {
	uid := c.Param("uid")
	if uid == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "uid is required"})
		return
	}

	deviceUID := normalizeUID(uid)

	// Check if registered
	registered, deviceID, _ := store.IsDeviceUIDRegistered(deviceUID)

	// Check if there's a pending request
	var hasPending bool
	var requestID string
	if pendingReq, err := store.GetProvisioningRequestByUID(deviceUID); err == nil {
		if pendingReq.Status == "pending" && time.Now().Before(pendingReq.ExpiresAt) {
			hasPending = true
			requestID = pendingReq.ID
		}
	}

	c.JSON(http.StatusOK, CheckUIDResponse{
		Registered: registered,
		DeviceID:   deviceID,
		HasPending: hasPending,
		RequestID:  requestID,
	})
}

// getProvisioningStatusHandler returns the status of a provisioning request
// GET /devices/provisioning/:request_id
func getProvisioningStatusHandler(c *gin.Context) {
	userID := c.GetString("user_id")
	requestID := c.Param("request_id")

	req, err := store.GetProvisioningRequest(requestID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "provisioning request not found"})
		return
	}

	// Check ownership
	if req.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "not authorized to view this request"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"request_id":  req.ID,
		"device_uid":  req.DeviceUID,
		"device_name": req.DeviceName,
		"device_id":   req.DeviceID,
		"status":      req.Status,
		"expires_at":  req.ExpiresAt,
		"created_at":  req.CreatedAt,
	})
}

// cancelProvisioningHandler cancels a pending provisioning request
// DELETE /devices/provisioning/:request_id
func cancelProvisioningHandler(c *gin.Context) {
	userID := c.GetString("user_id")
	requestID := c.Param("request_id")

	req, err := store.GetProvisioningRequest(requestID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "provisioning request not found"})
		return
	}

	// Check ownership
	if req.UserID != userID {
		logger.GetLogger().Warn().
			Str("event", "provisioning_cancel_forbidden").
			Str("user_id", userID).
			Str("request_id", requestID).
			Str("owner_id", req.UserID).
			Str("ip", c.ClientIP()).
			Msg("Unauthorized provisioning cancel attempt")

		c.JSON(http.StatusForbidden, gin.H{"error": "not authorized to cancel this request"})
		return
	}

	if err := store.CancelProvisioningRequest(requestID); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Log cancellation
	logger.GetLogger().Info().
		Str("event", "provisioning_cancelled").
		Str("user_id", userID).
		Str("request_id", requestID).
		Str("device_uid", req.DeviceUID).
		Str("ip", c.ClientIP()).
		Msg("Provisioning request cancelled")

	c.JSON(http.StatusOK, gin.H{"message": "provisioning request cancelled"})
}

// listProvisioningRequestsHandler lists all provisioning requests for the current user
// GET /devices/provisioning
func listProvisioningRequestsHandler(c *gin.Context) {
	userID := c.GetString("user_id")
	if userID == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	requests, err := store.GetUserProvisioningRequests(userID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to get provisioning requests"})
		return
	}

	// Filter out sensitive data and format response
	var response []gin.H
	for _, req := range requests {
		response = append(response, gin.H{
			"request_id":  req.ID,
			"device_uid":  req.DeviceUID,
			"device_name": req.DeviceName,
			"device_type": req.DeviceType,
			"device_id":   req.DeviceID,
			"status":      req.Status,
			"expires_at":  req.ExpiresAt,
			"created_at":  req.CreatedAt,
		})
	}

	c.JSON(http.StatusOK, gin.H{"requests": response})
}

// ============ Device Endpoints (No Auth - Called by IoT Device) ============

// deviceActivateHandler handles device activation requests
// POST /provisioning/activate
// Called by the device when it boots and has no credentials
func deviceActivateHandler(c *gin.Context) {
	var req DeviceActivateRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid request: " + err.Error()})
		return
	}

	deviceUID := normalizeUID(req.DeviceUID)

	// Check if already registered (device might have lost credentials)
	registered, deviceID, _ := store.IsDeviceUIDRegistered(deviceUID)
	if registered {
		// Device is already registered but asking for credentials
		// This could be a factory reset scenario
		c.JSON(http.StatusConflict, gin.H{
			"error":     "device already registered",
			"device_id": deviceID,
			"message":   "Contact owner to reset device or transfer ownership",
		})
		return
	}

	// Look for pending provisioning request
	provReq, err := store.GetProvisioningRequestByUID(deviceUID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{
			"error":   "no provisioning request found",
			"message": "Device must be registered via mobile app first",
		})
		return
	}

	// Check if request is valid
	if provReq.Status != "pending" {
		c.JSON(http.StatusGone, gin.H{
			"error":  "provisioning request is not pending",
			"status": provReq.Status,
		})
		return
	}

	if time.Now().After(provReq.ExpiresAt) {
		c.JSON(http.StatusGone, gin.H{
			"error":   "provisioning request has expired",
			"message": "Please create a new provisioning request via mobile app",
		})
		return
	}

	// Complete the provisioning
	device, err := store.CompleteProvisioningRequest(provReq.ID)
	if err != nil {
		logger.GetLogger().Error().
			Str("request_id", provReq.ID).
			Str("device_uid", deviceUID).
			Str("ip", c.ClientIP()).
			Err(err).
			Msg("Failed to complete provisioning")

		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to complete provisioning: " + err.Error()})
		return
	}

	// Log successful activation
	logger.GetLogger().Info().
		Str("event", "device_activation").
		Str("device_id", device.ID).
		Str("device_uid", deviceUID).
		Str("user_id", provReq.UserID).
		Str("request_id", provReq.ID).
		Str("firmware", req.FirmwareVersion).
		Str("model", req.Model).
		Str("ip", c.ClientIP()).
		Msg("Device activated successfully")

	c.JSON(http.StatusOK, DeviceActivateResponse{
		DeviceID:  device.ID,
		APIKey:    device.APIKey,
		ServerURL: provReq.ServerURL,
		WiFiSSID:  provReq.WiFiSSID,
		WiFiPass:  provReq.WiFiPass,
		Message:   "Device activated successfully",
	})
}

// deviceCheckHandler allows device to check if there's a pending provisioning request
// GET /provisioning/check/:uid
// Called by device periodically when in setup mode
func deviceCheckHandler(c *gin.Context) {
	uid := c.Param("uid")
	if uid == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "uid is required"})
		return
	}

	deviceUID := normalizeUID(uid)

	// Check if there's a pending request
	provReq, err := store.GetProvisioningRequestByUID(deviceUID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{
			"status":  "unconfigured",
			"message": "No provisioning request found. Register device via mobile app.",
		})
		return
	}

	if provReq.Status != "pending" {
		c.JSON(http.StatusOK, gin.H{
			"status":  provReq.Status,
			"message": "Provisioning request is " + provReq.Status,
		})
		return
	}

	if time.Now().After(provReq.ExpiresAt) {
		c.JSON(http.StatusOK, gin.H{
			"status":  "expired",
			"message": "Provisioning request has expired",
		})
		return
	}

	// There's a valid pending request - device should activate
	c.JSON(http.StatusOK, gin.H{
		"status":       "pending",
		"message":      "Provisioning request found. Call /provisioning/activate to complete.",
		"activate_url": fmt.Sprintf("%s/provisioning/activate", provisioningConfig.ServerURL),
		"expires_at":   provReq.ExpiresAt,
	})
}

// ============ Helper Functions ============

// normalizeUID normalizes a device UID (uppercase, remove common separators)
func normalizeUID(uid string) string {
	uid = strings.ToUpper(uid)
	uid = strings.ReplaceAll(uid, ":", "")
	uid = strings.ReplaceAll(uid, "-", "")
	uid = strings.ReplaceAll(uid, " ", "")
	return uid
}

// generateProvisioningID generates a prefixed random ID for provisioning
func generateProvisioningID(prefix string) string {
	bytes := make([]byte, 6)
	rand.Read(bytes)
	return fmt.Sprintf("%s_%s", prefix, hex.EncodeToString(bytes))
}

// generateProvisioningAPIKey generates a secure API key for provisioning
func generateProvisioningAPIKey() string {
	bytes := make([]byte, 8)
	rand.Read(bytes)
	return "dk_" + hex.EncodeToString(bytes)
}

// ============ Route Registration ============

// RegisterProvisioningRoutes registers all provisioning-related routes
func RegisterProvisioningRoutes(r *gin.Engine, authMiddleware gin.HandlerFunc) {
	// Mobile app endpoints (require JWT auth)
	devices := r.Group("/devices")
	devices.Use(authMiddleware)
	{
		devices.POST("/register", registerDeviceHandler)
		devices.GET("/check-uid/:uid", checkDeviceUIDHandler)
		devices.GET("/provisioning", listProvisioningRequestsHandler)
		devices.GET("/provisioning/:request_id", getProvisioningStatusHandler)
		devices.DELETE("/provisioning/:request_id", cancelProvisioningHandler)
	}

	// Device endpoints (no auth - device doesn't have credentials yet)
	// Apply rate limiting to prevent abuse
	provisioning := r.Group("/provisioning")
	provisioning.Use(auth.RateLimitMiddleware())
	{
		provisioning.POST("/activate", deviceActivateHandler)
		provisioning.GET("/check/:uid", deviceCheckHandler)
	}
}
