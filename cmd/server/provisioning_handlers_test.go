package main

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
)

// setupProvisioningTestEnvironment creates a test router with provisioning routes
func setupProvisioningTestEnvironment(t *testing.T) (*gin.Engine, *storage.Storage, string, string) {
	gin.SetMode(gin.TestMode)

	// Create in-memory storage
	testStore, err := storage.New(":memory:", "", 7*24*time.Hour)
	assert.NoError(t, err)

	store = testStore // Set global store

	// Initialize system
	err = testStore.InitializeSystem("Test Platform", false, 7)
	assert.NoError(t, err)

	// Create test user
	testUser := &storage.User{
		ID:           "test-user-id",
		Email:        "user@test.com",
		PasswordHash: "hashed_password",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
		UpdatedAt:    time.Now(),
	}
	err = testStore.CreateUser(testUser)
	assert.NoError(t, err)

	// Generate user token
	token, err := auth.GenerateToken(testUser.ID, testUser.Email)
	assert.NoError(t, err)

	// Setup router
	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", testUser.ID)
		c.Next()
	})
	RegisterProvisioningRoutes(r, auth.AuthMiddleware())

	return r, testStore, token, testUser.ID
}

// TestRegisterDeviceHandler tests device registration
func TestRegisterDeviceHandler(t *testing.T) {
	r, testStore, token, userID := setupProvisioningTestEnvironment(t)

	reqData := RegisterDeviceRequest{
		DeviceUID:  "AA:BB:CC:DD:EE:FF",
		DeviceName: "Test Device",
		DeviceType: "ESP32",
		WiFiSSID:   "TestWiFi",
		WiFiPass:   "testpass123",
	}

	body, _ := json.Marshal(reqData)
	req, _ := http.NewRequest("POST", "/devices/register", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var response RegisterDeviceResponse
	err := json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.NotEmpty(t, response.RequestID)
	assert.NotEmpty(t, response.DeviceID)
	assert.NotEmpty(t, response.APIKey)
	assert.Equal(t, "AABBCCDDEEFF", response.DeviceUID) // Normalized
	assert.Equal(t, "TestWiFi", response.WiFiSSID)
	assert.Equal(t, "pending", response.Status)

	// Verify in database
	provReq, err := testStore.GetProvisioningRequest(response.RequestID)
	assert.NoError(t, err)
	assert.Equal(t, userID, provReq.UserID)
	assert.Equal(t, "Test Device", provReq.DeviceName)
}

// TestRegisterDeviceHandlerDuplicateUID tests registering with existing UID
func TestRegisterDeviceHandlerDuplicateUID(t *testing.T) {
	r, testStore, token, userID := setupProvisioningTestEnvironment(t)

	// First registration
	reqData := RegisterDeviceRequest{
		DeviceUID:  "AA:BB:CC:DD:EE:FF",
		DeviceName: "Test Device",
		DeviceType: "ESP32",
	}

	provReq := &storage.ProvisioningRequest{
		ID:         "prov_123",
		DeviceUID:  "AABBCCDDEEFF",
		UserID:     userID,
		DeviceName: "Test Device",
		DeviceType: "ESP32",
		Status:     "pending",
		DeviceID:   "dev_123",
		APIKey:     "dk_test_key",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err := testStore.CreateProvisioningRequest(provReq)
	assert.NoError(t, err)

	// Complete provisioning to register device
	_, err = testStore.CompleteProvisioningRequest("prov_123")
	assert.NoError(t, err)

	// Try to register again with same UID
	body, _ := json.Marshal(reqData)
	req, _ := http.NewRequest("POST", "/devices/register", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusConflict, w.Code)

	var response map[string]interface{}
	err = json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.Contains(t, response["error"], "already registered")
}

// TestRegisterDeviceHandlerInvalidRequest tests invalid registration data
func TestRegisterDeviceHandlerInvalidRequest(t *testing.T) {
	r, _, token, _ := setupProvisioningTestEnvironment(t)

	// Missing required fields
	reqData := map[string]interface{}{
		"device_name": "Test Device",
		// Missing device_uid
	}

	body, _ := json.Marshal(reqData)
	req, _ := http.NewRequest("POST", "/devices/register", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

// TestCheckDeviceUIDHandler tests UID checking
func TestCheckDeviceUIDHandler(t *testing.T) {
	r, _, token, _ := setupProvisioningTestEnvironment(t)

	// Check unregistered UID
	req, _ := http.NewRequest("GET", "/devices/check-uid/AA:BB:CC:DD:EE:FF", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response CheckUIDResponse
	err := json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.False(t, response.Registered)
	assert.False(t, response.HasPending)
}

// TestCheckDeviceUIDHandlerWithPending tests UID with pending request
func TestCheckDeviceUIDHandlerWithPending(t *testing.T) {
	r, testStore, token, userID := setupProvisioningTestEnvironment(t)

	// Create pending request
	provReq := &storage.ProvisioningRequest{
		ID:         "prov_123",
		DeviceUID:  "AABBCCDDEEFF",
		UserID:     userID,
		DeviceName: "Test Device",
		Status:     "pending",
		DeviceID:   "dev_123",
		APIKey:     "dk_test_key",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err := testStore.CreateProvisioningRequest(provReq)
	assert.NoError(t, err)

	// Check UID
	req, _ := http.NewRequest("GET", "/devices/check-uid/AA:BB:CC:DD:EE:FF", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response CheckUIDResponse
	err = json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.False(t, response.Registered)
	assert.True(t, response.HasPending)
	assert.Equal(t, "prov_123", response.RequestID)
}

// TestListProvisioningRequestsHandler tests listing provisioning requests
func TestListProvisioningRequestsHandler(t *testing.T) {
	r, testStore, token, userID := setupProvisioningTestEnvironment(t)

	// Create multiple provisioning requests
	provReq1 := &storage.ProvisioningRequest{
		ID:         "prov_1",
		DeviceUID:  "UID1",
		UserID:     userID,
		DeviceName: "Device 1",
		Status:     "pending",
		DeviceID:   "dev_1",
		APIKey:     "dk_1",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err := testStore.CreateProvisioningRequest(provReq1)
	assert.NoError(t, err)

	provReq2 := &storage.ProvisioningRequest{
		ID:         "prov_2",
		DeviceUID:  "UID2",
		UserID:     userID,
		DeviceName: "Device 2",
		Status:     "completed",
		DeviceID:   "dev_2",
		APIKey:     "dk_2",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err = testStore.CreateProvisioningRequest(provReq2)
	assert.NoError(t, err)

	// List requests
	req, _ := http.NewRequest("GET", "/devices/provisioning", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	err = json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	requests := response["requests"].([]interface{})
	assert.Len(t, requests, 2)
}

// TestGetProvisioningStatusHandler tests getting provisioning status
func TestGetProvisioningStatusHandler(t *testing.T) {
	r, testStore, token, userID := setupProvisioningTestEnvironment(t)

	// Create provisioning request
	provReq := &storage.ProvisioningRequest{
		ID:         "prov_123",
		DeviceUID:  "UID1",
		UserID:     userID,
		DeviceName: "Device 1",
		Status:     "pending",
		DeviceID:   "dev_1",
		APIKey:     "dk_1",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err := testStore.CreateProvisioningRequest(provReq)
	assert.NoError(t, err)

	// Get status
	req, _ := http.NewRequest("GET", "/devices/provisioning/prov_123", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	err = json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.Equal(t, "prov_123", response["request_id"])
	assert.Equal(t, "Device 1", response["device_name"])
	assert.Equal(t, "pending", response["status"])
}

// TestGetProvisioningStatusHandlerNotFound tests getting non-existent request
func TestGetProvisioningStatusHandlerNotFound(t *testing.T) {
	r, _, token, _ := setupProvisioningTestEnvironment(t)

	req, _ := http.NewRequest("GET", "/devices/provisioning/nonexistent", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

// TestCancelProvisioningHandler tests canceling a provisioning request
func TestCancelProvisioningHandler(t *testing.T) {
	r, testStore, token, userID := setupProvisioningTestEnvironment(t)

	// Create provisioning request
	provReq := &storage.ProvisioningRequest{
		ID:         "prov_123",
		DeviceUID:  "UID1",
		UserID:     userID,
		DeviceName: "Device 1",
		Status:     "pending",
		DeviceID:   "dev_1",
		APIKey:     "dk_1",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err := testStore.CreateProvisioningRequest(provReq)
	assert.NoError(t, err)

	// Cancel request
	req, _ := http.NewRequest("DELETE", "/devices/provisioning/prov_123", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify cancelled
	updated, err := testStore.GetProvisioningRequest("prov_123")
	assert.NoError(t, err)
	assert.Equal(t, "cancelled", updated.Status)
}

// TestDeviceActivateHandler tests device activation
func TestDeviceActivateHandler(t *testing.T) {
	r, testStore, _, userID := setupProvisioningTestEnvironment(t)

	// Create pending provisioning request
	provReq := &storage.ProvisioningRequest{
		ID:         "prov_123",
		DeviceUID:  "AABBCCDDEEFF",
		UserID:     userID,
		DeviceName: "Test Device",
		Status:     "pending",
		DeviceID:   "dev_123",
		APIKey:     "dk_test_key",
		ServerURL:  "http://localhost:8000",
		WiFiSSID:   "TestWiFi",
		WiFiPass:   "testpass123",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err := testStore.CreateProvisioningRequest(provReq)
	assert.NoError(t, err)

	// Activate device
	activateReq := DeviceActivateRequest{
		DeviceUID:       "AA:BB:CC:DD:EE:FF",
		FirmwareVersion: "1.0.0",
		Model:           "ESP32-S3",
	}

	body, _ := json.Marshal(activateReq)
	req, _ := http.NewRequest("POST", "/provisioning/activate", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response DeviceActivateResponse
	err = json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.Equal(t, "dev_123", response.DeviceID)
	assert.Equal(t, "dk_test_key", response.APIKey)
	assert.Equal(t, "TestWiFi", response.WiFiSSID)
	assert.Equal(t, "testpass123", response.WiFiPass)

	// Verify device created
	device, err := testStore.GetDevice("dev_123")
	assert.NoError(t, err)
	assert.Equal(t, "Test Device", device.Name)
	assert.Equal(t, userID, device.UserID)
}

// TestDeviceActivateHandlerNoPendingRequest tests activation without request
func TestDeviceActivateHandlerNoPendingRequest(t *testing.T) {
	r, _, _, _ := setupProvisioningTestEnvironment(t)

	activateReq := DeviceActivateRequest{
		DeviceUID:       "AA:BB:CC:DD:EE:FF",
		FirmwareVersion: "1.0.0",
	}

	body, _ := json.Marshal(activateReq)
	req, _ := http.NewRequest("POST", "/provisioning/activate", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)

	var response map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.Contains(t, response["error"], "no provisioning request found")
}

// TestDeviceActivateHandlerExpiredRequest tests activation with expired request
func TestDeviceActivateHandlerExpiredRequest(t *testing.T) {
	r, testStore, _, userID := setupProvisioningTestEnvironment(t)

	// Create expired provisioning request
	provReq := &storage.ProvisioningRequest{
		ID:         "prov_123",
		DeviceUID:  "AABBCCDDEEFF",
		UserID:     userID,
		DeviceName: "Test Device",
		Status:     "pending",
		DeviceID:   "dev_123",
		APIKey:     "dk_test_key",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(-1 * time.Hour), // Expired
		CreatedAt:  time.Now().Add(-2 * time.Hour),
	}
	err := testStore.CreateProvisioningRequest(provReq)
	assert.NoError(t, err)

	// Try to activate
	activateReq := DeviceActivateRequest{
		DeviceUID: "AA:BB:CC:DD:EE:FF",
	}

	body, _ := json.Marshal(activateReq)
	req, _ := http.NewRequest("POST", "/provisioning/activate", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusGone, w.Code)

	var response map[string]interface{}
	err = json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.Contains(t, response["error"], "expired")
}

// TestDeviceActivateHandlerAlreadyRegistered tests activation of already registered device
func TestDeviceActivateHandlerAlreadyRegistered(t *testing.T) {
	r, testStore, _, userID := setupProvisioningTestEnvironment(t)

	// Create and complete provisioning
	provReq := &storage.ProvisioningRequest{
		ID:         "prov_123",
		DeviceUID:  "AABBCCDDEEFF",
		UserID:     userID,
		DeviceName: "Test Device",
		Status:     "pending",
		DeviceID:   "dev_123",
		APIKey:     "dk_test_key",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err := testStore.CreateProvisioningRequest(provReq)
	assert.NoError(t, err)
	_, err = testStore.CompleteProvisioningRequest("prov_123")
	assert.NoError(t, err)

	// Try to activate again
	activateReq := DeviceActivateRequest{
		DeviceUID: "AA:BB:CC:DD:EE:FF",
	}

	body, _ := json.Marshal(activateReq)
	req, _ := http.NewRequest("POST", "/provisioning/activate", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusConflict, w.Code)

	var response map[string]interface{}
	err = json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.Contains(t, response["error"], "already registered")
}

// TestDeviceCheckHandler tests device provisioning check
func TestDeviceCheckHandler(t *testing.T) {
	r, testStore, _, userID := setupProvisioningTestEnvironment(t)

	// Create pending provisioning request
	provReq := &storage.ProvisioningRequest{
		ID:         "prov_123",
		DeviceUID:  "AABBCCDDEEFF",
		UserID:     userID,
		DeviceName: "Test Device",
		Status:     "pending",
		DeviceID:   "dev_123",
		APIKey:     "dk_test_key",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err := testStore.CreateProvisioningRequest(provReq)
	assert.NoError(t, err)

	// Check for provisioning request
	req, _ := http.NewRequest("GET", "/provisioning/check/AA:BB:CC:DD:EE:FF", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	err = json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.Equal(t, "pending", response["status"])
	assert.NotEmpty(t, response["activate_url"])
}

// TestDeviceCheckHandlerNoPending tests check with no pending request
func TestDeviceCheckHandlerNoPending(t *testing.T) {
	r, _, _, _ := setupProvisioningTestEnvironment(t)

	req, _ := http.NewRequest("GET", "/provisioning/check/AA:BB:CC:DD:EE:FF", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)

	var response map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.Equal(t, "unconfigured", response["status"])
}

// TestNormalizeUID tests UID normalization
func TestNormalizeUID(t *testing.T) {
	tests := []struct {
		input    string
		expected string
	}{
		{"aa:bb:cc:dd:ee:ff", "AABBCCDDEEFF"},
		{"AA-BB-CC-DD-EE-FF", "AABBCCDDEEFF"},
		{"AA BB CC DD EE FF", "AABBCCDDEEFF"},
		{"aabbccddeeff", "AABBCCDDEEFF"},
		{"AB:CD:EF:12:34:56", "ABCDEF123456"},
	}

	for _, tt := range tests {
		result := normalizeUID(tt.input)
		assert.Equal(t, tt.expected, result, "Input: %s", tt.input)
	}
}

// TestGenerateProvisioningID tests ID generation
func TestGenerateProvisioningID(t *testing.T) {
	id1 := generateProvisioningID("prov")
	id2 := generateProvisioningID("prov")

	assert.NotEqual(t, id1, id2, "IDs should be unique")
	assert.Contains(t, id1, "prov_")
	assert.Greater(t, len(id1), 10)
}

// TestGenerateProvisioningAPIKey tests API key generation
func TestGenerateProvisioningAPIKey(t *testing.T) {
	key1 := generateProvisioningAPIKey()
	key2 := generateProvisioningAPIKey()

	assert.NotEqual(t, key1, key2, "Keys should be unique")
	assert.Contains(t, key1, "dk_")
	assert.Greater(t, len(key1), 10)
}

// TestSetProvisioningServerURL tests setting server URL
func TestSetProvisioningServerURL(t *testing.T) {
	originalURL := provisioningConfig.ServerURL
	defer func() { provisioningConfig.ServerURL = originalURL }()

	newURL := "https://example.com"
	SetProvisioningServerURL(newURL)
	assert.Equal(t, newURL, provisioningConfig.ServerURL)
}
