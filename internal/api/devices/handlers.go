// Package devices provides HTTP handlers for device management endpoints.
package devices

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"net/http"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/mqtt"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// Handler provides device HTTP handlers.
type Handler struct {
	Store      storage.Provider
	MQTTBroker *mqtt.Broker
}

// NewHandler creates a new device handler with dependencies.
func NewHandler(store storage.Provider, broker *mqtt.Broker) *Handler {
	return &Handler{
		Store:      store,
		MQTTBroker: broker,
	}
}

// RegisterRoutes registers device routes.
func (h *Handler) RegisterRoutes(r *gin.RouterGroup) {
	r.POST("", h.CreateDevice)
	r.GET("", h.ListDevices)
	r.GET("/:device_id", h.GetDevice)
	r.DELETE("/:device_id", h.DeleteDevice)
	r.PATCH("/:device_id/config", h.UpdateDeviceConfig)
	r.PUT("/:device_id/thing-description", h.UpdateDeviceThingDescription)
}

// ============ Request/Response types ============

// CreateDeviceRequest holds device creation data.
type CreateDeviceRequest struct {
	ID   string `json:"device_id"`
	Name string `json:"name" binding:"required"`
	Type string `json:"type" binding:"required"`
}

// DeviceResponse represents a device in API responses.
type DeviceResponse struct {
	ID               string                 `json:"id"`
	Name             string                 `json:"name"`
	Type             string                 `json:"type"`
	DeviceUID        string                 `json:"device_uid"`
	PublicIP         string                 `json:"public_ip"`
	LastSeen         time.Time              `json:"last_seen"`
	CreatedAt        time.Time              `json:"created_at"`
	Status           string                 `json:"status"`
	ShadowState      map[string]interface{} `json:"shadow_state"`
	DesiredState     map[string]interface{} `json:"desired_state"`
	ThingDescription map[string]interface{} `json:"thing_description"`
}

// ============ Handlers ============

// CreateDevice creates a new device for the authenticated user.
// POST /dev
func (h *Handler) CreateDevice(c *gin.Context) {
	userID, _ := auth.GetUserID(c)

	var req CreateDeviceRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	deviceID := req.ID
	if deviceID == "" {
		deviceID = generateID("dev")
	}
	apiKey, err := auth.GenerateAPIKey()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate API key"})
		return
	}

	device := &storage.Device{
		ID:        deviceID,
		UserID:    userID,
		Name:      req.Name,
		Type:      req.Type,
		APIKey:    apiKey,
		CreatedAt: time.Now(),
	}

	if err := h.Store.CreateDevice(device); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"device_id": deviceID,
		"api_key":   apiKey,
		"message":   "Save this API key - it won't be shown again",
	})
}

// ListDevices lists all devices for the authenticated user.
// GET /dev
func (h *Handler) ListDevices(c *gin.Context) {
	userID, _ := auth.GetUserID(c)
	role, _ := auth.GetUserRole(c)

	var devices []storage.Device
	var err error

	if role == "admin" {
		devices, err = h.Store.GetAllDevices()
	} else {
		devices, err = h.Store.GetUserDevices(userID)
	}

	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	var response []DeviceResponse
	for _, d := range devices {
		status := "offline"
		if time.Since(d.LastSeen) < 5*time.Minute {
			status = "online"
		}
		response = append(response, DeviceResponse{
			ID:               d.ID,
			Name:             d.Name,
			Type:             d.Type,
			DeviceUID:        d.DeviceUID,
			PublicIP:         d.PublicIP,
			LastSeen:         d.LastSeen,
			CreatedAt:        d.CreatedAt,
			Status:           status,
			ShadowState:      d.ShadowState,
			DesiredState:     d.DesiredState,
			ThingDescription: d.ThingDescription,
		})
	}

	c.JSON(http.StatusOK, gin.H{"devices": response})
}

// GetDevice gets a single device by ID.
// GET /dev/:device_id
func (h *Handler) GetDevice(c *gin.Context) {
	userID, _ := auth.GetUserID(c)
	role, _ := auth.GetUserRole(c)
	deviceID := c.Param("device_id")

	device, err := h.Store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}

	isAuthorized := false
	if role == "admin" {
		isAuthorized = true
	} else if userID != "" && device.UserID == userID {
		isAuthorized = true
	} else if val, exists := c.Get("api_key"); exists {
		apiKey := val.(string)
		if requesterDevice, err := h.Store.GetDeviceByAPIKey(apiKey); err == nil {
			if requesterDevice.ID == device.ID || requesterDevice.UserID == device.UserID {
				isAuthorized = true
			}
		}
	}

	if !isAuthorized {
		c.JSON(http.StatusForbidden, gin.H{"error": "Unauthorized"})
		return
	}

	shadowData, err := h.Store.GetLatestData(deviceID)
	var shadowState map[string]interface{}
	if err == nil && shadowData != nil {
		shadowState = shadowData.Data
	} else {
		shadowState = device.ShadowState
	}

	status := "offline"
	if time.Since(device.LastSeen) < 5*time.Minute {
		status = "online"
	}

	resp := DeviceResponse{
		ID:               device.ID,
		Name:             device.Name,
		Type:             device.Type,
		DeviceUID:        device.DeviceUID,
		PublicIP:         device.PublicIP,
		LastSeen:         device.LastSeen,
		CreatedAt:        device.CreatedAt,
		Status:           status,
		ShadowState:      shadowState,
		DesiredState:     device.DesiredState,
		ThingDescription: device.ThingDescription,
	}

	c.JSON(http.StatusOK, resp)
}

// DeleteDevice deletes a device.
// DELETE /dev/:device_id
func (h *Handler) DeleteDevice(c *gin.Context) {
	userID, _ := auth.GetUserID(c)
	role, _ := auth.GetUserRole(c)
	deviceID := c.Param("device_id")

	device, err := h.Store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}

	if role != "admin" && device.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Unauthorized"})
		return
	}

	if err := h.Store.DeleteDevice(deviceID, userID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to delete device"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "Device deleted"})
}

// UpdateDeviceConfig updates the desired configuration.
// PATCH /dev/:device_id/config
func (h *Handler) UpdateDeviceConfig(c *gin.Context) {
	deviceID := c.Param("device_id")
	userID, _ := auth.GetUserID(c)
	role, _ := auth.GetUserRole(c)

	device, err := h.Store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}
	if role != "admin" && device.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Access denied"})
		return
	}

	var config map[string]interface{}
	if err := c.ShouldBindJSON(&config); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if err := h.Store.UpdateDeviceConfig(deviceID, config); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	if h.MQTTBroker != nil {
		payload := map[string]interface{}{
			"desired":   config,
			"timestamp": time.Now().Unix(),
		}
		jsonBytes, _ := json.Marshal(payload)
		topic := "dev/" + deviceID + "/conf/set"
		h.MQTTBroker.Publish(topic, jsonBytes, false)
	}

	c.JSON(http.StatusOK, gin.H{"status": "updated", "config": config})
}

// UpdateDeviceThingDescription updates the Thing Description of a device.
// PUT /dev/:device_id/thing-description
func (h *Handler) UpdateDeviceThingDescription(c *gin.Context) {
	deviceID := c.Param("device_id")
	userID, _ := auth.GetUserID(c)
	role, _ := auth.GetUserRole(c)

	authDeviceID := ""
	if val, exists := c.Get("device_id"); exists {
		authDeviceID = val.(string)
	} else if val, exists := c.Get("api_key"); exists {
		apiKey := val.(string)
		if dev, err := h.Store.GetDeviceByAPIKey(apiKey); err == nil {
			authDeviceID = dev.ID
		}
	}

	device, err := h.Store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}

	isAuthorized := false
	if role == "admin" {
		isAuthorized = true
	}
	if userID != "" && device.UserID == userID {
		isAuthorized = true
	}
	if authDeviceID != "" && authDeviceID == deviceID {
		isAuthorized = true
	}

	if !isAuthorized {
		c.JSON(http.StatusForbidden, gin.H{"error": "Unauthorized"})
		return
	}

	var td map[string]interface{}
	if err := c.ShouldBindJSON(&td); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if err := h.Store.UpdateDeviceThingDescription(deviceID, td); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update TD"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "Thing Description updated", "thing_description": td})
}

// ============ Helper Functions ============

func generateID(prefix string) string {
	bytes := make([]byte, 8)
	rand.Read(bytes)
	return fmt.Sprintf("%s_%s", prefix, hex.EncodeToString(bytes))
}
