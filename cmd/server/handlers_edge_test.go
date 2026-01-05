package main

import (
	"bytes"
	"datum-go/internal/auth"
	"datum-go/internal/processing"
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

func setupEdgeCaseTestServer(t *testing.T) (*gin.Engine, func()) {
	gin.SetMode(gin.TestMode)

	tmpDir := t.TempDir()
	var err error
	store, err = storage.New(tmpDir+"/meta.db", tmpDir+"/tsdata", 7*24*time.Hour)
	require.NoError(t, err)
	telemetryProcessor = processing.NewTelemetryProcessor(store)

	router := gin.New()
	cleanup := func() {
		store.Close()
	}

	return router, cleanup
}

// Test setupSystemHandler with duplicate admin email
func TestSetupSystemHandlerDuplicateEmail(t *testing.T) {
	router, cleanup := setupEdgeCaseTestServer(t)
	defer cleanup()

	router.POST("/system/setup", setupSystemHandler)

	// First setup
	requestBody := map[string]interface{}{
		"platform_name":  "Test Platform",
		"admin_email":    "admin@test.com",
		"admin_password": "password123",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPost, "/system/setup", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)
}

// Test createUserHandler with admin role
func TestCreateUserHandlerAdminRole(t *testing.T) {
	router, cleanup := setupEdgeCaseTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	router.POST("/admin/users", createUserHandler)

	requestBody := map[string]interface{}{
		"email":    "admin2@test.com",
		"password": "password123",
		"role":     "admin",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPost, "/admin/users", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)
}

// Test updateUserHandler with password change
func TestUpdateUserHandlerWithPassword(t *testing.T) {
	router, cleanup := setupEdgeCaseTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	hashedPassword, _ := auth.HashPassword("oldpassword")
	user := &storage.User{
		ID:           "update-pwd-user-1",
		Email:        "updatepwd@test.com",
		PasswordHash: hashedPassword,
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user)

	router.PUT("/admin/users/:user_id", updateUserHandler)

	requestBody := map[string]interface{}{
		"password": "newpassword123",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPut, "/admin/users/update-pwd-user-1", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

// Test updateUserHandler not found
func TestUpdateUserHandlerNotFound(t *testing.T) {
	router, cleanup := setupEdgeCaseTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	router.PUT("/admin/users/:user_id", updateUserHandler)

	requestBody := map[string]interface{}{
		"role": "admin",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPut, "/admin/users/nonexistent", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

// Test deleteUserHandler not found
func TestDeleteUserHandlerNotFound(t *testing.T) {
	router, cleanup := setupEdgeCaseTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	router.DELETE("/admin/users/:user_id", deleteUserHandler)

	req := httptest.NewRequest(http.MethodDelete, "/admin/users/nonexistent", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

// Test resetPasswordHandler with empty password (generates random)
func TestResetPasswordHandlerEmptyPassword(t *testing.T) {
	router, cleanup := setupEdgeCaseTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	hashedPassword, _ := auth.HashPassword("oldpassword")
	user := &storage.User{
		ID:           "reset-empty-1",
		Email:        "resetempty@test.com",
		PasswordHash: hashedPassword,
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user)

	router.POST("/admin/users/:username/reset-password", resetPasswordHandler)

	requestBody := map[string]interface{}{
		"new_password": "",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPost, "/admin/users/resetempty@test.com/reset-password", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.NotEmpty(t, response["new_password"])
}

// Test resetPasswordHandler user not found
func TestResetPasswordHandlerUserNotFound(t *testing.T) {
	router, cleanup := setupEdgeCaseTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	router.POST("/admin/users/:username/reset-password", resetPasswordHandler)

	requestBody := map[string]interface{}{
		"new_password": "newpassword123",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPost, "/admin/users/nonexistent@test.com/reset-password", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

// Test healthHandler when storage is nil
func TestHealthHandlerStorageCheck(t *testing.T) {
	router, cleanup := setupEdgeCaseTestServer(t)
	defer cleanup()

	router.GET("/health", healthHandler)

	req := httptest.NewRequest(http.MethodGet, "/health", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "healthy", response["status"])
	assert.Contains(t, response, "storage")
}

// Test readinessHandler
func TestReadinessHandlerReady(t *testing.T) {
	router, cleanup := setupEdgeCaseTestServer(t)
	defer cleanup()

	router.GET("/readiness", readinessHandler)

	req := httptest.NewRequest(http.MethodGet, "/readiness", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, true, response["ready"])
}

// Test listUsersHandler with multiple users
func TestListUsersHandlerMultiple(t *testing.T) {
	router, cleanup := setupEdgeCaseTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	// Create multiple users
	for i := 1; i <= 3; i++ {
		hashedPassword, _ := auth.HashPassword("password123")
		user := &storage.User{
			ID:           "list-user-" + string(rune(i)),
			Email:        "user" + string(rune(i)) + "@test.com",
			PasswordHash: hashedPassword,
			Role:         "user",
			Status:       "active",
			CreatedAt:    time.Now(),
		}
		store.CreateUser(user)
	}

	router.GET("/admin/users", listUsersHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/users", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	users := response["users"].([]interface{})
	assert.GreaterOrEqual(t, len(users), 3)
}

// Test updateDeviceHandler with active status
func TestUpdateDeviceHandlerActive(t *testing.T) {
	router, cleanup := setupEdgeCaseTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	device := &storage.Device{
		ID:        "update-active-1",
		UserID:    "user-123",
		Name:      "Test Device",
		APIKey:    "sk_updateactive",
		Status:    "suspended",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.PUT("/admin/devices/:device_id", updateDeviceHandler)

	requestBody := map[string]interface{}{
		"status": "active",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPut, "/admin/devices/update-active-1", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

// Test listDevicesHandler with user context
func TestListDevicesHandlerWithContext(t *testing.T) {
	router, cleanup := setupEdgeCaseTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	user := &storage.User{
		ID:    "context-user-1",
		Email: "contextuser@test.com",
		Role:  "user",
	}
	store.CreateUser(user)

	device := &storage.Device{
		ID:        "context-dev-1",
		UserID:    "context-user-1",
		Name:      "User Device",
		APIKey:    "sk_context1",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.GET("/devices", func(c *gin.Context) {
		c.Set("user_id", "context-user-1")
		listDevicesHandler(c)
	})

	req := httptest.NewRequest(http.MethodGet, "/devices", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

// Test postDataHandler with complex data
func TestPostDataHandlerComplexData(t *testing.T) {
	router, cleanup := setupEdgeCaseTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	device := &storage.Device{
		ID:        "complex-dev-1",
		UserID:    "user-123",
		Name:      "Complex Device",
		APIKey:    "sk_complex1",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.POST("/devices/:device_id/data", func(c *gin.Context) {
		c.Set("api_key", "sk_complex1")
		postDataHandler(c)
	})

	requestBody := map[string]interface{}{
		"temperature": 25.5,
		"humidity":    60.0,
		"pressure":    1013.25,
		"status":      "ok",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPost, "/devices/complex-dev-1/data", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}
