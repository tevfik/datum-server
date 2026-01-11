package main

import (
	"net/http"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// ============ Device Request/Response types ============

type CreateDeviceRequest struct {
	Name string `json:"name" binding:"required"`
	Type string `json:"type" binding:"required"`
}

type DeviceResponse struct {
	ID          string                 `json:"id"`
	Name        string                 `json:"name"`
	Type        string                 `json:"type"`
	DeviceUID   string                 `json:"device_uid"` // Add DeviceUID
	PublicIP    string                 `json:"public_ip"`  // Add PublicIP
	LastSeen    time.Time              `json:"last_seen"`
	CreatedAt   time.Time              `json:"created_at"`
	Status      string                 `json:"status"`
	ShadowState map[string]interface{} `json:"shadow_state"` // Added ShadowState
}

// ============ Device Handlers ============

// createDeviceHandler creates a new device for the authenticated user
// POST /device
func createDeviceHandler(c *gin.Context) {
	userID, _ := auth.GetUserID(c)

	var req CreateDeviceRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	deviceID := generateID("dev")
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

	if err := store.CreateDevice(device); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"device_id": deviceID,
		"api_key":   apiKey,
		"message":   "Save this API key - it won't be shown again",
	})
}

// listDevicesHandler lists all devices for the authenticated user
// GET /device
func listDevicesHandler(c *gin.Context) {
	userID, _ := auth.GetUserID(c)
	role, _ := auth.GetUserRole(c)

	var devices []storage.Device
	var err error

	// Admins see all devices, regular users see only theirs
	if role == "admin" {
		devices, err = store.GetAllDevices()
	} else {
		devices, err = store.GetUserDevices(userID)
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
			ID:        d.ID,
			Name:      d.Name,
			Type:      d.Type,
			DeviceUID: d.DeviceUID,
			PublicIP:  d.PublicIP,
			LastSeen:  d.LastSeen,
			CreatedAt: d.CreatedAt,
			Status:    status,
		})
	}

	c.JSON(http.StatusOK, gin.H{"devices": response})
}

// getDeviceHandler gets a single device by ID
// GET /devices/:device_id
func getDeviceHandler(c *gin.Context) {
	userID, _ := auth.GetUserID(c)
	role, _ := auth.GetUserRole(c)
	deviceID := c.Param("device_id")

	device, err := store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}

	// Authorization check
	if role != "admin" && device.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Unauthorized"})
		return
	}

	// Get latest shadow state
	shadowData, err := store.GetLatestData(deviceID)
	var shadowState map[string]interface{}
	if err == nil && shadowData != nil {
		shadowState = shadowData.Data
	}

	status := "offline"
	if time.Since(device.LastSeen) < 5*time.Minute {
		status = "online"
	}

	resp := DeviceResponse{
		ID:          device.ID,
		Name:        device.Name,
		Type:        device.Type,
		DeviceUID:   device.DeviceUID,
		PublicIP:    device.PublicIP,
		LastSeen:    device.LastSeen,
		CreatedAt:   device.CreatedAt,
		Status:      status,
		ShadowState: shadowState,
	}

	c.JSON(http.StatusOK, resp)
}

// deleteDeviceHandler deletes a device
// DELETE /devices/:device_id
func deleteDeviceHandler(c *gin.Context) {
	userID, _ := auth.GetUserID(c)
	role, _ := auth.GetUserRole(c)
	deviceID := c.Param("device_id")

	device, err := store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}

	// Authorization check
	if role != "admin" && device.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Unauthorized"})
		return
	}

	if err := store.DeleteDevice(deviceID, userID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to delete device"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "Device deleted"})
}
