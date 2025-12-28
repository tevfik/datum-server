package main

import (
	"crypto/rand"
	"encoding/hex"
	"flag"
	"fmt"
	"net/http"
	"os"
	"strconv"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/logger"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

var store *storage.Storage

// Security headers middleware
func securityHeadersMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		// Prevent clickjacking
		c.Header("X-Frame-Options", "DENY")
		// Prevent MIME type sniffing
		c.Header("X-Content-Type-Options", "nosniff")
		// Enable XSS protection
		c.Header("X-XSS-Protection", "1; mode=block")
		// Strict transport security (HTTPS only - uncomment in production with HTTPS)
		// c.Header("Strict-Transport-Security", "max-age=31536000; includeSubDomains")
		// Referrer policy
		c.Header("Referrer-Policy", "strict-origin-when-cross-origin")
		// Content security policy (basic)
		c.Header("Content-Security-Policy", "default-src 'self'")
		c.Next()
	}
}

func main() {
	// Command-line flags
	port := flag.String("port", "", "Server port (default: 8000 or PORT env var)")
	dataDir := flag.String("data-dir", "", "Data directory path (default: ./data or DATA_DIR env var)")
	retentionDays := flag.Int("retention-days", 0, "Data retention in days (default: 7 or RETENTION_MAX_DAYS env var)")
	retentionCheckHours := flag.Int("retention-check-hours", 0, "Retention check interval in hours (default: 24 or RETENTION_CHECK_HOURS env var)")
	showVersion := flag.Bool("version", false, "Show version information")
	flag.Parse()

	// Show version and exit
	if *showVersion {
		fmt.Println("Datum IoT Platform Server")
		fmt.Println("Version: 1.0.0")
		fmt.Println("Build: 2025-12-25")
		os.Exit(0)
	}

	// Initialize structured logging
	logger.InitLogger()
	log := logger.GetLogger()

	// Get data directory (priority: flag > env > default)
	dataDirPath := *dataDir
	if dataDirPath == "" {
		dataDirPath = os.Getenv("DATA_DIR")
		if dataDirPath == "" {
			dataDirPath = "./data"
		}
	}

	// Initialize storage (BuntDB for metadata, tstorage for time-series)
	var err error
	store, err = storage.New(dataDirPath+"/meta.db", dataDirPath+"/tsdata")
	if err != nil {
		log.Fatal().Err(err).Msg("Failed to initialize storage")
	}
	defer store.Close()

	log.Info().
		Str("metadata", "BuntDB").
		Str("timeseries", "tstorage").
		Str("data_dir", dataDirPath).
		Msg("Storage initialized")

	// Configure retention (priority: flag > env > default)
	retentionConfig := storage.GetRetentionConfigFromEnv()
	if *retentionDays > 0 {
		retentionConfig.MaxAge = time.Duration(*retentionDays) * 24 * time.Hour
	}
	if *retentionCheckHours > 0 {
		retentionConfig.CleanupEvery = time.Duration(*retentionCheckHours) * time.Hour
	}

	// Start retention worker (cleanup old data)
	store.StartRetentionWorker(retentionConfig, dataDirPath+"/tsdata")
	log.Info().
		Dur("max_age", retentionConfig.MaxAge).
		Dur("check_interval", retentionConfig.CleanupEvery).
		Msg("Data retention worker started")

	// Setup router
	gin.SetMode(gin.ReleaseMode)
	r := gin.New()
	r.Use(gin.Recovery())

	// Metrics tracking
	r.Use(metricsMiddleware())

	// Rate limiting
	r.Use(auth.RateLimitMiddleware())

	// Security headers
	r.Use(securityHeadersMiddleware())

	// CORS (configurable via CORS_ORIGINS env var)
	corsOrigins := os.Getenv("CORS_ORIGINS")
	if corsOrigins == "" {
		corsOrigins = "*" // Default: allow all (dev mode)
	}
	r.Use(func(c *gin.Context) {
		c.Writer.Header().Set("Access-Control-Allow-Origin", corsOrigins)
		c.Writer.Header().Set("Access-Control-Allow-Methods", "POST, GET, OPTIONS, DELETE, PUT")
		c.Writer.Header().Set("Access-Control-Allow-Headers", "Content-Type, Authorization")
		if c.Request.Method == "OPTIONS" {
			c.AbortWithStatus(204)
			return
		}
		c.Next()
	})

	// Admin routes (system setup, user/device/db management)
	setupAdminRoutes(r, store)

	// Provisioning routes (WiFi AP mode device setup)
	RegisterProvisioningRoutes(r, auth.AuthMiddleware())

	// Root endpoint
	r.GET("/", rootHandler)

	// Health check endpoints
	r.GET("/health", healthHandler)
	r.GET("/health/live", livenessHandler)
	r.GET("/health/ready", readinessHandler)

	// Metrics endpoint
	r.GET("/metrics", metricsHandler)

	// Auth routes
	authGroup := r.Group("/auth")
	{
		authGroup.POST("/register", registerHandler)
		authGroup.POST("/login", loginHandler)
	}

	// Device management routes (require user auth)
	devicesGroup := r.Group("/devices")
	devicesGroup.Use(auth.AuthMiddleware())
	{
		devicesGroup.POST("", createDeviceHandler)
		devicesGroup.GET("", listDevicesHandler)
		devicesGroup.DELETE("/:device_id", deleteDeviceHandler)

		// Command endpoints (user sends commands)
		devicesGroup.POST("/:device_id/commands", sendCommandHandler)
		devicesGroup.GET("/:device_id/commands", listCommandsHandler)

		// Provisioning endpoints (mobile app)
		devicesGroup.POST("/register", registerDeviceHandler)
		devicesGroup.GET("/check", checkDeviceUIDHandler)
		devicesGroup.GET("/provisioning", listProvisioningRequestsHandler)
		devicesGroup.GET("/provisioning/:request_id", getProvisioningStatusHandler)
		devicesGroup.DELETE("/provisioning/:request_id", cancelProvisioningHandler)
	}

	// Device command polling (device auth)
	deviceCommandGroup := r.Group("/device")
	deviceCommandGroup.Use(auth.DeviceAuthMiddleware())
	{
		deviceCommandGroup.GET("/:device_id/commands", pollCommandsHandler)
		deviceCommandGroup.GET("/:device_id/commands/stream", sseCommandsHandler) // SSE long polling
		deviceCommandGroup.GET("/:device_id/commands/poll", webhookPollHandler)   // HTTP long polling
		deviceCommandGroup.POST("/:device_id/commands/:command_id/ack", ackCommandHandler)
		deviceCommandGroup.GET("/:device_id/push", pushDataViaGetHandler) // For constrained devices (GET data push)
	}

	// Data routes (require device auth)
	dataGroup := r.Group("/data")
	dataGroup.Use(auth.DeviceAuthMiddleware())
	{
		dataGroup.POST("/:device_id", postDataHandler)
	}

	// Data query routes (require user auth)
	dataQueryGroup := r.Group("/data")
	dataQueryGroup.Use(auth.AuthMiddleware())
	{
		dataQueryGroup.GET("/:device_id", getLatestDataHandler)
		dataQueryGroup.GET("/:device_id/history", getDataHistoryHandler)
	}

	// Public data endpoint (NO authentication - for quick prototyping)
	r.POST("/public/data/:device_id", postPublicDataHandler)
	r.GET("/public/data/:device_id", getPublicDataHandler)
	r.GET("/public/data/:device_id/history", getPublicDataHistoryHandler)

	// Provisioning endpoints (device-side, NO authentication - called by unconfigured devices)
	r.POST("/provisioning/activate/:request_id", deviceActivateHandler)
	r.GET("/provisioning/check/:uid", deviceCheckHandler)

	// Swagger UI documentation
	setupSwagger(r)

	// Get port (priority: flag > env > default)
	serverPort := *port
	if serverPort == "" {
		serverPort = os.Getenv("PORT")
		if serverPort == "" {
			serverPort = "8000"
		}
	}

	log.Info().
		Str("port", serverPort).
		Str("endpoints", "/auth, /devices, /data, /public/data, /provisioning").
		Str("docs", "http://localhost:"+serverPort+"/docs").
		Msg("🚀 Datum IoT Platform starting")

	if err := r.Run(":" + serverPort); err != nil {
		log.Fatal().Err(err).Msg("Server failed to start")
	}
}

func rootHandler(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"service": "Datumpy IoT Platform",
		"version": "1.0.0",
		"endpoints": gin.H{
			"auth":         []string{"POST /auth/register", "POST /auth/login"},
			"devices":      []string{"POST /devices", "GET /devices", "DELETE /devices/{id}"},
			"commands":     []string{"POST /devices/{id}/commands", "GET /device/{id}/commands"},
			"data":         []string{"POST /data/{id}", "GET /data/{id}", "GET /data/{id}/history"},
			"public":       []string{"POST /public/data/{id}", "GET /public/data/{id}"},
			"provisioning": []string{"POST /devices/register", "GET /devices/provisioning", "POST /provisioning/activate/{id}"},
		},
	})
}

// Health check handlers
func healthHandler(c *gin.Context) {
	// Basic health check
	status := gin.H{
		"status":    "healthy",
		"timestamp": time.Now().Unix(),
		"service":   "datumpy-api",
		"version":   "1.0.0",
	}

	// Check storage connection
	if store == nil {
		status["status"] = "unhealthy"
		status["storage"] = "disconnected"
		c.JSON(http.StatusServiceUnavailable, status)
		return
	}

	status["storage"] = "connected"
	c.JSON(http.StatusOK, status)
}

func livenessHandler(c *gin.Context) {
	// Simple liveness check - is the service running?
	c.JSON(http.StatusOK, gin.H{
		"alive": true,
	})
}

func readinessHandler(c *gin.Context) {
	// Readiness check - is the service ready to handle requests?
	ready := true
	issues := []string{}

	// Check storage
	if store == nil {
		ready = false
		issues = append(issues, "storage not initialized")
	}

	if ready {
		c.JSON(http.StatusOK, gin.H{
			"ready": true,
		})
	} else {
		c.JSON(http.StatusServiceUnavailable, gin.H{
			"ready":  false,
			"issues": issues,
		})
	}
}

// Auth handlers

type RegisterRequest struct {
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required,min=8"`
}

func registerHandler(c *gin.Context) {
	// Check if system is initialized
	if !store.IsSystemInitialized() {
		c.JSON(http.StatusForbidden, gin.H{
			"error":          "System not initialized. Please complete setup first.",
			"setup_required": true,
		})
		return
	}

	// Check if registration is allowed
	config, _ := store.GetSystemConfig()
	if !config.AllowRegister {
		c.JSON(http.StatusForbidden, gin.H{
			"error": "Public registration is disabled. Contact administrator.",
		})
		return
	}

	var req RegisterRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	hashedPassword, err := auth.HashPassword(req.Password)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to hash password"})
		return
	}

	userID := generateID("usr")
	user := &storage.User{
		ID:           userID,
		Email:        req.Email,
		PasswordHash: hashedPassword,
		Role:         "user",   // Default role for registered users
		Status:       "active", // Active by default
		CreatedAt:    time.Now(),
	}

	if err := store.CreateUser(user); err != nil {
		c.JSON(http.StatusConflict, gin.H{"error": "User already exists"})
		return
	}

	token, err := auth.GenerateToken(userID, req.Email)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate token"})
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"user_id": userID,
		"token":   token,
		"role":    "user",
	})
}

type LoginRequest struct {
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required"`
}

func loginHandler(c *gin.Context) {
	var req LoginRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	user, err := store.GetUserByEmail(req.Email)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid credentials"})
		return
	}

	// Check if user is suspended
	if user.Status == "suspended" {
		c.JSON(http.StatusForbidden, gin.H{"error": "Account suspended. Contact administrator."})
		return
	}

	if !auth.CheckPassword(user.PasswordHash, req.Password) {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid credentials"})
		return
	}

	// Update last login
	store.UpdateUserLastLogin(user.ID)

	token, err := auth.GenerateToken(user.ID, user.Email)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate token"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"token":      token,
		"user_id":    user.ID,
		"email":      user.Email,
		"role":       user.Role,
		"expires_at": time.Now().Add(24 * time.Hour).Format(time.RFC3339),
	})
}

// Device handlers

type CreateDeviceRequest struct {
	Name string `json:"name" binding:"required"`
	Type string `json:"type" binding:"required"`
}

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

func listDevicesHandler(c *gin.Context) {
	userID, _ := auth.GetUserID(c)

	devices, err := store.GetUserDevices(userID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	type DeviceResponse struct {
		ID        string    `json:"id"`
		Name      string    `json:"name"`
		Type      string    `json:"type"`
		LastSeen  time.Time `json:"last_seen"`
		CreatedAt time.Time `json:"created_at"`
		Status    string    `json:"status"`
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
			LastSeen:  d.LastSeen,
			CreatedAt: d.CreatedAt,
			Status:    status,
		})
	}

	c.JSON(http.StatusOK, gin.H{"devices": response})
}

// Data handlers

func postDataHandler(c *gin.Context) {
	deviceID := c.Param("device_id")
	apiKey, _ := c.Get("api_key")

	device, err := store.GetDeviceByAPIKey(apiKey.(string))
	if err != nil || device.ID != deviceID {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid device credentials"})
		return
	}

	var data map[string]interface{}
	if err := c.ShouldBindJSON(&data); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	point := &storage.DataPoint{
		DeviceID:  deviceID,
		Timestamp: time.Now(),
		Data:      data,
	}

	if err := store.StoreData(point); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	// Check for pending commands
	commandsPending := store.GetPendingCommandCount(deviceID)

	c.JSON(http.StatusOK, gin.H{
		"status":           "ok",
		"timestamp":        point.Timestamp.Format(time.RFC3339),
		"commands_pending": commandsPending,
	})
}

func getLatestDataHandler(c *gin.Context) {
	deviceID := c.Param("device_id")
	userID, _ := auth.GetUserID(c)

	device, err := store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}
	if device.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Access denied"})
		return
	}

	point, err := store.GetLatestData(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "No data found"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"device_id": deviceID,
		"timestamp": point.Timestamp.Format(time.RFC3339),
		"data":      point.Data,
	})
}

func getDataHistoryHandler(c *gin.Context) {
	deviceID := c.Param("device_id")
	userID, _ := auth.GetUserID(c)

	device, err := store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}
	if device.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Access denied"})
		return
	}

	limit := 1000
	if limitStr := c.Query("limit"); limitStr != "" {
		fmt.Sscanf(limitStr, "%d", &limit)
	}

	end := time.Now()
	start := end.Add(-7 * 24 * time.Hour) // Default 7 days
	interval := time.Duration(0)          // No aggregation by default

	// Parse duration helper
	parseDuration := func(s string) time.Duration {
		switch s {
		case "1s":
			return time.Second
		case "1m":
			return time.Minute
		case "1h":
			return time.Hour
		case "1d":
			return 24 * time.Hour
		case "7d":
			return 7 * 24 * time.Hour
		case "30d":
			return 30 * 24 * time.Hour
		case "24h":
			return 24 * time.Hour
		default:
			return 0
		}
	}

	// Method 1: start (unix_ms) + stop (duration)
	if startMs := c.Query("start"); startMs != "" {
		if ms, err := strconv.ParseInt(startMs, 10, 64); err == nil {
			start = time.UnixMilli(ms)
		}
	}

	if stopStr := c.Query("stop"); stopStr != "" {
		if dur := parseDuration(stopStr); dur > 0 {
			end = start.Add(dur)
		}
	}

	// Method 2: range shorthand (1h, 24h, 7d, 30d) - backwards from now
	if rangeStr := c.Query("range"); rangeStr != "" {
		if dur := parseDuration(rangeStr); dur > 0 {
			end = time.Now()
			start = end.Add(-dur)
		}
	}

	// Method 3: RFC3339 start/end
	if startRFC := c.Query("start_rfc"); startRFC != "" {
		if t, err := time.Parse(time.RFC3339, startRFC); err == nil {
			start = t
		}
	}
	if endRFC := c.Query("end_rfc"); endRFC != "" {
		if t, err := time.Parse(time.RFC3339, endRFC); err == nil {
			end = t
		}
	}

	// Parse interval for aggregation
	if intStr := c.Query("int"); intStr != "" {
		interval = parseDuration(intStr)
	}

	// Get data
	points, err := store.GetDataHistoryWithRange(deviceID, start, end, limit)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	type DataResponse struct {
		Timestamp   string                 `json:"timestamp"`
		TimestampMs int64                  `json:"timestamp_ms"`
		Data        map[string]interface{} `json:"data"`
	}

	var response []DataResponse

	// If interval is specified, aggregate data
	if interval > 0 && len(points) > 0 {
		// Group by interval buckets
		buckets := make(map[int64][]storage.DataPoint)

		for _, p := range points {
			bucket := p.Timestamp.UnixMilli() / int64(interval/time.Millisecond) * int64(interval/time.Millisecond)
			buckets[bucket] = append(buckets[bucket], p)
		}

		// Compute averages for each bucket
		var bucketKeys []int64
		for k := range buckets {
			bucketKeys = append(bucketKeys, k)
		}
		// Sort buckets descending
		for i := 0; i < len(bucketKeys)-1; i++ {
			for j := i + 1; j < len(bucketKeys); j++ {
				if bucketKeys[i] < bucketKeys[j] {
					bucketKeys[i], bucketKeys[j] = bucketKeys[j], bucketKeys[i]
				}
			}
		}

		for _, bucket := range bucketKeys {
			pts := buckets[bucket]
			avgData := make(map[string]interface{})
			fieldSums := make(map[string]float64)
			fieldCounts := make(map[string]int)

			for _, p := range pts {
				for k, v := range p.Data {
					if fv, ok := v.(float64); ok {
						fieldSums[k] += fv
						fieldCounts[k]++
					}
				}
			}

			for k, sum := range fieldSums {
				avgData[k] = sum / float64(fieldCounts[k])
			}

			response = append(response, DataResponse{
				Timestamp:   time.UnixMilli(bucket).Format(time.RFC3339),
				TimestampMs: bucket,
				Data:        avgData,
			})
		}
	} else {
		for _, p := range points {
			response = append(response, DataResponse{
				Timestamp:   p.Timestamp.Format(time.RFC3339),
				TimestampMs: p.Timestamp.UnixMilli(),
				Data:        p.Data,
			})
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"device_id": deviceID,
		"count":     len(response),
		"data":      response,
		"range": gin.H{
			"start":    start.Format(time.RFC3339),
			"start_ms": start.UnixMilli(),
			"end":      end.Format(time.RFC3339),
			"end_ms":   end.UnixMilli(),
		},
		"interval": interval.String(),
	})
}

// pushDataViaGetHandler allows constrained devices to push data via GET request
// using query parameters instead of JSON body
// Example: GET /data/dev_xxx/push?temp=25.5&humidity=60&battery=85
func pushDataViaGetHandler(c *gin.Context) {
	deviceID := c.Param("device_id")
	apiKey, exists := c.Get("api_key")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid device credentials"})
		return
	}

	device, err := store.GetDeviceByAPIKey(apiKey.(string))
	if err != nil || device.ID != deviceID {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid device credentials"})
		return
	}

	// Check if device is banned
	if device.Status == "banned" {
		c.JSON(http.StatusForbidden, gin.H{"error": "Device is banned"})
		return
	}

	// Parse query parameters into data map
	data := make(map[string]interface{})
	for key, values := range c.Request.URL.Query() {
		if len(values) > 0 && key != "key" {
			// Try to parse as number first
			if floatVal, err := strconv.ParseFloat(values[0], 64); err == nil {
				data[key] = floatVal
			} else if intVal, err := strconv.ParseInt(values[0], 10, 64); err == nil {
				data[key] = intVal
			} else if boolVal, err := strconv.ParseBool(values[0]); err == nil {
				data[key] = boolVal
			} else {
				data[key] = values[0]
			}
		}
	}

	if len(data) == 0 {
		c.JSON(http.StatusBadRequest, gin.H{"error": "No data parameters provided"})
		return
	}

	point := &storage.DataPoint{
		DeviceID:  deviceID,
		Timestamp: time.Now(),
		Data:      data,
	}

	if err := store.StoreData(point); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	// Check for pending commands
	commandsPending := store.GetPendingCommandCount(deviceID)

	c.JSON(http.StatusOK, gin.H{
		"status":           "ok",
		"timestamp":        point.Timestamp.Format(time.RFC3339),
		"fields_stored":    len(data),
		"commands_pending": commandsPending,
	})
}

// Utility functions

func generateID(prefix string) string {
	bytes := make([]byte, 8)
	rand.Read(bytes)
	return prefix + "_" + hex.EncodeToString(bytes)
}
