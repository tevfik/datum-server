package provisioning

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

// Config holds provisioning configuration
type Config struct {
	ServerURL          string        // Public server URL for devices
	DefaultExpiration  time.Duration // Default provisioning request expiration
	AllowedDeviceTypes []string      // Allowed device types (empty = all)
}

// Handler handles provisioning requests
type Handler struct {
	Store  storage.Provider
	Config Config
}

// NewHandler creates a new provisioning handler
func NewHandler(store storage.Provider, cfg Config) *Handler {
	if cfg.DefaultExpiration == 0 {
		cfg.DefaultExpiration = 15 * time.Minute
	}
	return &Handler{
		Store:  store,
		Config: cfg,
	}
}

// ============ Request/Response types ============

type RegisterDeviceRequest struct {
	DeviceUID  string `json:"device_uid" binding:"required,max=128"`  // Hardware UID from device
	DeviceName string `json:"device_name" binding:"required,max=100"` // User-provided name
	DeviceType string `json:"device_type" binding:"max=50"`           // Optional device type
	AuthMode   string `json:"auth_mode" binding:"max=20"`             // Optional: "static" (default) or "rotating"
	WiFiSSID   string `json:"wifi_ssid" binding:"max=64"`             // Optional WiFi SSID
	WiFiPass   string `json:"wifi_pass" binding:"max=128"`            // Optional WiFi password
}

type RegisterDeviceResponse struct {
	RequestID    string    `json:"request_id"`
	DeviceUID    string    `json:"device_uid"`
	DeviceID     string    `json:"device_id"`
	APIKey       string    `json:"api_key,omitempty"`       // For static auth
	MasterSecret string    `json:"master_secret,omitempty"` // For rotating auth
	ServerURL    string    `json:"server_url"`
	WiFiSSID     string    `json:"wifi_ssid,omitempty"`
	WiFiPass     string    `json:"wifi_pass,omitempty"`
	ExpiresAt    time.Time `json:"expires_at"`
	Status       string    `json:"status"`
	ActivateURL  string    `json:"activate_url"` // URL for device to call
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

// RegisterDeviceHandler handles device registration from mobile app
// POST /dev/register
func (h *Handler) RegisterDeviceHandler(c *gin.Context) {
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
	registered, existingDeviceID, _ := h.Store.IsDeviceUIDRegistered(deviceUID)
	if registered {
		// Device already registered - delete old registration to allow re-onboarding
		logger.GetLogger().Info().
			Str("device_uid", deviceUID).
			Str("old_device_id", existingDeviceID).
			Msg("Re-registering existing device - deleting old record")

		if err := h.Store.ForceDeleteDevice(existingDeviceID); err != nil {
			// If device not found, it means we have an orphaned UID index. Clean it up directly.
			if err.Error() == "device not found" {
				logger.GetLogger().Warn().Str("device_uid", deviceUID).Msg("Orphaned UID index found, cleaning up manually")
				if err := h.Store.DeleteDeviceUIDIndex(deviceUID); err != nil {
					logger.GetLogger().Error().Err(err).Msg("Failed to remove orphaned UID index")
					c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to clean up orphaned device registration"})
					return
				}
				// Cleanup successful, proceed to registration
			} else {
				logger.GetLogger().Error().Err(err).Str("device_id", existingDeviceID).Msg("Failed to delete old device during re-registration")
				c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to clean up old device registration"})
				return
			}
		}
	}

	// Generate IDs and keys
	requestID := generateProvisioningID("prov")
	deviceID := generateProvisioningID("dev")

	var apiKey string
	var masterSecret string
	var err error

	// Determine Auth Mode
	if req.AuthMode == "rotating" {
		// Generate Master Secret for Rotating Auth
		masterSecret, err = auth.GenerateMasterSecret()
		if err != nil {
			logger.GetLogger().Error().Err(err).Msg("Failed to generate master secret")
			c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to generate security credentials"})
			return
		}
		// In rotating mode, we store the Master Secret as the API Key in DB
		// The device will derive dynamic keys (dk_) from this secret.
		apiKey = masterSecret
	} else {
		// Default: Static Auth (sk_)
		req.AuthMode = "static" // normalize
		apiKey = generateProvisioningAPIKey()
	}

	// 1. Create Provisioning Request (Pending)
	provReq := &storage.ProvisioningRequest{
		ID:         requestID,
		DeviceUID:  deviceUID,
		UserID:     userID,
		DeviceName: req.DeviceName,
		DeviceType: req.DeviceType,
		Status:     "pending", // Start as pending, then complete
		DeviceID:   deviceID,
		APIKey:     apiKey,
		ServerURL:  h.Config.ServerURL,
		WiFiSSID:   req.WiFiSSID,
		WiFiPass:   req.WiFiPass,
		ExpiresAt:  time.Now().Add(h.Config.DefaultExpiration),
		CreatedAt:  time.Now(),
	}

	if err := h.Store.CreateProvisioningRequest(provReq); err != nil {
		logger.GetLogger().Error().Err(err).Msg("Failed to create provisioning request")
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to create provisioning request"})
		return
	}

	// AUTO-COMPLETE: Immediately activate the device (Seamless Onboarding)
	// The user expects the device to be active immediately after registration.
	device, err := h.Store.CompleteProvisioningRequest(requestID)
	if err != nil {
		logger.GetLogger().Error().Err(err).Msg("Failed to auto-complete provisioning")
		// Fallback to pending if completion fails (unlikely)
	} else {
		// Update Status and API Key using the permanent device key
		provReq.Status = "active"
		apiKey = device.APIKey // Use the confirmed key
	}

	// 2. Log successful provisioning registration
	logger.GetLogger().Info().
		Str("event", "provisioning_registration").
		Str("user_id", userID).
		Str("device_uid", deviceUID).
		Str("request_id", requestID).
		Str("device_id", deviceID).
		Str("status", provReq.Status).
		Str("ip", c.ClientIP()).
		Time("expires_at", provReq.ExpiresAt).
		Msg("Device registered and activated")

	// Prepare Response
	resp := RegisterDeviceResponse{
		RequestID:   requestID,
		DeviceUID:   deviceUID,
		DeviceID:    deviceID,
		ServerURL:   h.Config.ServerURL,
		WiFiSSID:    req.WiFiSSID,
		WiFiPass:    req.WiFiPass,
		ExpiresAt:   provReq.ExpiresAt,
		Status:      provReq.Status, // Will be "active"
		ActivateURL: "",             // No activation needed
	}

	// Populate Credentials based on Auth Mode
	if req.AuthMode == "rotating" {
		resp.MasterSecret = apiKey // Used as API Key in DB
	} else {
		resp.APIKey = apiKey
	}

	c.JSON(http.StatusCreated, resp)
}

// CheckDeviceUIDHandler checks if a device UID is already registered
// GET /dev/check-uid/:uid
func (h *Handler) CheckDeviceUIDHandler(c *gin.Context) {
	uid := c.Param("uid")
	if uid == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "uid is required"})
		return
	}

	deviceUID := normalizeUID(uid)

	// Check if registered
	registered, deviceID, _ := h.Store.IsDeviceUIDRegistered(deviceUID)

	// Check if there's a pending request
	var hasPending bool
	var requestID string
	if pendingReq, err := h.Store.GetProvisioningRequestByUID(deviceUID); err == nil {
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

// GetProvisioningStatusHandler returns the status of a provisioning request
// GET /dev/prov/:request_id
func (h *Handler) GetProvisioningStatusHandler(c *gin.Context) {
	userID := c.GetString("user_id")
	requestID := c.Param("request_id")

	req, err := h.Store.GetProvisioningRequest(requestID)
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

// CancelProvisioningHandler cancels a pending provisioning request
// DELETE /dev/prov/:request_id
func (h *Handler) CancelProvisioningHandler(c *gin.Context) {
	userID := c.GetString("user_id")
	requestID := c.Param("request_id")

	req, err := h.Store.GetProvisioningRequest(requestID)
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

	if err := h.Store.CancelProvisioningRequest(requestID); err != nil {
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

// ListProvisioningRequestsHandler lists all provisioning requests for the current user
// GET /dev/prov
func (h *Handler) ListProvisioningRequestsHandler(c *gin.Context) {
	userID := c.GetString("user_id")
	if userID == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	requests, err := h.Store.GetUserProvisioningRequests(userID)
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

// DeviceActivateHandler handles device activation requests
// POST /prov/activate
// Called by the device when it boots and has no credentials
func (h *Handler) DeviceActivateHandler(c *gin.Context) {
	var req DeviceActivateRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid request: " + err.Error()})
		return
	}

	deviceUID := normalizeUID(req.DeviceUID)

	// Check if already registered (device might have lost credentials)
	registered, deviceID, _ := h.Store.IsDeviceUIDRegistered(deviceUID)
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
	provReq, err := h.Store.GetProvisioningRequestByUID(deviceUID)
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
	device, err := h.Store.CompleteProvisioningRequest(provReq.ID)
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

// DeviceCheckHandler allows device to check if there's a pending provisioning request
// GET /prov/check/:uid
// Called by device periodically when in setup mode
func (h *Handler) DeviceCheckHandler(c *gin.Context) {
	uid := c.Param("uid")
	if uid == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "uid is required"})
		return
	}

	deviceUID := normalizeUID(uid)

	// Check if device is already registered
	registered, _, _ := h.Store.IsDeviceUIDRegistered(deviceUID)
	if registered {
		c.JSON(http.StatusOK, gin.H{
			"status":  "active",
			"message": "Device is already registered and active",
		})
		return
	}

	// Check if there's a pending request
	provReq, err := h.Store.GetProvisioningRequestByUID(deviceUID)
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
		"message":      "Provisioning request found. Call /prov/activate to complete.",
		"activate_url": fmt.Sprintf("%s/prov/activate", h.Config.ServerURL),
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

// generateProvisioningAPIKey generates a secure API key for provisioning (Standardized 32 hex)
func generateProvisioningAPIKey() string {
	bytes := make([]byte, 16)
	rand.Read(bytes)
	return "sk_" + hex.EncodeToString(bytes)
}

// RegisterRoutes registers all provisioning-related routes
func (h *Handler) RegisterRoutes(r *gin.Engine, authMiddleware gin.HandlerFunc, rateLimitMiddleware gin.HandlerFunc) {
	// Mobile app endpoints (require JWT auth)
	devices := r.Group("/dev")
	devices.Use(authMiddleware)
	{
		devices.POST("/register", h.RegisterDeviceHandler)
		devices.GET("/check-uid/:uid", h.CheckDeviceUIDHandler)
		devices.GET("/prov", h.ListProvisioningRequestsHandler)
		devices.GET("/prov/:request_id", h.GetProvisioningStatusHandler)
		devices.DELETE("/prov/:request_id", h.CancelProvisioningHandler)
	}

	// Device endpoints (no auth - device doesn't have credentials yet)
	// Apply rate limiting to prevent abuse
	provisioning := r.Group("/prov")
	provisioning.Use(rateLimitMiddleware)
	{
		provisioning.POST("/activate", h.DeviceActivateHandler)
		provisioning.GET("/check/:uid", h.DeviceCheckHandler)
	}
}
