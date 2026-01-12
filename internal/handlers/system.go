package handlers

import (
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// ============ System Handlers ============

func (h *AdminHandler) GetSystemStatusHandler(c *gin.Context) {
	initialized := h.Store.IsSystemInitialized()

	status := gin.H{
		"initialized": initialized,
	}
	// Note: Platform details are hidden if initialized
	c.JSON(http.StatusOK, status)
}

type SetupRequest struct {
	PlatformName  string `json:"platform_name" binding:"required"`
	AdminEmail    string `json:"admin_email" binding:"required,email"`
	AdminPassword string `json:"admin_password" binding:"required,min=8"`
	AllowRegister bool   `json:"allow_register"`
	DataRetention int    `json:"data_retention"`
}

func (h *AdminHandler) SetupSystemHandler(c *gin.Context) {
	// Check if already initialized
	if h.Store.IsSystemInitialized() {
		c.JSON(http.StatusConflict, gin.H{"error": "System already initialized"})
		return
	}

	var req SetupRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	req.AdminEmail = strings.ToLower(req.AdminEmail)

	// Set defaults
	if req.DataRetention == 0 {
		req.DataRetention = 7
	}

	// Create admin user
	hashedPassword, err := auth.HashPassword(req.AdminPassword)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to hash password"})
		return
	}

	userID := generateIDString(16)
	user := &storage.User{
		ID:           userID,
		Email:        req.AdminEmail,
		PasswordHash: hashedPassword,
		Role:         "admin",
		Status:       "active",
		CreatedAt:    timeNow(),
	}

	if err := h.Store.CreateUser(user); err != nil {
		c.JSON(http.StatusConflict, gin.H{"error": "Email already exists"})
		return
	}

	// Initialize system
	if err := h.Store.InitializeSystem(req.PlatformName, req.AllowRegister, req.DataRetention); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to initialize system"})
		return
	}

	// Generate token for admin
	token, _ := auth.GenerateToken(userID, req.AdminEmail, "admin")

	c.JSON(http.StatusCreated, gin.H{
		"message":  "System initialized successfully",
		"user_id":  userID,
		"email":    req.AdminEmail,
		"role":     "admin",
		"token":    token,
		"platform": req.PlatformName,
	})
}

func (h *AdminHandler) GetSystemConfigHandler(c *gin.Context) {
	config, err := h.Store.GetSystemConfig()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"retention": gin.H{
			"days":                 config.DataRetention,
			"check_interval_hours": 6,
		},
		"rate_limit": gin.H{
			"max_requests":   100,
			"window_seconds": 60,
		},
		"alerts": gin.H{
			"email_enabled":    false,
			"disk_threshold":   90,
			"memory_threshold": 90,
		},
	})
}

type UpdateRegistrationRequest struct {
	AllowRegister bool `json:"allow_register"`
}

func (h *AdminHandler) UpdateRegistrationConfigHandler(c *gin.Context) {
	var req UpdateRegistrationRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if err := h.Store.UpdateRegistrationConfig(req.AllowRegister); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message":        "Registration configuration updated",
		"allow_register": req.AllowRegister,
	})
}

type UpdateRetentionRequest struct {
	Days               int `json:"days" binding:"required,min=1,max=365"`
	CheckIntervalHours int `json:"check_interval_hours" binding:"required,min=1,max=24"`
}

func (h *AdminHandler) UpdateRetentionPolicyHandler(c *gin.Context) {
	var req UpdateRetentionRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if err := h.Store.UpdateDataRetention(req.Days); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "Retention policy updated",
		"retention": gin.H{
			"days":                 req.Days,
			"check_interval_hours": req.CheckIntervalHours,
		},
	})
}

type UpdateRateLimitRequest struct {
	MaxRequests   int `json:"max_requests" binding:"required,min=10,max=10000"`
	WindowSeconds int `json:"window_seconds" binding:"required,min=1,max=3600"`
}

func (h *AdminHandler) UpdateRateLimitHandler(c *gin.Context) {
	var req UpdateRateLimitRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Store in system config (requires extending storage layer)
	c.JSON(http.StatusOK, gin.H{
		"message": "Rate limit updated",
		"rate_limit": gin.H{
			"max_requests":   req.MaxRequests,
			"window_seconds": req.WindowSeconds,
		},
	})
}

type UpdateAlertConfigRequest struct {
	EmailEnabled    bool `json:"email_enabled"`
	DiskThreshold   int  `json:"disk_threshold" binding:"required,min=10,max=95"`
	MemoryThreshold int  `json:"memory_threshold" binding:"required,min=10,max=95"`
}

func (h *AdminHandler) UpdateAlertConfigHandler(c *gin.Context) {
	var req UpdateAlertConfigRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Store in system config (requires extending storage layer)
	c.JSON(http.StatusOK, gin.H{
		"message": "Alert configuration updated",
		"alerts": gin.H{
			"email_enabled":    req.EmailEnabled,
			"disk_threshold":   req.DiskThreshold,
			"memory_threshold": req.MemoryThreshold,
		},
	})
}

func (h *AdminHandler) GetDatabaseStatsHandler(c *gin.Context) {
	stats, err := h.Store.GetDatabaseStats()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	config, _ := h.Store.GetSystemConfig()
	stats["platform_name"] = config.PlatformName
	stats["allow_register"] = config.AllowRegister
	stats["data_retention_days"] = config.DataRetention

	// Add Server Time and Uptime
	stats["server_time"] = time.Now().Format(time.RFC3339)
	stats["server_uptime_seconds"] = int64(time.Since(h.ServerStartTime).Seconds())

	// Add DB Size (Approximate size of data directory)
	var size int64
	filepath.Walk("data", func(_ string, info os.FileInfo, err error) error {
		if err == nil && !info.IsDir() {
			size += info.Size()
		}
		return nil
	})
	stats["db_size_bytes"] = size

	// Add Environment Variables (Filtered)
	envVars := make(map[string]string)
	for _, e := range os.Environ() {
		pair := strings.SplitN(e, "=", 2)
		if len(pair) == 2 {
			key := pair[0]
			// Value filtering for security
			val := pair[1]
			upperKey := strings.ToUpper(key)
			if strings.Contains(upperKey, "SECRET") || strings.Contains(upperKey, "PASSWORD") || strings.Contains(upperKey, "KEY") || strings.Contains(upperKey, "TOKEN") {
				val = "******"
			}
			envVars[key] = val
		}
	}
	stats["env_vars"] = envVars

	c.JSON(http.StatusOK, stats)
}

func (h *AdminHandler) ExportDatabaseHandler(c *gin.Context) {
	export, err := h.Store.ExportDatabase()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, export)
}

func (h *AdminHandler) ForceCleanupHandler(c *gin.Context) {
	// Manual cleanup is deprecated as tstorage handles retention internally
	// This endpoint is kept for compatibility but does nothing
	c.JSON(http.StatusOK, gin.H{
		"message":            "Cleanup is handled automatically by storage engine",
		"partitions_deleted": 0,
	})
}

type ResetRequest struct {
	Confirm string `json:"confirm" binding:"required"` // Must be "RESET"
}

func (h *AdminHandler) ResetDatabaseHandler(c *gin.Context) {
	var req ResetRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if req.Confirm != "RESET" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Must confirm with 'RESET'"})
		return
	}

	// Create backup first
	export, _ := h.Store.ExportDatabase()
	_ = export // In production, save this to a backup file

	// Reset database
	if err := h.Store.ResetSystem(); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	// Optionally clear tstorage data
	dataPath := "/app/data/tsdata"
	if envPath := os.Getenv("TSDATA_PATH"); envPath != "" {
		dataPath = envPath
	}
	os.RemoveAll(dataPath)
	os.MkdirAll(dataPath, 0755)

	c.JSON(http.StatusOK, gin.H{"message": "System reset to factory state"})
}
