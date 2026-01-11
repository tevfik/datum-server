package main

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/handlers"
	"datum-go/internal/processing"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
)

// TestPostDataWithDeviceAuth tests data submission with device API key
func TestPostDataWithDeviceAuth(t *testing.T) {
	gin.SetMode(gin.TestMode)
	testStore, err := storage.New(":memory:", "", 7*24*time.Hour)
	assert.NoError(t, err)
	store = testStore
	telemetryProcessor = processing.NewTelemetryProcessor(testStore)

	// Initialize system
	testStore.InitializeSystem("Test", false, 7)

	// Create user
	user := &storage.User{
		ID:    "user-1",
		Email: "user@test.com",
		Role:  "admin",
	}
	testStore.CreateUser(user)

	// Create device
	device := &storage.Device{
		ID:        "device-1",
		UserID:    "user-1",
		Name:      "Test Device",
		APIKey:    "dk_test_api_key_123",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	err = testStore.CreateDevice(device)
	assert.NoError(t, err)

	// Setup router
	r := gin.New()
	dataGroup := r.Group("/data")
	dataGroup.Use(auth.DeviceAuthMiddleware())
	{
		dataGroup.POST("/:device_id", postDataHandler)
	}

	// Post data
	dataPayload := map[string]interface{}{
		"temperature": 25.5,
		"humidity":    60,
	}

	body, _ := json.Marshal(dataPayload)
	req, _ := http.NewRequest("POST", "/data/device-1", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer dk_test_api_key_123")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	testStore.Close()
}

// TestPostDataUnauthorized tests data submission without auth
func TestPostDataUnauthorized(t *testing.T) {
	gin.SetMode(gin.TestMode)
	testStore, _ := storage.New(":memory:", "", 7*24*time.Hour)
	store = testStore
	telemetryProcessor = processing.NewTelemetryProcessor(testStore)

	r := gin.New()
	dataGroup := r.Group("/data")
	dataGroup.Use(auth.DeviceAuthMiddleware())
	{
		dataGroup.POST("/:device_id", postDataHandler)
	}

	dataPayload := map[string]interface{}{
		"temperature": 25.5,
	}

	body, _ := json.Marshal(dataPayload)
	req, _ := http.NewRequest("POST", "/data/device-1", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)

	testStore.Close()
}

// TestPublicDataEndpoint tests public data endpoint
func TestPublicDataEndpoint(t *testing.T) {
	gin.SetMode(gin.TestMode)
	testStore, _ := storage.New(":memory:", "", 7*24*time.Hour)
	store = testStore

	testStore.InitializeSystem("Test", false, 7)

	r := gin.New()
	r.POST("/pub/data/:device_id", postPublicDataHandler)

	dataPayload := map[string]interface{}{
		"temperature": 25.5,
		"humidity":    60,
	}

	body, _ := json.Marshal(dataPayload)
	req, _ := http.NewRequest("POST", "/pub/data/test-device", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	// Public endpoint should accept data even without auth
	assert.Equal(t, http.StatusOK, w.Code)

	testStore.Close()
}

// TestGetLatestData tests fetching latest device data
func TestGetLatestData(t *testing.T) {
	gin.SetMode(gin.TestMode)
	testStore, _ := storage.New(":memory:", "", 7*24*time.Hour)
	store = testStore

	testStore.InitializeSystem("Test", false, 7)

	// Create user and device
	user := &storage.User{
		ID:    "user-1",
		Email: "user@test.com",
		Role:  "user",
	}
	testStore.CreateUser(user)

	device := &storage.Device{
		ID:     "device-1",
		UserID: "user-1",
		Name:   "Test Device",
	}
	testStore.CreateDevice(device)
	// Insert some test data
	dataPoint := &storage.DataPoint{
		DeviceID:  "device-1",
		Timestamp: time.Now(),
		Data: map[string]interface{}{
			"temperature": 25.5,
		},
	}
	testStore.StoreData(dataPoint)

	// Generate user token
	token, _ := auth.GenerateToken("user-1", "user@test.com", "user")

	r := gin.New()
	dataQueryGroup := r.Group("/data")
	dataQueryGroup.Use(auth.AuthMiddleware())
	{
		dataQueryGroup.GET("/:device_id", getLatestDataHandler)
	}

	req, _ := http.NewRequest("GET", "/data/device-1", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	testStore.Close()
}

// TestDeleteDevice tests device deletion
func TestDeleteDevice(t *testing.T) {
	gin.SetMode(gin.TestMode)
	testStore, _ := storage.New(":memory:", "", 7*24*time.Hour)
	store = testStore

	testStore.InitializeSystem("Test", false, 7)

	// Create admin user
	adminUser := &storage.User{
		ID:    "admin-1",
		Email: "admin@test.com",
		Role:  "admin",
	}
	testStore.CreateUser(adminUser)

	// Create device
	device := &storage.Device{
		ID:     "device-to-delete",
		UserID: "admin-1",
		Name:   "Delete Me",
	}
	testStore.CreateDevice(device)

	// Generate admin token
	token, _ := auth.GenerateToken("admin-1", "admin@test.com", "admin")

	r := gin.New()
	h := handlers.NewAdminHandler(testStore, nil)
	h.RegisterRoutes(r)

	req, _ := http.NewRequest("DELETE", "/admin/dev/device-to-delete", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify device is deleted
	_, err := testStore.GetDevice("device-to-delete")
	assert.Error(t, err)

	testStore.Close()
}

// TestGetDevice tests fetching single device
func TestGetDevice(t *testing.T) {
	gin.SetMode(gin.TestMode)
	testStore, _ := storage.New(":memory:", "", 7*24*time.Hour)
	store = testStore

	testStore.InitializeSystem("Test", false, 7)

	// Create admin user
	adminUser := &storage.User{
		ID:    "admin-1",
		Email: "admin@test.com",
		Role:  "admin",
	}
	testStore.CreateUser(adminUser)

	// Create device
	device := &storage.Device{
		ID:     "device-123",
		UserID: "admin-1",
		Name:   "Test Device",
		Type:   "sensor",
		Status: "active",
	}
	testStore.CreateDevice(device)

	// Generate admin token
	token, _ := auth.GenerateToken("admin-1", "admin@test.com", "admin")

	r := gin.New()
	h := handlers.NewAdminHandler(testStore, nil)
	h.RegisterRoutes(r)

	req, _ := http.NewRequest("GET", "/admin/dev/device-123", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "device-123", response["id"])
	assert.Equal(t, "Test Device", response["name"])
	assert.Equal(t, "sensor", response["type"])

	testStore.Close()
}

// TestUpdateDevice tests device update
func TestUpdateDevice(t *testing.T) {
	gin.SetMode(gin.TestMode)
	testStore, _ := storage.New(":memory:", "", 7*24*time.Hour)
	store = testStore

	testStore.InitializeSystem("Test", false, 7)

	// Create admin user
	adminUser := &storage.User{
		ID:    "admin-1",
		Email: "admin@test.com",
		Role:  "admin",
	}
	testStore.CreateUser(adminUser)

	// Create device
	device := &storage.Device{
		ID:     "device-update",
		UserID: "admin-1",
		Name:   "Old Name",
		Type:   "sensor",
	}
	testStore.CreateDevice(device)

	// Generate admin token
	token, _ := auth.GenerateToken("admin-1", "admin@test.com", "admin")

	r := gin.New()
	h := handlers.NewAdminHandler(testStore, nil)
	h.RegisterRoutes(r)

	updateData := map[string]interface{}{
		"status": "suspended",
	}

	body, _ := json.Marshal(updateData)
	req, _ := http.NewRequest("PUT", "/admin/dev/device-update", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "Device updated", response["message"])

	testStore.Close()
}

// TestDataHistory tests historical data query
func TestDataHistory(t *testing.T) {
	gin.SetMode(gin.TestMode)
	testStore, _ := storage.New(":memory:", "", 7*24*time.Hour)
	store = testStore

	testStore.InitializeSystem("Test", false, 7)

	// Create user and device
	user := &storage.User{
		ID:    "user-1",
		Email: "user@test.com",
		Role:  "user",
	}
	testStore.CreateUser(user)

	device := &storage.Device{
		ID:     "device-1",
		UserID: "user-1",
		Name:   "Test Device",
	}
	testStore.CreateDevice(device)

	// Generate user token
	token, _ := auth.GenerateToken("user-1", "user@test.com", "user")

	r := gin.New()
	dataQueryGroup := r.Group("/data")
	dataQueryGroup.Use(auth.AuthMiddleware())
	{
		dataQueryGroup.GET("/:device_id/history", getDataHistoryHandler)
	}

	req, _ := http.NewRequest("GET", "/data/device-1/history?start=2025-01-01T00:00:00Z&end=2025-12-31T23:59:59Z", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	testStore.Close()
}

// TestInvalidDeviceID tests operations with non-existent device
func TestInvalidDeviceID(t *testing.T) {
	gin.SetMode(gin.TestMode)
	testStore, _ := storage.New(":memory:", "", 7*24*time.Hour)
	store = testStore

	testStore.InitializeSystem("Test", false, 7)

	adminUser := &storage.User{
		ID:    "admin-1",
		Email: "admin@test.com",
		Role:  "admin",
	}
	testStore.CreateUser(adminUser)

	token, _ := auth.GenerateToken("admin-1", "admin@test.com", "admin")

	r := gin.New()
	h := handlers.NewAdminHandler(testStore, nil)
	h.RegisterRoutes(r)

	req, _ := http.NewRequest("GET", "/admin/dev/non-existent-device", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)

	testStore.Close()
}

// TestRateLimitingOnPublicEndpoint tests rate limiting
func TestRateLimitingOnPublicEndpoint(t *testing.T) {
	gin.SetMode(gin.TestMode)
	testStore, _ := storage.New(":memory:", "", 7*24*time.Hour)
	store = testStore

	r := gin.New()
	r.Use(auth.RateLimitMiddleware())
	r.POST("/pub/data/:device_id", postPublicDataHandler)

	dataPayload := map[string]interface{}{
		"value": 123,
	}

	// Make multiple requests to test rate limiting
	successCount := 0
	for i := 0; i < 150; i++ { // Try to exceed rate limit
		body, _ := json.Marshal(dataPayload)
		req, _ := http.NewRequest("POST", "/pub/data/test", bytes.NewBuffer(body))
		req.Header.Set("Content-Type", "application/json")
		req.RemoteAddr = "127.0.0.1:12345" // Consistent IP
		w := httptest.NewRecorder()
		r.ServeHTTP(w, req)

		if w.Code == http.StatusCreated || w.Code == http.StatusOK {
			successCount++
		}
	}

	// Should have some successful requests but not all
	assert.Greater(t, successCount, 0)
	assert.Less(t, successCount, 150)

	testStore.Close()
}
