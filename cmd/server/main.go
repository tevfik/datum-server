package main

import (
	"context"
	"crypto/rand"
	"embed"
	"encoding/hex"
	"flag"
	"fmt"
	"io/fs"
	"net/http"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/email"
	"datum-go/internal/logger"
	mqtt_internal "datum-go/internal/mqtt"
	"datum-go/internal/processing"
	"datum-go/internal/storage"
	"datum-go/internal/storage/postgres"

	"github.com/gin-gonic/gin"
	"github.com/joho/godotenv"
)

//go:embed dist/*
var webFS embed.FS

var (
	Version   = "1.1.0"
	BuildDate = "unknown"
)

var (
	store              storage.Provider
	emailService       *email.EmailSender
	telemetryProcessor *processing.TelemetryProcessor
	mqttBroker         *mqtt_internal.Broker
)

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
		c.Header("Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline' https://unpkg.com; style-src 'self' 'unsafe-inline' https://unpkg.com; img-src 'self' data:;")
		c.Next()
	}
}

func main() {
	// Load environment variables from .env file if it exists
	if err := godotenv.Load(); err != nil {
		// Just log, don't fail, as env vars might be set in the environment (e.g. Docker)
		// Use fmt here since logger is not initialized yet
		// fmt.Println("No .env file found or error loading it")
	}

	// Ensure we log if main exits unexpectedly
	defer func() {
		if r := recover(); r != nil {
			fmt.Fprintf(os.Stderr, "PANIC: %v\n", r)
		}
		fmt.Fprintf(os.Stderr, "DEBUG: Main function exiting\n")
	}()

	// Command-line flags
	port := flag.String("port", "", "Server port (default: 8000 or PORT env var)")
	serverURL := flag.String("server-url", "", "Public server URL (default: http://localhost:8000 or SERVER_URL env var)")
	dataDir := flag.String("data-dir", "", "Data directory path (default: ./data or DATA_DIR env var)")
	retentionDays := flag.Int("retention-days", 0, "Data retention in days (default: 7 or RETENTION_MAX_DAYS env var)")
	// retentionCheckHours is deprecated as retention is handled by storage engine
	_ = flag.Int("retention-check-hours", 0, "DEPRECATED: Retention check interval in hours")
	showVersion := flag.Bool("version", false, "Show version information")
	flag.Parse()

	// Show version and exit
	if *showVersion {
		fmt.Println("Datum IoT Platform Server")
		fmt.Printf("Version: %s\n", Version)
		fmt.Printf("Build: %s\n", BuildDate)
		os.Exit(0)
	}

	// Get port (priority: flag > env > default)
	serverPort := *port
	if serverPort == "" {
		serverPort = os.Getenv("PORT")
		if serverPort == "" {
			serverPort = "8000"
		}
	}

	// Configure public URL
	publicURL := *serverURL
	if publicURL == "" {
		publicURL = os.Getenv("SERVER_URL")
		if publicURL == "" {
			publicURL = "http://localhost:" + serverPort
		}
	}
	SetProvisioningServerURL(publicURL)

	// Get data directory (priority: flag > env > default)
	dataDirPath := *dataDir
	if dataDirPath == "" {
		dataDirPath = os.Getenv("DATA_DIR")
		if dataDirPath == "" {
			dataDirPath = "./data"
		}
	}

	// Initialize structured logging
	// Log file path: data/server.log
	logFile := "server.log"
	// Basic path join if filepath not imported, valid for linux
	if strings.HasSuffix(dataDirPath, "/") {
		logFile = dataDirPath + "server.log"
	} else {
		logFile = dataDirPath + "/server.log"
	}

	logger.InitLogger(logFile)
	log := logger.GetLogger()

	// Configure retention (priority: flag > env > default)
	retentionConfig := storage.GetRetentionConfigFromEnv()
	if *retentionDays > 0 {
		retentionConfig.MaxAge = time.Duration(*retentionDays) * 24 * time.Hour
	}
	// Note: CleanupEvery is handled internally by tstorage or ignored if using WithRetention

	// Initialize storage (BuntDB for metadata, tstorage for time-series)
	// Initialize storage
	var err error
	dbURL := os.Getenv("DATABASE_URL")
	if dbURL != "" {
		// Use PostgreSQL
		store, err = postgres.New(dbURL)
		if err != nil {
			log.Fatal().Err(err).Msg("Failed to initialize PostgreSQL storage")
		}
		log.Info().Str("backend", "PostgreSQL").Msg("Storage initialized")
	} else {
		// Use BuntDB (Default)
		store, err = storage.New(dataDirPath+"/meta.db", dataDirPath+"/tsdata", retentionConfig.MaxAge)
		if err != nil {
			log.Fatal().Err(err).Msg("Failed to initialize BuntDB storage")
		}
		log.Info().
			Str("backend", "BuntDB").
			Str("timeseries", "tstorage").
			Str("data_dir", dataDirPath).
			Dur("retention", retentionConfig.MaxAge).
			Msg("Storage initialized")
	}

	// Startup cleanup: expired grace periods and provisioning requests
	if cleanedCount, err := store.CleanupExpiredGracePeriods(); err == nil && cleanedCount > 0 {
		log.Info().Int("count", cleanedCount).Msg("Cleaned up expired grace periods")
	}
	if cleanedCount, err := store.CleanupExpiredProvisioningRequests(); err == nil && cleanedCount > 0 {
		log.Info().Int("count", cleanedCount).Msg("Cleaned up expired provisioning requests")
	}

	// Start periodic cleanup goroutine (runs every hour)
	startPeriodicCleanup()
	log.Info().Msg("Periodic cleanup job started (runs every hour)")

	// Initialize Email Service
	emailService = email.NewEmailSender(publicURL)

	// Initialize Telemetry Processor
	telemetryProcessor = processing.NewTelemetryProcessor(store)

	// Initialize and Start MQTT Broker
	mqttBroker = mqtt_internal.NewBroker(store, telemetryProcessor)
	if err := mqttBroker.Start(); err != nil {
		log.Error().Err(err).Msg("Failed to start MQTT Broker")
	} else {
		// Ensure Broker closes on shutdown
		defer mqttBroker.Stop()
	}

	// Set global PublicURL for handlers that need it (e.g. web fallback)
	GlobalPublicURL = publicURL

	// Setup router
	gin.SetMode(gin.ReleaseMode)
	r := gin.New()

	// Middleware setup
	r.Use(gin.Recovery())
	// Use our custom structured logger for requests which filters noisy endpoints
	r.Use(logger.GinLogger())
	// Metrics middleware
	r.Use(metricsMiddleware())

	r.Use(securityHeadersMiddleware())
	// CORS setup
	r.Use(func(c *gin.Context) {
		// Basic CORS for development
		// In production, configure stricter rules
		c.Writer.Header().Set("Access-Control-Allow-Origin", "*")
		c.Writer.Header().Set("Access-Control-Allow-Credentials", "true")
		c.Writer.Header().Set("Access-Control-Allow-Headers", "Content-Type, Content-Length, Accept-Encoding, X-CSRF-Token, Authorization, accept, origin, Cache-Control, X-Requested-With")
		c.Writer.Header().Set("Access-Control-Allow-Methods", "POST, OPTIONS, GET, PUT, DELETE")

		if c.Request.Method == "OPTIONS" {
			c.AbortWithStatus(204)
			return
		}

		c.Next()
	})

	// Static Assets
	// Serve /assets from embedded "dist/assets"
	r.StaticFS("/assets", http.FS(mustSubFS(webFS, "dist/assets")))

	// SPA Fallback / Root Handler
	// If no route matches (e.g. /dashboard, /devices), serve index.html
	r.NoRoute(func(c *gin.Context) {
		// If accepting HTML, serve index.html (SPA)
		if strings.Contains(c.Request.Header.Get("Accept"), "text/html") {
			f, err := fs.Sub(webFS, "dist")
			if err != nil {
				c.String(http.StatusInternalServerError, "Web assets error")
				return
			}
			// Read and serve index.html directly to avoid 301 redirects from FileServer
			content, err := fs.ReadFile(f, "index.html")
			if err != nil {
				c.String(http.StatusInternalServerError, "index.html not found")
				return
			}
			c.Data(http.StatusOK, "text/html; charset=utf-8", content)
			return
		}
		// Otherwise 404
		c.JSON(http.StatusNotFound, gin.H{"error": "Not Found"})
	})

	// Root handler (Explicit /)
	r.GET("/", func(c *gin.Context) {
		// If accepting HTML, serve index.html
		if strings.Contains(c.Request.Header.Get("Accept"), "text/html") {
			f, err := fs.Sub(webFS, "dist")
			if err != nil {
				c.String(http.StatusInternalServerError, "Web assets error")
				return
			}
			// Read and serve index.html directly
			content, err := fs.ReadFile(f, "index.html")
			if err != nil {
				c.String(http.StatusInternalServerError, "index.html not found")
				return
			}
			c.Data(http.StatusOK, "text/html; charset=utf-8", content)
			return
		}

		// Otherwise, serve API info (legacy JSON behavior)
		c.JSON(http.StatusOK, gin.H{
			"service": "Datum IoT Platform",
			"version": Version,
			"endpoints": gin.H{
				"auth":         []string{"POST /auth/register", "POST /auth/login"},
				"devices":      []string{"POST /devices", "GET /devices", "DELETE /devices/{id}"},
				"commands":     []string{"POST /devices/{id}/commands", "GET /devices/{id}/commands"},
				"data":         []string{"POST /data/{id}", "GET /data/{id}", "GET /data/{id}/history"},
				"public":       []string{"POST /public/data/{id}", "GET /public/data/{id}"},
				"provisioning": []string{"POST /devices/register", "GET /devices/provisioning", "POST /provisioning/activate/{id}"},
			},
		})
	})

	r.GET("/health", healthHandler)
	r.GET("/healthz", healthHandler)
	r.GET("/live", livenessHandler)
	r.GET("/ready", readinessHandler)
	// System routes
	r.GET("/system/time", getSystemTimeHandler)

	// Auth routes
	authGroup := r.Group("/auth")
	{
		authGroup.POST("/register", registerHandler)
		authGroup.POST("/login", loginHandler)
		// Password Reset
		authGroup.POST("/forgot-password", forgotPasswordHandler)
		authGroup.POST("/reset-password", completeResetPasswordHandler)
	}

	// Password Reset Web Page (for deep linking fallback)
	r.GET("/reset-password", resetPasswordWebHandler)

	// Authenticated user routes (password change, self-deletion)
	authProtectedGroup := r.Group("/auth")
	authProtectedGroup.Use(UserAuthMiddleware(store))
	{
		authProtectedGroup.PUT("/password", changePasswordHandler)
		authProtectedGroup.DELETE("/user", deleteSelfHandler)

		// User API Key Management
		authProtectedGroup.POST("/keys", createKeyHandler)
		authProtectedGroup.GET("/keys", listKeysHandler)
		authProtectedGroup.DELETE("/keys/:id", deleteKeyHandler)
	}

	// Device management routes (require user auth)
	devicesGroup := r.Group("/devices")
	devicesGroup.Use(UserAuthMiddleware(store))
	{
		devicesGroup.POST("", createDeviceHandler)
		devicesGroup.GET("", listDevicesHandler)
		devicesGroup.DELETE("/:device_id", deleteDeviceHandler)

		// Command endpoints (user sends commands)
		devicesGroup.POST("/:device_id/commands", sendCommandHandler)
		devicesGroup.GET("/:device_id/commands", listCommandsHandler)
	}

	// Device command polling (device auth)
	deviceCommandGroup := r.Group("/devices")
	deviceCommandGroup.Use(auth.DeviceAuthMiddleware())
	{
		// Renamed from /:device_id/commands to /:device_id/commands/pending to avoid collision with User List Commands
		deviceCommandGroup.GET("/:device_id/commands/pending", pollCommandsHandler)
		deviceCommandGroup.GET("/:device_id/commands/stream", sseCommandsHandler) // SSE long polling
		deviceCommandGroup.GET("/:device_id/commands/poll", webhookPollHandler)   // HTTP long polling
		deviceCommandGroup.POST("/:device_id/commands/:command_id/ack", ackCommandHandler)
		deviceCommandGroup.GET("/:device_id/push", pushDataViaGetHandler) // For constrained devices (GET data push)

		// Video streaming upload (device uploads frames)
		deviceCommandGroup.POST("/:device_id/stream/frame", uploadFrameHandler)
	}

	// Data routes (require device auth)
	dataGroup := r.Group("/data")
	dataGroup.Use(auth.DeviceAuthMiddleware())
	{
		dataGroup.POST("/:device_id", postDataHandler)
	}

	// Data query routes (require user auth)
	dataQueryGroup := r.Group("/data")
	dataQueryGroup.Use(UserAuthMiddleware(store))
	{

		// Video streaming routes (require user auth)
		streamGroup := r.Group("/devices")
		streamGroup.Use(UserAuthMiddleware(store))
		{
			streamGroup.GET("/:device_id/stream/mjpeg", mjpegStreamHandler)       // MJPEG over HTTP (SSE-like)
			streamGroup.GET("/:device_id/stream/snapshot", streamSnapshotHandler) // Current frame snapshot
			streamGroup.GET("/:device_id/stream/ws", websocketStreamHandler)      // WebSocket binary stream
			streamGroup.GET("/:device_id/stream/info", streamInfoHandler)         // Stream metadata
		}
		dataQueryGroup.GET("/:device_id", getLatestDataHandler)
		dataQueryGroup.GET("/:device_id/history", getDataHistoryHandler)
	}

	// Public data endpoint (NO authentication - for quick prototyping)
	r.POST("/public/data/:device_id", postPublicDataHandler)
	r.GET("/public/data/:device_id", getPublicDataHandler)
	r.GET("/public/data/:device_id/history", getPublicDataHistoryHandler)

	// Serve firmware updates (protected)
	// Devices must provide ?token=<API_KEY> query parameter
	// This replaces public static serving
	// Serve firmware updates (protected)
	// Devices must provide ?token=<API_KEY> query parameter
	// This replaces public static serving
	r.GET("/devices/firmware/:filename", auth.DeviceAuthMiddleware(), func(c *gin.Context) {
		filename := c.Param("filename")
		// Prevent directory traversal
		if strings.Contains(filename, "..") || strings.Contains(filename, "/") || strings.Contains(filename, "\\") {
			c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid filename"})
			return
		}

		filePath := "./firmware/" + filename
		if _, err := os.Stat(filePath); os.IsNotExist(err) {
			c.JSON(http.StatusNotFound, gin.H{"error": "Firmware not found"})
			return
		}

		c.File(filePath)
	})

	// Admin routes
	setupAdminRoutes(r, store)

	// Provisioning routes (Mobile App & Device Activation)
	RegisterProvisioningRoutes(r, UserAuthMiddleware(store))

	// Swagger UI documentation
	setupSwagger(r)

	log.Info().
		Str("port", serverPort).
		Str("public_url", publicURL).
		Str("endpoints", "/auth, /device, /data, /public/data, /provisioning").
		Str("docs", publicURL+"/docs").
		Msg("🚀 Datum IoT Platform starting")

	// Start server
	srv := &http.Server{
		Addr:    ":" + serverPort,
		Handler: r,
	}

	go func() {
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			fmt.Fprintf(os.Stderr, "CRITICAL ERROR: Server failed to start: %v\n", err)
			log.Fatal().Err(err).Msg("Server failed to start")
		}
	}()

	// Wait for interrupt signal to gracefully shutdown the server
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit
	log.Info().Msg("Shutting down server...")

	// The context is used to inform the server it has 5 seconds to finish
	// the request it is currently handling
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	if err := srv.Shutdown(ctx); err != nil {
		log.Fatal().Err(err).Msg("Server forced to shutdown")
	}

	// Close storage
	if err := store.Close(); err != nil {
		log.Error().Err(err).Msg("Error closing storage")
	}

	log.Info().Msg("Server exiting")
}

// Helper to get sub-fs without error return (panics on error, safe for init)
func mustSubFS(f fs.FS, dir string) fs.FS {
	sub, err := fs.Sub(f, dir)
	if err != nil {
		panic(err)
	}
	return sub
}

// Health check handlers
func healthHandler(c *gin.Context) {
	// Basic health check
	status := gin.H{
		"status":    "healthy",
		"timestamp": time.Now().Unix(),
		"service":   "datumpy-api",
		"version":   Version,
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

// Utility functions

func generateID(prefix string) string {
	bytes := make([]byte, 6)
	rand.Read(bytes)
	return prefix + "_" + hex.EncodeToString(bytes)
}

// startPeriodicCleanup runs background cleanup tasks periodically
func startPeriodicCleanup() {
	log := logger.GetLogger()
	ticker := time.NewTicker(1 * time.Hour) // Run every hour
	go func() {
		for range ticker.C {
			// Cleanup expired grace periods (old tokens after grace period)
			if count, err := store.CleanupExpiredGracePeriods(); err == nil && count > 0 {
				log.Info().Int("count", count).Msg("Periodic cleanup: expired grace periods")
			}

			// Cleanup expired provisioning requests (Soft Delete)
			if count, err := store.CleanupExpiredProvisioningRequests(); err == nil && count > 0 {
				log.Info().Int("count", count).Msg("Periodic cleanup: expired provisioning requests")
			}

			// Purge old provisioning requests (Hard Delete > 7 days old)
			if count, err := store.PurgeProvisioningRequests(7 * 24 * time.Hour); err == nil && count > 0 {
				log.Info().Int("count", count).Msg("Periodic cleanup: purged old provisioning requests")
			}
		}
	}()
}
