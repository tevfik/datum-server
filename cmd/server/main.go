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
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"time"

	"datum-go/internal/api"
	"datum-go/internal/auth"
	"datum-go/internal/email"
	"datum-go/internal/handlers"
	"datum-go/internal/logger"
	"datum-go/internal/metrics"
	mqtt_internal "datum-go/internal/mqtt"
	"datum-go/internal/processing"
	"datum-go/internal/storage"
	"datum-go/internal/storage/postgres"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/joho/godotenv"
)

//go:embed dist/*
var webFS embed.FS

var (
	Version   = "1.4.1"
	BuildDate = "unknown"
)

var (
	store              storage.Provider
	emailService       *email.EmailSender
	telemetryProcessor *processing.TelemetryProcessor
	mqttBroker         *mqtt_internal.Broker
	serverStartTime    time.Time // Track server start time for uptime
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
		// Strict transport security (enabled by default, disable with ENABLE_HSTS=false)
		if os.Getenv("ENABLE_HSTS") != "false" {
			c.Header("Strict-Transport-Security", "max-age=31536000; includeSubDomains")
		}
		// Referrer policy
		c.Header("Referrer-Policy", "strict-origin-when-cross-origin")
		// Content security policy
		csp := os.Getenv("CONTENT_SECURITY_POLICY")
		if csp == "" {
			csp = "default-src 'self'; script-src 'self' https://unpkg.com; style-src 'self' 'unsafe-inline' https://unpkg.com; img-src 'self' data:; connect-src 'self' ws: wss:;"
		}
		c.Header("Content-Security-Policy", csp)
		// Permissions policy
		c.Header("Permissions-Policy", "camera=(), microphone=(), geolocation=()")
		c.Next()
	}
}

// requestIDMiddleware adds a unique request ID to each request context and response header.
func requestIDMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		requestID := c.GetHeader("X-Request-ID")
		if requestID == "" {
			requestID = uuid.New().String()
		}
		c.Set("request_id", requestID)
		c.Header("X-Request-ID", requestID)
		c.Next()
	}
}

// requestBodyLimitMiddleware limits the maximum request body size.
func requestBodyLimitMiddleware() gin.HandlerFunc {
	// Default 5MB, configurable via MAX_REQUEST_BODY_BYTES
	maxBytes := int64(5 * 1024 * 1024)
	if v := os.Getenv("MAX_REQUEST_BODY_BYTES"); v != "" {
		if parsed, err := strconv.ParseInt(v, 10, 64); err == nil && parsed > 0 {
			maxBytes = parsed
		}
	}
	return func(c *gin.Context) {
		c.Request.Body = http.MaxBytesReader(c.Writer, c.Request.Body, maxBytes)
		c.Next()
	}
}

// requestTimeoutMiddleware adds a deadline to non-streaming requests.
func requestTimeoutMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		path := c.Request.URL.Path
		// Skip timeout for streaming, SSE, and WebSocket endpoints
		if strings.Contains(path, "/stream/") ||
			strings.Contains(path, "/cmd/stream") ||
			strings.Contains(path, "/cmd/poll") ||
			strings.Contains(path, "/logs/stream") {
			c.Next()
			return
		}
		ctx, cancel := context.WithTimeout(c.Request.Context(), 30*time.Second)
		defer cancel()
		c.Request = c.Request.WithContext(ctx)
		c.Next()
	}
}

// loadOrGenerateJWTSecret manages JWT secret persistence
func loadOrGenerateJWTSecret(dataDir string) {
	// 1. Check environment variable (highest priority)
	if os.Getenv("JWT_SECRET") != "" {
		return // internal/auth already handles this
	}

	// 2. Check for persistent secret file
	secretFile := filepath.Join(dataDir, ".jwt_secret")

	if content, err := os.ReadFile(secretFile); err == nil {
		if len(content) >= 32 {
			auth.SetJWTSecret(content)
			return
		}
	}

	// 3. Generate new secret
	bytes := make([]byte, 32)
	if _, err := rand.Read(bytes); err != nil {
		log := logger.GetLogger()
		log.Fatal().Err(err).Msg("Critical: Failed to generate random bytes for JWT secret")
		return
	}
	secret := hex.EncodeToString(bytes)

	log := logger.GetLogger()
	// 4. Save to file
	if err := os.WriteFile(secretFile, []byte(secret), 0600); err != nil {
		log.Warn().Err(err).Str("file", secretFile).Msg("Failed to persist JWT secret")
	} else {
		log.Info().Str("file", secretFile).Msg("Persisted new JWT secret")
	}

	// 5. Update auth package
	auth.SetJWTSecret([]byte(secret))
}

func main() {
	// Record server start time for uptime tracking
	serverStartTime = time.Now()

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

	// Get data directory (priority: flag > env > default)
	dataDirPath := *dataDir
	if dataDirPath == "" {
		dataDirPath = os.Getenv("DATA_DIR")
		if dataDirPath == "" {
			dataDirPath = "./data"
		}
	}

	// Initialize structured logging
	logFile := filepath.Join(dataDirPath, "server.log")

	logger.InitLogger(logFile)
	log := logger.GetLogger()

	// Handle JWT Secret persistence
	loadOrGenerateJWTSecret(dataDirPath)

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
			Dur("public_retention", retentionConfig.PublicMaxAge).
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
	}
	// mqttBroker is stopped in the graceful shutdown handler below

	// Setup router
	gin.SetMode(gin.ReleaseMode)
	r := gin.New()

	// Trust all proxies (Private Networks) to resolve correct IP behind Nginx/Docker
	// This enables ClientIP() to read X-Forwarded-For correctly
	r.SetTrustedProxies([]string{"127.0.0.1", "192.168.0.0/16", "172.16.0.0/12", "10.0.0.0/8"})

	// Middleware setup
	r.Use(gin.Recovery())
	// Request ID for tracing
	r.Use(requestIDMiddleware())
	// Request body size limit
	r.Use(requestBodyLimitMiddleware())
	// Request timeout for non-streaming endpoints
	r.Use(requestTimeoutMiddleware())
	// Use our custom structured logger for requests which filters noisy endpoints
	r.Use(logger.GinLogger())
	// Metrics middleware
	r.Use(metrics.Middleware())

	r.Use(securityHeadersMiddleware())
	// CORS setup (origins parsed once and cached)
	var corsAllowedOrigins []string
	var corsAllowAll bool
	{
		raw := os.Getenv("CORS_ALLOWED_ORIGINS")
		if raw == "" || raw == "*" {
			corsAllowAll = true
		} else {
			for _, o := range strings.Split(raw, ",") {
				if trimmed := strings.TrimSpace(o); trimmed != "" {
					corsAllowedOrigins = append(corsAllowedOrigins, trimmed)
				}
			}
		}
	}
	r.Use(func(c *gin.Context) {
		origin := c.Request.Header.Get("Origin")

		if corsAllowAll {
			c.Writer.Header().Set("Access-Control-Allow-Origin", "*")
		} else {
			for _, allowed := range corsAllowedOrigins {
				if allowed == origin {
					c.Writer.Header().Set("Access-Control-Allow-Origin", origin)
					c.Writer.Header().Set("Access-Control-Allow-Credentials", "true")
					break
				}
			}
		}

		c.Writer.Header().Set("Access-Control-Allow-Headers", "Content-Type, Content-Length, Accept-Encoding, X-CSRF-Token, Authorization, accept, origin, Cache-Control, X-Requested-With")
		c.Writer.Header().Set("Access-Control-Allow-Methods", "POST, OPTIONS, GET, PUT, DELETE")

		if c.Request.Method == "OPTIONS" {
			c.AbortWithStatus(204)
			return
		}

		c.Next()
	})

	// Static Assets
	// Serve /assets from embedded "dist/assets" (gracefully skip if dist not built)
	if sub, err := fs.Sub(webFS, "dist/assets"); err == nil {
		r.StaticFS("/assets", http.FS(sub))
	} else {
		log.Warn().Msg("Web assets (dist/assets) not found in embed — UI will not be served")
	}

	// SPA Fallback / Root Handler
	// If no route matches, check if it's an API call or UI
	r.NoRoute(func(c *gin.Context) {
		path := c.Request.URL.Path
		// If it looks like an API call, return JSON 404
		if strings.HasPrefix(path, "/auth/") ||
			strings.HasPrefix(path, "/api/") ||
			strings.HasPrefix(path, "/dev/") ||
			strings.HasPrefix(path, "/data/") ||
			strings.HasPrefix(path, "/pub/") ||
			strings.HasPrefix(path, "/sys/") {
			c.JSON(http.StatusNotFound, gin.H{"error": "Not Found"})
			return
		}

		// Otherwise serve index.html (SPA)
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
	})

	// Root handler (Explicit /)
	r.GET("/", rootHandler)
	// Define API Config for all route registers
	apiConfig := api.Config{
		Store:        store,
		Processor:    telemetryProcessor,
		MQTTBroker:   mqttBroker,
		EmailService: emailService,
		PublicURL:    publicURL,
		Version:      Version,
		BuildDate:    BuildDate,
	}

	r.GET("/health", healthHandler)
	r.GET("/healthz", healthHandler)
	r.GET("/live", livenessHandler)
	r.GET("/ready", readinessHandler)
	// System routes (using new internal/api package)
	api.RegisterSystemRoutes(r, apiConfig)

	// Auth routes (using new internal/api package)
	api.RegisterAuthRoutes(r, apiConfig)

	// User API Key routes (using new internal/api package)
	api.RegisterKeyRoutes(r, apiConfig)

	// Database Routes (using new internal/api package)
	// Registers /db (User) and /admin/db (Admin)
	api.RegisterDBRoutes(r, apiConfig)

	// Public Routes (using new internal/api package)
	api.RegisterPublicRoutes(r, apiConfig)

	// Admin routes

	// Command routes (using new internal/api package)
	api.RegisterCommandRoutes(r, apiConfig)

	// Stream and SSE routes (using new internal/api package)
	api.RegisterStreamRoutes(r, apiConfig)
	api.RegisterSSERoutes(r, apiConfig)

	// Device routes (using new internal/api package)
	api.RegisterDeviceRoutes(r, apiConfig)

	// Data routes (using new internal/api package)
	api.RegisterDataRoutes(r, apiConfig)

	// Specialized/Hybrid Routes (using new internal/api package)
	api.RegisterSpecializedRoutes(r, apiConfig)

	// MQTT admin routes (using new internal/api package)
	api.RegisterMQTTRoutes(r, apiConfig)

	// Metrics endpoint
	api.RegisterMetricsRoutes(r, apiConfig)

	// Serve firmware updates (protected)
	// Devices must provide valid device auth
	firmwareDir, _ := filepath.Abs("./firmware")
	r.GET("/dev/fw/:filename", auth.DeviceAuthMiddleware(), func(c *gin.Context) {
		filename := c.Param("filename")
		// Only allow alphanumeric, dash, underscore, dot
		if strings.Contains(filename, "..") || strings.Contains(filename, "/") || strings.Contains(filename, "\\") {
			c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid filename"})
			return
		}

		// Use filepath.Join with absolute base and verify the resolved path stays within firmwareDir
		filePath := filepath.Join(firmwareDir, filepath.Base(filename))
		resolved, err := filepath.EvalSymlinks(filePath)
		if err != nil {
			c.JSON(http.StatusNotFound, gin.H{"error": "Firmware not found"})
			return
		}
		if !strings.HasPrefix(resolved, firmwareDir) {
			c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid filename"})
			return
		}

		c.File(resolved)
	})

	// Public command access (for demo purposes)
	// r.POST("/api/dev/:device_id/commands", commandHandler)

	// Auth routes
	// setupAuthRoutes(r, store) // Assuming this is a new call, but not explicitly in the original file.
	// Admin routes
	// Legacy admin handlers removed (migrated to internal/api)

	// API Key routes
	// setupKeyRoutes(r, store) // Assuming this is a new call, but not explicitly in the original file.

	// Provisioning Endpoints (using new internal/api package)
	api.RegisterProvisioningRoutes(r, apiConfig)

	// Legacy Admin Routes (not yet migrated to internal/api)
	// Registers: /admin/dev/*, /admin/config/*, /admin/logs/*, /admin/firmware
	adminHandler := handlers.NewAdminHandler(store, mqttBroker, serverStartTime)
	adminHandler.RegisterRoutes(r)

	// Swagger UI documentation
	setupSwagger(r)

	log.Info().
		Str("port", serverPort).
		Str("public_url", publicURL).
		Str("version", Version).
		Str("build_date", BuildDate).
		Str("endpoints", "/auth, /dev, /dev/data, /pub, /prov").
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

	// Stop MQTT Broker
	if mqttBroker != nil {
		log.Info().Msg("Stopping MQTT broker...")
		mqttBroker.Stop()
	}

	// Flush remaining telemetry data
	if telemetryProcessor != nil {
		log.Info().Msg("Flushing telemetry processor...")
		telemetryProcessor.Close()
	}

	// Close storage
	if err := store.Close(); err != nil {
		log.Error().Err(err).Msg("Error closing storage")
	}

	log.Info().Msg("Server exiting")
}

// Health check handlers
func healthHandler(c *gin.Context) {
	status := gin.H{
		"status":    "healthy",
		"timestamp": time.Now().Unix(),
		"service":   "datum-api",
		"version":   Version,
	}

	if store == nil {
		status["status"] = "unhealthy"
		status["storage"] = "disconnected"
		c.JSON(http.StatusServiceUnavailable, status)
		return
	}

	// Verify storage is actually responsive with a lightweight query
	if _, err := store.GetSystemConfig(); err != nil {
		status["status"] = "degraded"
		status["storage"] = "error"
		status["storage_error"] = err.Error()
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

// startPeriodicCleanup runs background cleanup tasks periodically
func startPeriodicCleanup() {
	log := logger.GetLogger()
	ticker := time.NewTicker(1 * time.Hour) // Run every hour
	go func() {
		for range ticker.C {
			opID := uuid.New().String()[:8]
			clog := log.With().Str("op", "periodic-cleanup").Str("op_id", opID).Logger()

			// Cleanup expired grace periods (old tokens after grace period)
			if count, err := store.CleanupExpiredGracePeriods(); err == nil && count > 0 {
				clog.Info().Int("count", count).Msg("Cleaned expired grace periods")
			}

			// Cleanup expired provisioning requests (Soft Delete)
			if count, err := store.CleanupExpiredProvisioningRequests(); err == nil && count > 0 {
				clog.Info().Int("count", count).Msg("Cleaned expired provisioning requests")
			}

			// Purge old provisioning requests (Hard Delete > 7 days old)
			if count, err := store.PurgeProvisioningRequests(7 * 24 * time.Hour); err == nil && count > 0 {
				clog.Info().Int("count", count).Msg("Purged old provisioning requests")
			}

			// Cleanup Public Data (Soft/Hard check)
			config, _ := store.GetSystemConfig()
			if config != nil && config.PublicDataRetention > 0 {
				if count, err := store.CleanupPublicData(time.Duration(config.PublicDataRetention) * 24 * time.Hour); err == nil && count > 0 {
					clog.Info().Int("count", count).Msg("Cleaned public data")
				}
			}
		}
	}()
}

func rootHandler(c *gin.Context) {
	// If explicitly asking for JSON, return API info
	if c.Request.Header.Get("Accept") == "application/json" {
		c.JSON(http.StatusOK, gin.H{
			"service": "Datum IoT Platform",
			"version": Version,
			"endpoints": gin.H{
				"auth": []string{"POST /auth/register", "POST /auth/login"},
				"dev":  []string{"POST /dev", "GET /dev", "DELETE /dev/{id}"},
			},
		})
		return
	}

	// Default to serving index.html
	f, err := fs.Sub(webFS, "dist")
	if err != nil {
		// If dist is empty (e.g. valid during development), return simple message
		c.String(http.StatusOK, "Datum IoT Server is running. (Web UI not embedded)")
		return
	}
	content, err := fs.ReadFile(f, "index.html")
	if err != nil {
		c.String(http.StatusOK, "Datum IoT Server is running. (index.html not found in dist)")
		return
	}
	c.Data(http.StatusOK, "text/html; charset=utf-8", content)
}
