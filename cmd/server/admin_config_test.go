package main

import (
	"bytes"
	"datum-go/internal/handlers"
	"datum-go/internal/logger"
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

	// Initialize logger
	logger.InitLogger(tmpDir + "/test.log")

	router := gin.New()
	cleanup := func() {
		store.Close()
	}

	return router, cleanup
}

func TestGetSystemConfigHandler(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.GET("/admin/sys/config", h.GetSystemConfigHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/sys/config", nil)
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

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.PUT("/admin/sys/retention", h.UpdateRetentionPolicyHandler)

	body := map[string]interface{}{
		"days":                 30,
		"check_interval_hours": 12,
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/sys/retention", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "retention")
	assert.Equal(t, "Retention policy updated", response["message"])
}

func TestUpdateRetentionPolicyHandlerWithPublic(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.PUT("/admin/sys/retention", h.UpdateRetentionPolicyHandler)

	body := map[string]interface{}{
		"days":                 30,
		"public_days":          3,
		"check_interval_hours": 12,
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/sys/retention", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	retention := response["retention"].(map[string]interface{})
	assert.Equal(t, float64(3), retention["public_days"])
}

func TestUpdateRetentionPolicyHandlerInvalidDays(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.PUT("/admin/sys/retention", h.UpdateRetentionPolicyHandler)

	body := map[string]interface{}{
		"days":                 0, // Invalid: less than min
		"check_interval_hours": 12,
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/sys/retention", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestUpdateRetentionPolicyHandlerInvalidInterval(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.PUT("/admin/sys/retention", h.UpdateRetentionPolicyHandler)

	body := map[string]interface{}{
		"days":                 30,
		"check_interval_hours": 0, // Invalid: less than min
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/sys/retention", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestUpdateRateLimitHandler(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.PUT("/admin/sys/ratelimit", h.UpdateRateLimitHandler)

	body := map[string]interface{}{
		"max_requests":   200,
		"window_seconds": 120,
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/sys/ratelimit", bytes.NewReader(bodyBytes))
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

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.PUT("/admin/sys/ratelimit", h.UpdateRateLimitHandler)

	body := map[string]interface{}{
		"max_requests":   5, // Invalid: less than min
		"window_seconds": 60,
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/sys/ratelimit", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestUpdateAlertConfigHandler(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.PUT("/admin/sys/alerts", h.UpdateAlertConfigHandler)

	body := map[string]interface{}{
		"email_enabled":    true,
		"disk_threshold":   85,
		"memory_threshold": 90,
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/sys/alerts", bytes.NewReader(bodyBytes))
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

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.PUT("/admin/sys/alerts", h.UpdateAlertConfigHandler)

	body := map[string]interface{}{
		"email_enabled":    true,
		"disk_threshold":   5, // Invalid: less than min
		"memory_threshold": 90,
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/sys/alerts", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestGetLogsHandler(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.GET("/admin/logs", h.GetLogsHandler)

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

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.GET("/admin/logs", h.GetLogsHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/logs?lines=50", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestClearLogsHandler(t *testing.T) {
	router, cleanup := setupAdminConfigTestServer(t)
	defer cleanup()

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.POST("/admin/logs/clear", h.ClearLogsHandler)

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

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.POST("/sys/setup", h.SetupSystemHandler)

	body := map[string]interface{}{
		"platform_name":  "Test Platform",
		"admin_email":    "newadmin@test.com",
		"admin_password": "SecurePass123!",
		"allow_register": true,
		"data_retention": 7,
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/sys/setup", bytes.NewReader(bodyBytes))
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
	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.POST("/sys/setup", h.SetupSystemHandler)

	body := map[string]interface{}{
		"email":    "admin@test.com",
		"password": "password123",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/sys/setup", bytes.NewReader(bodyBytes))
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

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.POST("/admin/users/:username/password", h.ResetPasswordHandler)

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

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.POST("/admin/users/:user_id/password", h.ResetPasswordHandler)

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
		h := handlers.NewAdminHandler(store, nil, time.Now())
		h.ProvisionDeviceHandler(c)
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
		h := handlers.NewAdminHandler(store, nil, time.Now())
		h.ProvisionDeviceHandler(c)
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

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.GET("/api/sys/status", h.GetSystemStatusHandler)

	req := httptest.NewRequest(http.MethodGet, "/api/sys/status", nil)
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
	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.GET("/api/sys/status", h.GetSystemStatusHandler)

	req := httptest.NewRequest(http.MethodGet, "/api/sys/status", nil)
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

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.GET("/admin/dev/:device_id", h.GetDeviceAdminHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/dev/admin-device-001", nil)
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

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.GET("/admin/dev/:device_id", h.GetDeviceAdminHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/dev/nonexistent", nil)
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

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.PATCH("/admin/dev/:device_id", h.UpdateDeviceHandler)

	body := map[string]interface{}{
		"status": "invalid_status",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPatch, "/admin/dev/invalid-status-device", bytes.NewReader(bodyBytes))
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

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.GET("/admin/database/stats", h.GetDatabaseStatsHandler)

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

	h := handlers.NewAdminHandler(store, nil, time.Now())
	router.GET("/admin/dev", h.ListAllDevicesHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/dev", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "devices")
}
