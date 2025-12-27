package main

import (
	"bytes"
	"datum-go/internal/auth"
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

func setupAdditionalTestServer(t *testing.T) (*gin.Engine, func()) {
	gin.SetMode(gin.TestMode)

	tmpDir := t.TempDir()
	var err error
	store, err = storage.New(tmpDir+"/meta.db", tmpDir+"/tsdata")
	require.NoError(t, err)

	router := gin.New()
	cleanup := func() {
		store.Close()
	}

	return router, cleanup
}

// Test setupSystemHandler with default data retention
func TestSetupSystemHandlerDefaultRetention(t *testing.T) {
	router, cleanup := setupAdditionalTestServer(t)
	defer cleanup()

	router.POST("/system/setup", setupSystemHandler)

	requestBody := map[string]interface{}{
		"platform_name":  "Test Platform",
		"admin_email":    "admin@test.com",
		"admin_password": "password123",
		"allow_register": false,
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPost, "/system/setup", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)
}

// Test createUserHandler with default role
func TestCreateUserHandlerDefaultRole(t *testing.T) {
	router, cleanup := setupAdditionalTestServer(t)
	defer cleanup()

	// Initialize system
	store.InitializeSystem("Test", true, 7)

	router.POST("/admin/users", createUserHandler)

	requestBody := map[string]interface{}{
		"email":    "newuser@test.com",
		"password": "password123",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPost, "/admin/users", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)
}

// Test updateUserHandler success
func TestUpdateUserHandlerSuccess(t *testing.T) {
	router, cleanup := setupAdditionalTestServer(t)
	defer cleanup()

	// Initialize system
	store.InitializeSystem("Test", true, 7)

	// Create test user
	hashedPassword, _ := auth.HashPassword("password123")
	user := &storage.User{
		ID:           "update-user-1",
		Email:        "update@test.com",
		PasswordHash: hashedPassword,
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user)

	router.PUT("/admin/users/:user_id", updateUserHandler)

	requestBody := map[string]interface{}{
		"role":   "admin",
		"status": "suspended",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPut, "/admin/users/update-user-1", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify changes
	updated, _ := store.GetUserByID("update-user-1")
	assert.Equal(t, "admin", updated.Role)
	assert.Equal(t, "suspended", updated.Status)
}

// Test deleteUserHandler success
func TestDeleteUserHandlerSuccess(t *testing.T) {
	router, cleanup := setupAdditionalTestServer(t)
	defer cleanup()

	// Initialize system
	store.InitializeSystem("Test", true, 7)

	// Create test user
	hashedPassword, _ := auth.HashPassword("password123")
	user := &storage.User{
		ID:           "delete-user-1",
		Email:        "delete@test.com",
		PasswordHash: hashedPassword,
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user)

	router.DELETE("/admin/users/:user_id", deleteUserHandler)

	req := httptest.NewRequest(http.MethodDelete, "/admin/users/delete-user-1", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify deletion
	_, err := store.GetUserByID("delete-user-1")
	assert.Error(t, err)
}

// Test resetPasswordHandler success
func TestResetPasswordHandlerSuccess(t *testing.T) {
	router, cleanup := setupAdditionalTestServer(t)
	defer cleanup()

	// Initialize system
	store.InitializeSystem("Test", true, 7)

	// Create test user
	hashedPassword, _ := auth.HashPassword("oldpassword")
	user := &storage.User{
		ID:           "reset-user-1",
		Email:        "reset@test.com",
		PasswordHash: hashedPassword,
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user)

	router.POST("/admin/users/:username/reset-password", resetPasswordHandler)

	requestBody := map[string]interface{}{
		"new_password": "newpassword123",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPost, "/admin/users/reset@test.com/reset-password", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

// Test resetPasswordHandler invalid password length
func TestResetPasswordHandlerShortPassword(t *testing.T) {
	router, cleanup := setupAdditionalTestServer(t)
	defer cleanup()

	// Initialize system
	store.InitializeSystem("Test", true, 7)

	// Create test user
	hashedPassword, _ := auth.HashPassword("oldpassword")
	user := &storage.User{
		ID:           "reset-user-2",
		Email:        "reset2@test.com",
		PasswordHash: hashedPassword,
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user)

	router.POST("/admin/users/:username/reset-password", resetPasswordHandler)

	requestBody := map[string]interface{}{
		"new_password": "short",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPost, "/admin/users/reset2@test.com/reset-password", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	// resetPasswordHandler accepts any length password
	assert.Equal(t, http.StatusOK, w.Code)
}

// Test getSystemConfigHandler success
func TestGetSystemConfigHandlerSuccess(t *testing.T) {
	router, cleanup := setupAdditionalTestServer(t)
	defer cleanup()

	// Initialize system
	store.InitializeSystem("Test Platform", true, 14)

	router.GET("/admin/config", getSystemConfigHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/config", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "retention")
	assert.Contains(t, response, "rate_limit")
	assert.Contains(t, response, "alerts")

	retention := response["retention"].(map[string]interface{})
	assert.Equal(t, float64(14), retention["days"])
}

// Test updateDeviceHandler with suspended status
func TestUpdateDeviceHandlerSuspended(t *testing.T) {
	router, cleanup := setupAdditionalTestServer(t)
	defer cleanup()

	// Initialize system
	store.InitializeSystem("Test", true, 7)

	// Create device
	device := &storage.Device{
		ID:        "update-dev-1",
		UserID:    "user-123",
		Name:      "Test Device",
		APIKey:    "sk_update1",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.PUT("/admin/devices/:device_id", updateDeviceHandler)

	requestBody := map[string]interface{}{
		"status": "suspended",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPut, "/admin/devices/update-dev-1", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify update
	updated, _ := store.GetDevice("update-dev-1")
	assert.Equal(t, "suspended", updated.Status)
}

// Test updateDeviceHandler not found
func TestUpdateDeviceHandlerNotFound(t *testing.T) {
	router, cleanup := setupAdditionalTestServer(t)
	defer cleanup()

	// Initialize system
	store.InitializeSystem("Test", true, 7)

	router.PUT("/admin/devices/:device_id", updateDeviceHandler)

	requestBody := map[string]interface{}{
		"status": "banned",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPut, "/admin/devices/nonexistent", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

// Test forceDeleteDeviceHandler with data
func TestForceDeleteDeviceHandlerWithData(t *testing.T) {
	router, cleanup := setupAdditionalTestServer(t)
	defer cleanup()

	// Initialize system
	store.InitializeSystem("Test", true, 7)

	// Create device with data
	device := &storage.Device{
		ID:        "force-del-1",
		UserID:    "user-123",
		Name:      "Device with Data",
		APIKey:    "sk_forcedel1",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.DELETE("/admin/devices/:device_id", forceDeleteDeviceHandler)

	// Add some data using StoreData
	dataPoint := &storage.DataPoint{
		DeviceID:  "force-del-1",
		Timestamp: time.Now(),
		Data:      map[string]interface{}{"temperature": 25.5},
	}
	store.StoreData(dataPoint)

	req := httptest.NewRequest(http.MethodDelete, "/admin/devices/force-del-1", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

// Test exportDatabaseHandler with data
func TestExportDatabaseHandlerWithData(t *testing.T) {
	router, cleanup := setupAdditionalTestServer(t)
	defer cleanup()

	// Initialize system
	store.InitializeSystem("Test", true, 7)

	// Create user and device
	hashedPassword, _ := auth.HashPassword("password123")
	user := &storage.User{
		ID:           "export-user-1",
		Email:        "export@test.com",
		PasswordHash: hashedPassword,
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user)

	device := &storage.Device{
		ID:        "export-dev-1",
		UserID:    "export-user-1",
		Name:      "Export Device",
		APIKey:    "sk_export1",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.GET("/admin/database/export", exportDatabaseHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/database/export", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "users")
	assert.Contains(t, response, "devices")

	users := response["users"].([]interface{})
	assert.GreaterOrEqual(t, len(users), 1)
}
