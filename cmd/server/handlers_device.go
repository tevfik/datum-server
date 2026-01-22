package main

import (
	"encoding/json"
	"net/http"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// ============ Device Request/Response types ============

type CreateDeviceRequest struct {
	ID   string `json:"device_id"`
	Name string `json:"name" binding:"required"`
	Type string `json:"type" binding:"required"`
}

type DeviceResponse struct {
	ID               string                 `json:"id"`
	Name             string                 `json:"name"`
	Type             string                 `json:"type"`
	DeviceUID        string                 `json:"device_uid"` // Add DeviceUID
	PublicIP         string                 `json:"public_ip"`  // Add PublicIP
	LastSeen         time.Time              `json:"last_seen"`
	CreatedAt        time.Time              `json:"created_at"`
	Status           string                 `json:"status"`
	ShadowState      map[string]interface{} `json:"shadow_state"`      // Added ShadowState
	DesiredState     map[string]interface{} `json:"desired_state"`     // Added DesiredState
	ThingDescription map[string]interface{} `json:"thing_description"` // Thing Description
}

type DeviceStatsResponse struct {
	TotalDevices   int `json:"total_devices"`
	OnlineDevices  int `json:"online_devices"`
	OfflineDevices int `json:"offline_devices"`
}

// ============ Device Handlers ============

// getDeviceStatsHandler returns statistics about the user's devices
// GET /dev/stats
func getDeviceStatsHandler(c *gin.Context) {
	userID, _ := auth.GetUserID(c)
	role, _ := auth.GetUserRole(c)

	var total, online, offline int

	// Admins see stats for all devices, regular users see only theirs
	if role == "admin" {
		devices, err := store.GetAllDevices()
		if err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
			return
		}
		total = len(devices)
		for _, d := range devices {
			if time.Since(d.LastSeen) < 5*time.Minute {
				online++
			}
		}
		offline = total - online
	} else {
		stats, err := store.GetUserDeviceStats(userID)
		if err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
			return
		}
		total = stats.Total
		online = stats.Online
		offline = stats.Offline
	}

	c.JSON(http.StatusOK, DeviceStatsResponse{
		TotalDevices:   total,
		OnlineDevices:  online,
		OfflineDevices: offline,
	})
}

// createDeviceHandler creates a new device for the authenticated user
// POST /device
func createDeviceHandler(c *gin.Context) {
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

// updateDeviceConfigHandler updates the desired configuration
// PATCH /devices/:id/config
func updateDeviceConfigHandler(c *gin.Context) {
	deviceID := c.Param("device_id")
	userID, _ := auth.GetUserID(c)
	role, _ := auth.GetUserRole(c)

	// Check ownership
	device, err := store.GetDevice(deviceID)
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

	if err := store.UpdateDeviceConfig(deviceID, config); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	// MQTT Push
	if mqttBroker != nil {
		// Publish config delta to device
		payload := map[string]interface{}{
			"desired":   config,
			"timestamp": time.Now().Unix(),
		}
		jsonBytes, _ := json.Marshal(payload)
		// Topic: dev/{id}/conf/set
		topic := "dev/" + deviceID + "/conf/set"
		mqttBroker.Publish(topic, jsonBytes, false)
	}

	c.JSON(http.StatusOK, gin.H{"status": "updated", "config": config})
}

// getDeviceHandler gets a single device by ID
// GET /dev/:device_id
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
	isAuthorized := false

	if role == "admin" {
		isAuthorized = true
	} else if userID != "" && device.UserID == userID {
		isAuthorized = true
	} else if val, exists := c.Get("api_key"); exists {
		// Device Auth: Check to see if requester belongs to same user
		apiKey := val.(string)
		if requesterDevice, err := store.GetDeviceByAPIKey(apiKey); err == nil {
			// Allow if requester is the same device OR belongs to same user
			if requesterDevice.ID == device.ID || requesterDevice.UserID == device.UserID {
				isAuthorized = true
			}
		}
	}

	if !isAuthorized {
		c.JSON(http.StatusForbidden, gin.H{"error": "Unauthorized"})
		return
	}

	// Get latest shadow state
	shadowData, err := store.GetLatestData(deviceID)
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

// deleteDeviceHandler deletes a device
// DELETE /dev/:device_id
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

// updateDeviceThingDescriptionHandler updates the Thing Description of a device
// PUT /dev/:device_id/thing-description
func updateDeviceThingDescriptionHandler(c *gin.Context) {
	deviceID := c.Param("device_id")
	userID, _ := auth.GetUserID(c)
	role, _ := auth.GetUserRole(c)

	// Check for Device Authentication
	authDeviceID := ""
	if val, exists := c.Get("device_id"); exists {
		authDeviceID = val.(string)
	} else if val, exists := c.Get("api_key"); exists {
		// If middleware set api_key, resolve device
		apiKey := val.(string)
		if dev, err := store.GetDeviceByAPIKey(apiKey); err == nil {
			authDeviceID = dev.ID
		}
	}

	device, err := store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}

	// Authorization Check (Dual Support)
	isAuthorized := false

	// 1. Admin
	if role == "admin" {
		isAuthorized = true
	}
	// 2. Device Owner (User)
	if userID != "" && device.UserID == userID {
		isAuthorized = true
	}
	// 3. The Device Itself
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

	if err := store.UpdateDeviceThingDescription(deviceID, td); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update TD"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "Thing Description updated", "thing_description": td})
}
