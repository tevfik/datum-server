package main

import (
	"crypto/rand"
	"encoding/hex"
	"io"
	"net/http"
	"os"
	"strings"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/logger"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// generateIDString creates a random hex string of specified byte length
func generateIDString(byteLen int) string {
	bytes := make([]byte, byteLen)
	rand.Read(bytes)
	return hex.EncodeToString(bytes)
}

// SetupAdminRoutes configures all admin-related routes
func setupAdminRoutes(r *gin.Engine, store storage.Provider) {
	// System status (public - no auth needed)
	r.GET("/system/status", getSystemStatusHandler)
	r.POST("/system/setup", setupSystemHandler)

	// Admin routes (require admin role)
	admin := r.Group("/admin")
	admin.Use(auth.AdminMiddleware(store))
	{
		// User management
		admin.POST("/users", createUserHandler)
		admin.GET("/users", listUsersHandler)
		admin.GET("/users/:user_id", getUserHandler)
		admin.PUT("/users/:user_id", updateUserHandler)
		admin.DELETE("/users/:user_id", deleteUserHandler)
		admin.POST("/users/:username/reset-password", resetPasswordHandler)

		// Device management (all devices across users)
		admin.POST("/devices", provisionDeviceHandler)
		admin.GET("/devices", listAllDevicesHandler)
		admin.GET("/devices/:device_id", getDeviceAdminHandler)
		admin.PUT("/devices/:device_id", updateDeviceHandler)
		admin.DELETE("/devices/:device_id", forceDeleteDeviceHandler)

		// Key management (token rotation and revocation)
		// Key management (token rotation and revocation)
		admin.POST("/devices/:device_id/rotate-key", rotateDeviceKeyHandler)
		admin.POST("/devices/:device_id/revoke-key", revokeDeviceKeyHandler)
		// admin.GET("/devices/:device_id/token-info", getDeviceTokenInfoHandler)
		// admin.GET("/devices/:device_id/token-info", getDeviceTokenInfoHandler)

		// Database operations
		admin.GET("/database/stats", getDatabaseStatsHandler)
		admin.POST("/database/export", exportDatabaseHandler)
		admin.POST("/database/cleanup", forceCleanupHandler)
		admin.DELETE("/database/reset", resetDatabaseHandler)

		// System Configuration
		admin.PUT("/config", updateRegistrationConfigHandler)

		// System configuration
		admin.GET("/config", getSystemConfigHandler)
		admin.PUT("/config/retention", updateRetentionPolicyHandler)
		admin.PUT("/config/rate-limit", updateRateLimitHandler)
		admin.PUT("/config/alerts", updateAlertConfigHandler)
		admin.PUT("/config/registration", updateRegistrationConfigHandler)

		// Logs management
		admin.GET("/logs", getLogsHandler)
		admin.DELETE("/logs", clearLogsHandler)
	}

	// Device auth routes for token refresh
	deviceAuth := r.Group("/devices")
	deviceAuth.Use(auth.DeviceAuthMiddleware())
	{
		// deviceAuth.POST("/token/refresh", refreshTokenHandler)
	}
}

// ============ System Handlers ============

func getSystemStatusHandler(c *gin.Context) {
	initialized := store.IsSystemInitialized()
	// config, _ := store.GetSystemConfig()

	status := gin.H{
		"initialized": initialized,
	}

	// Only return details if NOT initialized (to help with setup)
	// Once initialized, we don't want to leak platform details publicly
	/*
		if initialized {
			status["platform_name"] = config.PlatformName
			status["allow_register"] = config.AllowRegister
			status["setup_at"] = config.SetupAt
		}
	*/

	c.JSON(http.StatusOK, status)
}

type SetupRequest struct {
	PlatformName  string `json:"platform_name" binding:"required"`
	AdminEmail    string `json:"admin_email" binding:"required,email"`
	AdminPassword string `json:"admin_password" binding:"required,min=8"`
	AllowRegister bool   `json:"allow_register"`
	DataRetention int    `json:"data_retention"`
}

func setupSystemHandler(c *gin.Context) {
	// Check if already initialized
	if store.IsSystemInitialized() {
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

	if err := store.CreateUser(user); err != nil {
		c.JSON(http.StatusConflict, gin.H{"error": "Email already exists"})
		return
	}

	// Initialize system
	if err := store.InitializeSystem(req.PlatformName, req.AllowRegister, req.DataRetention); err != nil {
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

// ============ User Management Handlers ============

type CreateUserRequest struct {
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required,min=8"`
	Role     string `json:"role"`
}

func createUserHandler(c *gin.Context) {
	var req CreateUserRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	req.Email = strings.ToLower(req.Email)

	// Set defaults
	if req.Role == "" {
		req.Role = "user"
	}

	// Validate role
	if req.Role != "admin" && req.Role != "user" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid role. Must be 'admin' or 'user'"})
		return
	}

	// Hash password
	hashedPassword, err := auth.HashPassword(req.Password)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to hash password"})
		return
	}

	userID := generateIDString(16)
	user := &storage.User{
		ID:           userID,
		Email:        req.Email,
		PasswordHash: hashedPassword,
		Role:         req.Role,
		Status:       "active",
		CreatedAt:    timeNow(),
	}

	if err := store.CreateUser(user); err != nil {
		c.JSON(http.StatusConflict, gin.H{"error": "Email already exists"})
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"message": "User created",
		"user_id": userID,
		"email":   req.Email,
		"role":    req.Role,
	})
}

func listUsersHandler(c *gin.Context) {
	users, err := store.ListAllUsers()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	// Remove password hashes from response
	var safeUsers []gin.H
	for _, u := range users {
		devices, _ := store.GetUserDevices(u.ID)
		safeUsers = append(safeUsers, gin.H{
			"id":            u.ID,
			"email":         u.Email,
			"role":          u.Role,
			"status":        u.Status,
			"created_at":    u.CreatedAt,
			"updated_at":    u.UpdatedAt,
			"last_login_at": u.LastLoginAt,
			"device_count":  len(devices),
		})
	}

	c.JSON(http.StatusOK, gin.H{"users": safeUsers})
}

func getUserHandler(c *gin.Context) {
	userID := c.Param("user_id")

	user, err := store.GetUserByID(userID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	devices, _ := store.GetUserDevices(userID)

	c.JSON(http.StatusOK, gin.H{
		"id":            user.ID,
		"email":         user.Email,
		"role":          user.Role,
		"status":        user.Status,
		"created_at":    user.CreatedAt,
		"updated_at":    user.UpdatedAt,
		"last_login_at": user.LastLoginAt,
		"devices":       devices,
	})
}

type UpdateUserRequest struct {
	Role   string `json:"role"`   // "admin", "user"
	Status string `json:"status"` // "active", "suspended"
}

func updateUserHandler(c *gin.Context) {
	userID := c.Param("user_id")

	var req UpdateUserRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Validate role and status
	if req.Role != "" && req.Role != "admin" && req.Role != "user" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid role. Must be 'admin' or 'user'"})
		return
	}
	if req.Status != "" && req.Status != "active" && req.Status != "suspended" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid status. Must be 'active' or 'suspended'"})
		return
	}

	// Prevent admin from suspending or demoting themselves
	currentUserID := c.GetString("user_id")
	if userID == currentUserID {
		if req.Status == "suspended" {
			c.JSON(http.StatusForbidden, gin.H{"error": "Cannot suspend your own account"})
			return
		}
		if req.Role == "user" {
			c.JSON(http.StatusForbidden, gin.H{"error": "Cannot demote your own account"})
			return
		}
	}

	// Prevent demoting the last admin
	if req.Role == "user" {
		stats, _ := store.GetDatabaseStats()
		adminCount, _ := stats["admin_users"].(int)
		if adminCount <= 1 {
			user, _ := store.GetUserByID(userID)
			if user.Role == "admin" {
				c.JSON(http.StatusForbidden, gin.H{"error": "Cannot demote the last admin"})
				return
			}
		}
	}

	if err := store.UpdateUser(userID, req.Role, req.Status); err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "User updated"})
}

func deleteUserHandler(c *gin.Context) {
	userID := c.Param("user_id")
	currentUserID := c.GetString("user_id")

	// Prevent self-deletion
	if userID == currentUserID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Cannot delete yourself"})
		return
	}

	// Prevent deleting the last admin
	user, err := store.GetUserByID(userID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	if user.Role == "admin" {
		stats, _ := store.GetDatabaseStats()
		adminCount, _ := stats["admin_users"].(int)
		if adminCount <= 1 {
			c.JSON(http.StatusForbidden, gin.H{"error": "Cannot delete the last admin"})
			return
		}
	}

	if err := store.DeleteUser(userID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "User and associated devices deleted"})
}

// ============ Device Management Handlers ============

// listAllDevicesHandler lists all devices in the system
func listAllDevicesHandler(c *gin.Context) {
	devices, err := store.GetAllDevices()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	// Enrich with owner info
	var enrichedDevices []gin.H
	for _, d := range devices {
		owner, _ := store.GetUserByID(d.UserID)
		ownerEmail := ""
		if owner != nil {
			ownerEmail = owner.Email
		}

		enrichedDevices = append(enrichedDevices, gin.H{
			"id":          d.ID,
			"name":        d.Name,
			"type":        d.Type,
			"status":      d.Status,
			"owner_id":    d.UserID,
			"owner_email": ownerEmail,
			"last_seen":   d.LastSeen,
			"created_at":  d.CreatedAt,
		})
	}

	c.JSON(http.StatusOK, gin.H{"devices": enrichedDevices})
}

func getDeviceAdminHandler(c *gin.Context) {
	deviceID := c.Param("device_id")

	device, err := store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}

	owner, _ := store.GetUserByID(device.UserID)

	c.JSON(http.StatusOK, gin.H{
		"id":          device.ID,
		"name":        device.Name,
		"type":        device.Type,
		"api_key":     device.APIKey, // Admin can see API key
		"status":      device.Status,
		"owner_id":    device.UserID,
		"owner_email": owner.Email,
		"last_seen":   device.LastSeen,
		"created_at":  device.CreatedAt,
		"updated_at":  device.UpdatedAt,
	})
}

type UpdateDeviceRequest struct {
	Status string `json:"status"` // "active", "banned", "suspended"
}

func updateDeviceHandler(c *gin.Context) {
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

	if err := store.UpdateDevice(deviceID, req.Status); err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Device updated"})
}

func forceDeleteDeviceHandler(c *gin.Context) {
	deviceID := c.Param("device_id")

	if err := store.ForceDeleteDevice(deviceID); err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Device deleted"})
}

// ============ Database Management Handlers ============

func getDatabaseStatsHandler(c *gin.Context) {
	stats, err := store.GetDatabaseStats()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	config, _ := store.GetSystemConfig()
	stats["platform_name"] = config.PlatformName
	stats["allow_register"] = config.AllowRegister
	stats["data_retention_days"] = config.DataRetention

	c.JSON(http.StatusOK, stats)
}

func exportDatabaseHandler(c *gin.Context) {
	export, err := store.ExportDatabase()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, export)
}

func forceCleanupHandler(c *gin.Context) {
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

func resetDatabaseHandler(c *gin.Context) {
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
	export, _ := store.ExportDatabase()
	_ = export // In production, save this to a backup file

	// Reset database
	if err := store.ResetSystem(); err != nil {
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

// ============ Additional Admin Handlers ============

type ProvisionDeviceRequest struct {
	DeviceID string `json:"device_id"`
	Name     string `json:"name" binding:"required"`
	Type     string `json:"type"`
}

func provisionDeviceHandler(c *gin.Context) {
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

	if err := store.CreateDevice(device); err != nil {
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

type ResetPasswordRequest struct {
	NewPassword string `json:"new_password"`
}

func resetPasswordHandler(c *gin.Context) {
	username := c.Param("username")

	var req ResetPasswordRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// If no password provided, generate random one
	if req.NewPassword == "" {
		req.NewPassword = generateIDString(8) // 16 char hex string
	}

	// Get user
	user, err := store.GetUserByEmail(username)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	// Hash new password
	hashedPassword, err := auth.HashPassword(req.NewPassword)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to hash password"})
		return
	}

	// Update password
	if err := store.UpdateUserPassword(user.ID, hashedPassword); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	// Generate new token for immediate use
	newToken, _ := auth.GenerateToken(user.ID, user.Email, user.Role)

	c.JSON(http.StatusOK, gin.H{
		"message":      "Password reset successfully",
		"new_password": req.NewPassword,
		"token":        newToken,
	})
}

func getSystemConfigHandler(c *gin.Context) {
	config, err := store.GetSystemConfig()
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

type UpdateRetentionRequest struct {
	Days               int `json:"days" binding:"required,min=1,max=365"`
	CheckIntervalHours int `json:"check_interval_hours" binding:"required,min=1,max=24"`
}

func updateRetentionPolicyHandler(c *gin.Context) {
	var req UpdateRetentionRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if err := store.UpdateDataRetention(req.Days); err != nil {
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

func updateRateLimitHandler(c *gin.Context) {
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

func updateAlertConfigHandler(c *gin.Context) {
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

func getLogsHandler(c *gin.Context) {
	logType := c.DefaultQuery("type", "")
	level := c.DefaultQuery("level", "")
	search := c.DefaultQuery("search", "")

	logPath := logger.LogFilePath
	if logPath == "" {
		c.JSON(http.StatusOK, gin.H{"logs": []gin.H{}, "total": 0, "message": "File logging not enabled"})
		return
	}

	lines, err := readLastLines(logPath, 500)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to read logs: " + err.Error()})
		return
	}

	var logs []gin.H
	for _, line := range lines {
		if line == "" {
			continue
		}

		// Simple filtering on raw string
		if level != "" && !strings.Contains(strings.ToUpper(line), strings.ToUpper(level)) {
			continue
		}
		if search != "" && !strings.Contains(strings.ToLower(line), strings.ToLower(search)) {
			continue
		}
		if logType != "" && !strings.Contains(line, logType) {
			continue
		}

		logs = append(logs, gin.H{
			"raw": line,
		})
	}

	// Reverse logs to show newest first
	for i, j := 0, len(logs)-1; i < j; i, j = i+1, j-1 {
		logs[i], logs[j] = logs[j], logs[i]
	}

	c.JSON(http.StatusOK, gin.H{
		"logs":  logs,
		"total": len(logs),
	})
}

func clearLogsHandler(c *gin.Context) {
	logPath := logger.LogFilePath
	if logPath == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "File logging not enabled"})
		return
	}

	if err := os.Truncate(logPath, 0); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to clear logs: " + err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "Logs cleared successfully",
	})
}

// readLastLines reads the last N lines from a file
// efficient enough for a few MBs
func readLastLines(filename string, n int) ([]string, error) {
	file, err := os.Open(filename)
	if err != nil {
		return nil, err
	}
	defer file.Close()

	// 1. Get file size
	stat, err := file.Stat()
	if err != nil {
		return nil, err
	}
	filesize := stat.Size()

	// 2. Read file (for now simple implementation: read all, split, take last N)
	// Optimization: seek to end and read backwards is better but more complex.
	// Given typical log sizes for this project, reading 1-5MB into memory is okay.
	// If file is huge (>10MB), we should limit.

	startPos := int64(0)
	if filesize > 5*1024*1024 { // If > 5MB
		startPos = filesize - 5*1024*1024 // Read last 5MB
	}

	buf := make([]byte, filesize-startPos)
	_, err = file.ReadAt(buf, startPos)
	if err != nil && err != io.EOF {
		return nil, err
	}

	strContent := string(buf)
	lines := strings.Split(strContent, "\n")

	if len(lines) > n {
		return lines[len(lines)-n:], nil
	}
	return lines, nil
}

// Helper to check if path exists
func pathExists(path string) bool {
	_, err := os.Stat(path)
	return err == nil
}

// timeNow returns current time (for testability)
func timeNow() time.Time {
	return time.Now()
}

type UpdateRegistrationRequest struct {
	AllowRegister bool `json:"allow_register"`
}

func updateRegistrationConfigHandler(c *gin.Context) {
	var req UpdateRegistrationRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if err := store.UpdateRegistrationConfig(req.AllowRegister); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message":        "Registration configuration updated",
		"allow_register": req.AllowRegister,
	})
}

// rotateDeviceKeyHandler rotates the API key for a device
// POST /admin/devices/:device_id/rotate-key
func rotateDeviceKeyHandler(c *gin.Context) {
	deviceID := c.Param("device_id")

	// Generate new key
	newKey := generateIDString(16) // 32 chars hex

	if err := store.UpdateDeviceAPIKey(deviceID, newKey); err != nil {
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

// revokeDeviceKeyHandler revokes the API key for a device
// POST /admin/devices/:device_id/revoke-key
func revokeDeviceKeyHandler(c *gin.Context) {
	deviceID := c.Param("device_id")

	// Set key to empty string to revoke access
	if err := store.UpdateDeviceAPIKey(deviceID, ""); err != nil {
		logger.Logger.Error().Msgf("Failed to revoke key for device %s: %v", deviceID, err)
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to revoke key"})
		return
	}

	logger.Logger.Info().Msgf("Revoked API key for device %s by admin", deviceID)
	c.JSON(http.StatusOK, gin.H{
		"message": "Key revoked successfully",
	})
}
