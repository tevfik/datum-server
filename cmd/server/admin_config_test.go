package main

import (
	"bytes"
	"datum-go/internal/storage"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func setupAdminConfigTestServer(t *testing.T) (*gin.Engine, func()) {
	gin.SetMode(gin.TestMode)

	tmpDir := t.TempDir()
	var err error
	store, err = storage.New(tmpDir+"/meta.db", tmpDir+"/tsdata", 7*24*time.Hour)
	require.NoError(t, err)

	// Initialize system
	store.InitializeSystem("Test Platform", true, 7)

	router := gin.New()
	cleanup := func() {
		store.Close()
	}

	return router, cleanup
}

func TestGetSystemConfigHandler(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	router.GET("/admin/system/config", getSystemConfigHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/system/config", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "retention")
	assert.Contains(t, response, "rate_limit")
	assert.Contains(t, response, "alerts")
}

func TestUpdateRetentionPolicyHandler(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	router.PUT("/admin/system/retention", updateRetentionPolicyHandler)

	body := map[string]interface{}{
		"days":                 30,
		"check_interval_hours": 12,
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/system/retention", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "retention")
	assert.Equal(t, "Retention policy updated", response["message"])
}

func TestUpdateRetentionPolicyHandlerInvalidDays(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	router.PUT("/admin/system/retention", updateRetentionPolicyHandler)

	body := map[string]interface{}{
		"days":                 0, // Invalid: less than min
		"check_interval_hours": 12,
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/system/retention", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestUpdateRetentionPolicyHandlerInvalidInterval(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	router.PUT("/admin/system/retention", updateRetentionPolicyHandler)

	body := map[string]interface{}{
		"days":                 30,
		"check_interval_hours": 0, // Invalid: less than min
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/system/retention", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestUpdateRateLimitHandler(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	router.PUT("/admin/system/ratelimit", updateRateLimitHandler)

	body := map[string]interface{}{
		"max_requests":   200,
		"window_seconds": 120,
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/system/ratelimit", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "rate_limit")
	assert.Equal(t, "Rate limit updated", response["message"])
}

func TestUpdateRateLimitHandlerInvalidRequests(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	router.PUT("/admin/system/ratelimit", updateRateLimitHandler)

	body := map[string]interface{}{
		"max_requests":   5, // Invalid: less than min
		"window_seconds": 60,
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/system/ratelimit", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestUpdateAlertConfigHandler(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	router.PUT("/admin/system/alerts", updateAlertConfigHandler)

	body := map[string]interface{}{
		"email_enabled":    true,
		"disk_threshold":   85,
		"memory_threshold": 90,
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/system/alerts", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "alerts")
	assert.Equal(t, "Alert configuration updated", response["message"])
}

func TestUpdateAlertConfigHandlerInvalidThreshold(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	router.PUT("/admin/system/alerts", updateAlertConfigHandler)

	body := map[string]interface{}{
		"email_enabled":    true,
		"disk_threshold":   5, // Invalid: less than min
		"memory_threshold": 90,
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/system/alerts", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestGetLogsHandler(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	router.GET("/admin/logs", getLogsHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/logs", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	// Should return OK even if no log file exists
	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "logs")
}

func TestGetLogsHandlerWithLines(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	router.GET("/admin/logs", getLogsHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/logs?lines=50", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestClearLogsHandler(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	router.POST("/admin/logs/clear", clearLogsHandler)

	req := httptest.NewRequest(http.MethodPost, "/admin/logs/clear", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "message")
}

func TestSetupSystemHandler(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	// Reset system first
	store.ResetSystem()

	router.POST("/system/setup", setupSystemHandler)

	body := map[string]interface{}{
		"platform_name":  "Test Platform",
		"admin_email":    "newadmin@test.com",
		"admin_password": "SecurePass123!",
		"allow_register": true,
		"data_retention": 7,
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/system/setup", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "token")
	assert.Contains(t, response, "user_id")
}

func TestSetupSystemHandlerAlreadyInitialized(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	// System is already initialized in setup
	router.POST("/system/setup", setupSystemHandler)

	body := map[string]interface{}{
		"email":    "admin@test.com",
		"password": "password123",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/system/setup", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusConflict, w.Code)
}

func TestResetPasswordHandler(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	// Create a test user
	user := &storage.User{
		ID:           "reset-user",
		Email:        "reset@test.com",
		PasswordHash: "oldhash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user)

	router.POST("/admin/users/:username/password", resetPasswordHandler)

	body := map[string]interface{}{
		"new_password": "NewSecurePass123!",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/admin/users/reset@test.com/password", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestResetPasswordHandlerInvalidJSON(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	router.POST("/admin/users/:user_id/password", resetPasswordHandler)

	req := httptest.NewRequest(http.MethodPost, "/admin/users/user-id/password", bytes.NewReader([]byte("{invalid")))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestProvisionDeviceHandler(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	// Create test user
	user := &storage.User{
		ID:           "provision-user",
		Email:        "provision@test.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user)

	router.POST("/admin/provision", func(c *gin.Context) {
		c.Set("user_id", "provision-user")
		provisionDeviceHandler(c)
	})

	body := map[string]interface{}{
		"name": "Provisioned Device",
		"type": "sensor",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/admin/provision", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "device_id")
	assert.Contains(t, response, "api_key")
}

func TestProvisionDeviceHandlerWithDeviceID(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	user := &storage.User{
		ID:           "provision-user",
		Email:        "provision@test.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user)

	router.POST("/admin/provision", func(c *gin.Context) {
		c.Set("user_id", "provision-user")
		provisionDeviceHandler(c)
	})

	body := map[string]interface{}{
		"device_id": "custom_device_id",
		"name":      "Device with Custom ID",
		"type":      "actuator",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/admin/provision", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "custom_device_id", response["device_id"])
}

// System status handler tests
func TestGetSystemStatusHandler(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	router.GET("/api/system/status", getSystemStatusHandler)

	req := httptest.NewRequest(http.MethodGet, "/api/system/status", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, true, response["initialized"])
	// Platform details are hidden when initialized
	// assert.Contains(t, response, "platform_name")
}

func TestGetSystemStatusHandlerNotInitialized(t *testing.T) {
	gin.SetMode(gin.TestMode)
	tmpDir := t.TempDir()
	var err error
	store, err = storage.New(tmpDir+"/meta.db", tmpDir+"/tsdata", 7*24*time.Hour)
	require.NoError(t, err)
	defer store.Close()

	router := gin.New()
	router.GET("/api/system/status", getSystemStatusHandler)

	req := httptest.NewRequest(http.MethodGet, "/api/system/status", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, false, response["initialized"])
}

// Device admin handler tests
func TestGetDeviceAdminHandler(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	device := &storage.Device{
		ID:        "admin-device-001",
		UserID:    "owner-123",
		Name:      "Admin Test Device",
		Type:      "sensor",
		APIKey:    "sk_admin_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.GET("/admin/devices/:device_id", getDeviceAdminHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/devices/admin-device-001", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "admin-device-001", response["id"])
	assert.Equal(t, "Admin Test Device", response["name"])
	assert.Contains(t, response, "api_key")
}

func TestGetDeviceAdminHandlerNotFound(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	router.GET("/admin/devices/:device_id", getDeviceAdminHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/devices/nonexistent", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func TestUpdateDeviceHandlerInvalidStatus(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	device := &storage.Device{
		ID:        "invalid-status-device",
		UserID:    "owner-user",
		Name:      "Device",
		Type:      "sensor",
		APIKey:    "sk_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.PATCH("/admin/devices/:device_id", updateDeviceHandler)

	body := map[string]interface{}{
		"status": "invalid_status",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPatch, "/admin/devices/invalid-status-device", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

// Database stats handler test
func TestGetDatabaseStatsHandler(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	// Add some test data
	user := &storage.User{
		ID:        "stats-user",
		Email:     "stats@test.com",
		Role:      "user",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateUser(user)

	device := &storage.Device{
		ID:        "stats-device",
		UserID:    "stats-user",
		Name:      "Stats Device",
		Type:      "sensor",
		APIKey:    "sk_stats_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.GET("/admin/database/stats", getDatabaseStatsHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/database/stats", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "total_users")
	assert.Contains(t, response, "total_devices")
	assert.Contains(t, response, "platform_name")
}

// Test list all devices handler
func TestListAllDevicesHandler(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	// Create test devices
	device1 := &storage.Device{
		ID:        "list-device-1",
		UserID:    "user-1",
		Name:      "Device 1",
		Type:      "sensor",
		APIKey:    "sk_key1",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device1)

	device2 := &storage.Device{
		ID:        "list-device-2",
		UserID:    "user-2",
		Name:      "Device 2",
		Type:      "actuator",
		APIKey:    "sk_key2",
		Status:    "banned",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device2)

	router.GET("/admin/devices", listAllDevicesHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/devices", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "devices")
}
