package handlers

import (
	"fmt"
	"net/http"
	"os"
	"path/filepath"

	"datum-go/internal/logger"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// ============ Device Management Handlers ============

type ProvisionDeviceRequest struct {
	DeviceID string `json:"device_id"`
	Name     string `json:"name" binding:"required"`
	Type     string `json:"type"`
}

func (h *AdminHandler) ProvisionDeviceHandler(c *gin.Context) {
	var req ProvisionDeviceRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Generate device ID if not provided
	deviceID := req.DeviceID
	if deviceID == "" {
		deviceID = "dev_" + generateIDString(6)
	}

	// Generate API key (dk_ + 16 hex chars = 19 chars total)
	apiKey := "dk_" + generateIDString(8)

	// Get admin user ID from context
	adminUserID := c.GetString("user_id")

	// Default type if not provided
	deviceType := req.Type
	if deviceType == "" {
		deviceType = "sensor"
	}

	// Create device
	device := &storage.Device{
		ID:        deviceID,
		UserID:    adminUserID,
		Name:      req.Name,
		Type:      deviceType,
		APIKey:    apiKey,
		Status:    "active",
		CreatedAt: timeNow(),
		UpdatedAt: timeNow(),
	}

	if err := h.Store.CreateDevice(device); err != nil {
		c.JSON(http.StatusConflict, gin.H{"error": "Device ID already exists"})
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"device_id":  deviceID,
		"name":       req.Name,
		"type":       deviceType,
		"api_key":    apiKey,
		"status":     "active",
		"created_at": device.CreatedAt,
	})
}

func (h *AdminHandler) ListAllDevicesHandler(c *gin.Context) {
	devices, err := h.Store.GetAllDevices()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	// Enrich with owner info
	var enrichedDevices []gin.H
	for _, d := range devices {
		owner, _ := h.Store.GetUserByID(d.UserID)
		ownerEmail := ""
		if owner != nil {
			ownerEmail = owner.Email
		}

		enrichedDevices = append(enrichedDevices, gin.H{
			"id":           d.ID,
			"name":         d.Name,
			"type":         d.Type,
			"status":       d.Status, // Admin status: active/suspended/banned
			"admin_status": d.Status, // Explicit field for frontend clarity
			"owner_id":     d.UserID,
			"owner_email":  ownerEmail,
			"last_seen":    d.LastSeen,
			"created_at":   d.CreatedAt,
		})
	}

	c.JSON(http.StatusOK, gin.H{"devices": enrichedDevices})
}

func (h *AdminHandler) GetDeviceAdminHandler(c *gin.Context) {
	deviceID := c.Param("device_id")

	device, err := h.Store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}

	owner, _ := h.Store.GetUserByID(device.UserID)
	ownerEmail := ""
	if owner != nil {
		ownerEmail = owner.Email
	}

	c.JSON(http.StatusOK, gin.H{
		"id":          device.ID,
		"name":        device.Name,
		"type":        device.Type,
		"api_key":     device.APIKey, // Admin can see API key
		"status":      device.Status,
		"owner_id":    device.UserID,
		"owner_email": ownerEmail,
		"last_seen":   device.LastSeen,
		"created_at":  device.CreatedAt,
		"updated_at":  device.UpdatedAt,
	})
}

type UpdateDeviceRequest struct {
	Status string `json:"status"` // "active", "banned", "suspended"
}

func (h *AdminHandler) UpdateDeviceHandler(c *gin.Context) {
	deviceID := c.Param("device_id")

	var req UpdateDeviceRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if req.Status != "active" && req.Status != "banned" && req.Status != "suspended" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid status. Must be 'active', 'banned', or 'suspended'"})
		return
	}

	if err := h.Store.UpdateDevice(deviceID, req.Status); err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Device updated"})
}

func (h *AdminHandler) ForceDeleteDeviceHandler(c *gin.Context) {
	deviceID := c.Param("device_id")

	if err := h.Store.ForceDeleteDevice(deviceID); err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Device deleted"})
}

func (h *AdminHandler) RotateDeviceKeyHandler(c *gin.Context) {
	deviceID := c.Param("device_id")

	// Generate new key
	newKey := generateIDString(16) // 32 chars hex

	if err := h.Store.UpdateDeviceAPIKey(deviceID, newKey); err != nil {
		logger.Logger.Error().Msgf("Failed to rotate key for device %s: %v", deviceID, err)
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to rotate key"})
		return
	}

	logger.Logger.Info().Msgf("Rotated API key for device %s by admin", deviceID)
	c.JSON(http.StatusOK, gin.H{
		"message": "Key rotated successfully",
		"api_key": newKey,
	})
}

func (h *AdminHandler) RevokeDeviceKeyHandler(c *gin.Context) {
	deviceID := c.Param("device_id")

	// Set key to empty string to revoke access
	if err := h.Store.UpdateDeviceAPIKey(deviceID, ""); err != nil {
		logger.Logger.Error().Msgf("Failed to revoke key for device %s: %v", deviceID, err)
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to revoke key"})
		return
	}

	logger.Logger.Info().Msgf("Revoked API key for device %s by admin", deviceID)
	c.JSON(http.StatusOK, gin.H{
		"message": "Key revoked successfully",
	})
}

func (h *AdminHandler) UploadFirmwareHandler(c *gin.Context) {
	file, err := c.FormFile("firmware")
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "No firmware file uploaded"})
		return
	}

	// Validate extension
	ext := filepath.Ext(file.Filename)
	if ext != ".bin" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Only .bin files are allowed"})
		return
	}

	// Create firmware directory if not exists
	firmwareDir := "./firmware"
	if err := os.MkdirAll(firmwareDir, 0755); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to create firmware directory"})
		return
	}

	// Sanitize filename
	safeFilename := filepath.Base(file.Filename)
	dst := filepath.Join(firmwareDir, safeFilename)

	// Remove existing file if present to ensure clean overwrite
	if _, err := os.Stat(dst); err == nil {
		if err := os.Remove(dst); err != nil {
			logger.Logger.Warn().Err(err).Msgf("Failed to delete existing firmware file: %s", dst)
			// Continue anyway, SaveUploadedFile might overwrite (truncates)
		} else {
			logger.Logger.Info().Msgf("Deleted existing firmware file: %s", dst)
		}
	}

	if err := c.SaveUploadedFile(file, dst); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to save firmware file"})
		return
	}

	scheme := "http"
	if c.Request.TLS != nil || c.Request.Header.Get("X-Forwarded-Proto") == "https" {
		scheme = "https"
	}
	host := c.Request.Host
	publicURL := fmt.Sprintf("%s://%s/dev/fw/%s", scheme, host, safeFilename)

	c.JSON(http.StatusCreated, gin.H{
		"message":  "Firmware uploaded successfully",
		"filename": safeFilename,
		"url":      publicURL,
	})
}
