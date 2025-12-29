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

// setupTestEnvironment creates a test router with admin routes
func setupTestEnvironment(t *testing.T) (*gin.Engine, *storage.Storage, string) {
	gin.SetMode(gin.TestMode)

	// Create in-memory storage
	testStore, err := storage.New(":memory:", "", 7*24*time.Hour)
	assert.NoError(t, err)

	store = testStore // Set global store

	// Initialize system
	err = testStore.InitializeSystem("Test Platform", false, 7)
	assert.NoError(t, err)

	// Create admin user
	adminUser := &storage.User{
		ID:           "test-admin-id",
		Email:        "admin@test.com",
		PasswordHash: "hashed_password",
		Role:         "admin",
		Status:       "active",
		CreatedAt:    time.Now(),
		UpdatedAt:    time.Now(),
	}
	err = testStore.CreateUser(adminUser)
	assert.NoError(t, err)

	// Generate admin token
	token, err := auth.GenerateToken(adminUser.ID, adminUser.Email)
	assert.NoError(t, err)

	// Setup router
	r := gin.New()
	setupAdminRoutes(r, testStore)

	return r, testStore, token
}

// TestSystemSetup tests the system setup endpoint
func TestSystemSetup(t *testing.T) {
	gin.SetMode(gin.TestMode)
	testStore, _ := storage.New(":memory:", "", 7*24*time.Hour)
	store = testStore

	r := gin.New()
	r.POST("/system/setup", setupSystemHandler)

	setupData := map[string]interface{}{
		"platform_name":  "Test Platform",
		"admin_email":    "admin@test.com",
		"admin_password": "adminpass123",
		"allow_register": false,
		"data_retention": 7,
	}

	body, _ := json.Marshal(setupData)
	req, _ := http.NewRequest("POST", "/system/setup", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var response map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.NotEmpty(t, response["token"])
	assert.Equal(t, "admin@test.com", response["email"])
}

// TestSystemSetupAlreadyInitialized tests setup when system is already initialized
func TestSystemSetupAlreadyInitialized(t *testing.T) {
	r, _, _ := setupTestEnvironment(t)

	setupData := map[string]interface{}{
		"platform_name":  "Test Platform",
		"admin_email":    "admin2@test.com",
		"admin_password": "adminpass123",
	}

	body, _ := json.Marshal(setupData)
	req, _ := http.NewRequest("POST", "/system/setup", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusConflict, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response["error"], "already initialized")
}

// TestGetSystemStatus tests the system status endpoint
func TestGetSystemStatus(t *testing.T) {
	r, testStore, _ := setupTestEnvironment(t)

	req, _ := http.NewRequest("GET", "/system/status", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.Equal(t, true, response["initialized"])
	// Platform details are hidden when initialized
	// assert.NotNil(t, response["platform_name"])
	// assert.NotNil(t, response["setup_at"])

	testStore.Close()
}

// TestProvisionDevice tests device provisioning
func TestProvisionDevice(t *testing.T) {
	r, testStore, token := setupTestEnvironment(t)

	deviceData := map[string]interface{}{
		"name": "Test Device",
		"type": "sensor",
	}

	body, _ := json.Marshal(deviceData)
	req, _ := http.NewRequest("POST", "/admin/devices", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var response map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.NotEmpty(t, response["device_id"])
	assert.NotEmpty(t, response["api_key"])
	assert.Equal(t, "Test Device", response["name"])
	assert.Equal(t, "sensor", response["type"])

	testStore.Close()
}

// TestProvisionDeviceWithCustomID tests device provisioning with custom ID
func TestProvisionDeviceWithCustomID(t *testing.T) {
	r, testStore, token := setupTestEnvironment(t)

	deviceData := map[string]interface{}{
		"device_id": "custom-device-123",
		"name":      "Custom Device",
		"type":      "temperature",
	}

	body, _ := json.Marshal(deviceData)
	req, _ := http.NewRequest("POST", "/admin/devices", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "custom-device-123", response["device_id"])
	assert.Equal(t, "Custom Device", response["name"])
	assert.Equal(t, "temperature", response["type"])

	testStore.Close()
}

// TestProvisionDeviceDuplicateID tests duplicate device ID error
func TestProvisionDeviceDuplicateID(t *testing.T) {
	r, testStore, token := setupTestEnvironment(t)

	deviceData := map[string]interface{}{
		"device_id": "duplicate-device",
		"name":      "Device 1",
	}

	// Create first device
	body, _ := json.Marshal(deviceData)
	req, _ := http.NewRequest("POST", "/admin/devices", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)
	assert.Equal(t, http.StatusCreated, w.Code)

	// Try to create duplicate
	req, _ = http.NewRequest("POST", "/admin/devices", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+token)
	w = httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusConflict, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response["error"], "already exists")

	testStore.Close()
}

// TestListAllDevices tests listing all devices
func TestListAllDevices(t *testing.T) {
	r, testStore, token := setupTestEnvironment(t)

	// Create test devices
	devices := []map[string]interface{}{
		{"name": "Device 1", "type": "sensor"},
		{"name": "Device 2", "type": "temperature"},
		{"name": "Device 3", "type": "humidity"},
	}

	for _, deviceData := range devices {
		body, _ := json.Marshal(deviceData)
		req, _ := http.NewRequest("POST", "/admin/devices", bytes.NewBuffer(body))
		req.Header.Set("Content-Type", "application/json")
		req.Header.Set("Authorization", "Bearer "+token)
		w := httptest.NewRecorder()
		r.ServeHTTP(w, req)
		assert.Equal(t, http.StatusCreated, w.Code)
	}

	// List devices
	req, _ := http.NewRequest("GET", "/admin/devices", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	devicesArray := response["devices"].([]interface{})
	assert.Equal(t, 3, len(devicesArray))

	testStore.Close()
}

// TestCreateUser tests user creation
func TestCreateUser(t *testing.T) {
	r, testStore, token := setupTestEnvironment(t)

	userData := map[string]interface{}{
		"email":    "newuser@test.com",
		"password": "userpass123",
		"role":     "user",
	}

	body, _ := json.Marshal(userData)
	req, _ := http.NewRequest("POST", "/admin/users", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "newuser@test.com", response["email"])
	assert.Equal(t, "user", response["role"])

	testStore.Close()
}

// TestListUsers tests user listing
func TestListUsers(t *testing.T) {
	r, testStore, token := setupTestEnvironment(t)

	// Create additional users
	users := []map[string]interface{}{
		{"email": "user1@test.com", "password": "password123", "role": "user"},
		{"email": "user2@test.com", "password": "password123", "role": "user"},
	}

	for _, userData := range users {
		body, _ := json.Marshal(userData)
		req, _ := http.NewRequest("POST", "/admin/users", bytes.NewBuffer(body))
		req.Header.Set("Content-Type", "application/json")
		req.Header.Set("Authorization", "Bearer "+token)
		w := httptest.NewRecorder()
		r.ServeHTTP(w, req)
		assert.Equal(t, http.StatusCreated, w.Code)
	}

	// List users
	req, _ := http.NewRequest("GET", "/admin/users", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	usersArray := response["users"].([]interface{})
	assert.GreaterOrEqual(t, len(usersArray), 3) // admin + 2 created users

	testStore.Close()
}

// TestResetPassword tests password reset functionality
func TestResetPassword(t *testing.T) {
	r, testStore, token := setupTestEnvironment(t)

	// Create user first
	userData := map[string]interface{}{
		"email":    "resetuser@test.com",
		"password": "oldpass123",
		"role":     "user",
	}
	body, _ := json.Marshal(userData)
	req, _ := http.NewRequest("POST", "/admin/users", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)
	assert.Equal(t, http.StatusCreated, w.Code)

	// Reset password
	resetData := map[string]interface{}{
		"new_password": "newpass456",
	}
	body, _ = json.Marshal(resetData)
	req, _ = http.NewRequest("POST", "/admin/users/resetuser@test.com/reset-password", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+token)
	w = httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response["message"], "reset")

	testStore.Close()
}

// TestDeleteUser tests user deletion
func TestDeleteUser(t *testing.T) {
	r, testStore, token := setupTestEnvironment(t)

	// Create user first
	userData := map[string]interface{}{
		"email":    "deleteuser@test.com",
		"password": "password123",
		"role":     "user",
	}
	body, _ := json.Marshal(userData)
	req, _ := http.NewRequest("POST", "/admin/users", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)
	assert.Equal(t, http.StatusCreated, w.Code)

	var createResponse map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &createResponse)
	userID := createResponse["user_id"].(string)

	// Delete user
	req, _ = http.NewRequest("DELETE", "/admin/users/"+userID, nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w = httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	testStore.Close()
}

// TestDatabaseReset tests system reset
func TestDatabaseReset(t *testing.T) {
	r, testStore, token := setupTestEnvironment(t)

	resetData := map[string]interface{}{
		"confirm": "RESET",
	}

	body, _ := json.Marshal(resetData)
	req, _ := http.NewRequest("DELETE", "/admin/database/reset", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response["message"], "reset")

	testStore.Close()
}

// TestDatabaseResetInvalidConfirmation tests reset with wrong confirmation
func TestDatabaseResetInvalidConfirmation(t *testing.T) {
	r, testStore, token := setupTestEnvironment(t)

	resetData := map[string]interface{}{
		"confirm": "WRONG",
	}

	body, _ := json.Marshal(resetData)
	req, _ := http.NewRequest("DELETE", "/admin/database/reset", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)

	testStore.Close()
}

// TestUnauthorizedAccess tests accessing admin endpoint without token
func TestUnauthorizedAccess(t *testing.T) {
	r, testStore, _ := setupTestEnvironment(t)

	req, _ := http.NewRequest("GET", "/admin/devices", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)

	testStore.Close()
}

// TestInvalidToken tests accessing with invalid token
func TestInvalidToken(t *testing.T) {
	r, testStore, _ := setupTestEnvironment(t)

	req, _ := http.NewRequest("GET", "/admin/devices", nil)
	req.Header.Set("Authorization", "Bearer invalid-token-123")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)

	testStore.Close()
}

// TestGetDatabaseStats tests database statistics endpoint
func TestGetDatabaseStats(t *testing.T) {
	r, testStore, token := setupTestEnvironment(t)

	req, _ := http.NewRequest("GET", "/admin/database/stats", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.NotNil(t, response["total_users"])
	assert.NotNil(t, response["total_devices"])

	testStore.Close()
}
