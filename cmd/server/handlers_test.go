package main

import (
	"bytes"
	"datum-go/internal/auth"
	"datum-go/internal/storage"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func setupTestServerHandlers(t *testing.T) *gin.Engine {
	gin.SetMode(gin.TestMode)

	// Create temporary storage
	tmpDir := t.TempDir()
	var err error
	store, err = storage.New(tmpDir+"/meta.db", tmpDir+"/tsdata")
	require.NoError(t, err)

	// Create test admin user
	adminUser := &storage.User{
		ID:           "admin-user-001",
		Email:        "admin@test.com",
		PasswordHash: "hashed_password",
		Role:         "admin",
		Status:       "active",
		CreatedAt:    time.Now(),
		UpdatedAt:    time.Now(),
	}
	err = store.CreateUser(adminUser)
	require.NoError(t, err)

	// Create test regular user
	regularUser := &storage.User{
		ID:           "regular-user-001",
		Email:        "user@test.com",
		PasswordHash: "hashed_password",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
		UpdatedAt:    time.Now(),
	}
	err = store.CreateUser(regularUser)
	require.NoError(t, err)

	router := gin.New()
	return router
}

func TestGetUserHandler(t *testing.T) {
	router := setupTestServerHandlers(t)
	defer store.Close()

	router.GET("/admin/users/:user_id", getUserHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/users/regular-user-001", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	require.NoError(t, err)

	assert.Equal(t, "regular-user-001", response["id"])
	assert.Equal(t, "user@test.com", response["email"])
	assert.Equal(t, "user", response["role"])
	assert.Equal(t, "active", response["status"])
}

func TestGetUserHandlerNotFound(t *testing.T) {
	router := setupTestServerHandlers(t)
	defer store.Close()

	router.GET("/admin/users/:user_id", getUserHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/users/nonexistent", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func TestUpdateUserHandlerRole(t *testing.T) {
	router := setupTestServerHandlers(t)
	defer store.Close()

	router.PUT("/admin/users/:user_id", func(c *gin.Context) {
		c.Set("user_id", "admin-user-001") // Simulate authenticated admin
		updateUserHandler(c)
	})

	body := map[string]interface{}{
		"role": "admin",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/users/regular-user-001", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify update
	user, _ := store.GetUserByID("regular-user-001")
	assert.Equal(t, "admin", user.Role)
}

func TestUpdateUserHandlerStatus(t *testing.T) {
	router := setupTestServerHandlers(t)
	defer store.Close()

	router.PUT("/admin/users/:user_id", func(c *gin.Context) {
		c.Set("user_id", "admin-user-001") // Simulate authenticated admin
		updateUserHandler(c)
	})

	body := map[string]interface{}{
		"status": "suspended",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/users/regular-user-001", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify update
	user, _ := store.GetUserByID("regular-user-001")
	assert.Equal(t, "suspended", user.Status)
}

func TestUpdateUserHandlerInvalidRole(t *testing.T) {
	router := setupTestServerHandlers(t)
	defer store.Close()

	router.PUT("/admin/users/:user_id", func(c *gin.Context) {
		c.Set("user_id", "admin-user-001")
		updateUserHandler(c)
	})

	body := map[string]interface{}{
		"role": "superadmin", // Invalid role
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/users/regular-user-001", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
	assert.Contains(t, w.Body.String(), "Invalid role")
}

func TestUpdateUserHandlerInvalidStatus(t *testing.T) {
	router := setupTestServerHandlers(t)
	defer store.Close()

	router.PUT("/admin/users/:user_id", func(c *gin.Context) {
		c.Set("user_id", "admin-user-001")
		updateUserHandler(c)
	})

	body := map[string]interface{}{
		"status": "banned", // Invalid status
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/users/regular-user-001", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
	assert.Contains(t, w.Body.String(), "Invalid status")
}

func TestUpdateUserHandlerCannotSuspendSelf(t *testing.T) {
	router := setupTestServerHandlers(t)
	defer store.Close()

	router.PUT("/admin/users/:user_id", func(c *gin.Context) {
		c.Set("user_id", "admin-user-001")
		updateUserHandler(c)
	})

	body := map[string]interface{}{
		"status": "suspended",
	}
	bodyBytes, _ := json.Marshal(body)

	// Try to suspend own account
	req := httptest.NewRequest(http.MethodPut, "/admin/users/admin-user-001", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)
	assert.Contains(t, w.Body.String(), "Cannot suspend your own account")
}

func TestUpdateUserHandlerCannotDemoteSelf(t *testing.T) {
	router := setupTestServerHandlers(t)
	defer store.Close()

	router.PUT("/admin/users/:user_id", func(c *gin.Context) {
		c.Set("user_id", "admin-user-001")
		updateUserHandler(c)
	})

	body := map[string]interface{}{
		"role": "user",
	}
	bodyBytes, _ := json.Marshal(body)

	// Try to demote own account
	req := httptest.NewRequest(http.MethodPut, "/admin/users/admin-user-001", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)
	assert.Contains(t, w.Body.String(), "Cannot demote your own account")
}

func TestExportDatabaseHandler(t *testing.T) {
	router := setupTestServerHandlers(t)
	defer store.Close()

	router.GET("/admin/export", exportDatabaseHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/export", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	assert.Contains(t, w.Header().Get("Content-Type"), "application/json")
}

func TestForceCleanupHandler(t *testing.T) {
	router := setupTestServerHandlers(t)
	defer store.Close()

	router.POST("/admin/cleanup", forceCleanupHandler)

	req := httptest.NewRequest(http.MethodPost, "/admin/cleanup", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	assert.Contains(t, w.Body.String(), "Cleanup completed")
}

func TestLoginHandlerSuccess(t *testing.T) {
	router := setupTestServerHandlers(t)
	defer store.Close()

	// Hash password properly
	hashedPassword, _ := auth.HashPassword("testpassword123")
	testUser := &storage.User{
		ID:           "login-test-user",
		Email:        "login@test.com",
		PasswordHash: hashedPassword,
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
		UpdatedAt:    time.Now(),
	}
	store.CreateUser(testUser)

	router.POST("/auth/login", loginHandler)

	body := map[string]interface{}{
		"email":    "login@test.com",
		"password": "testpassword123",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/auth/login", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "token")
}

func TestLoginHandlerInvalidCredentials(t *testing.T) {
	router := setupTestServerHandlers(t)
	defer store.Close()

	router.POST("/auth/login", loginHandler)

	body := map[string]interface{}{
		"email":    "nonexistent@test.com",
		"password": "wrongpassword",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/auth/login", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func TestLoginHandlerSuspendedUser(t *testing.T) {
	router := setupTestServerHandlers(t)
	defer store.Close()

	hashedPassword, _ := auth.HashPassword("testpassword123")
	suspendedUser := &storage.User{
		ID:           "suspended-user",
		Email:        "suspended@test.com",
		PasswordHash: hashedPassword,
		Role:         "user",
		Status:       "suspended",
		CreatedAt:    time.Now(),
		UpdatedAt:    time.Now(),
	}
	store.CreateUser(suspendedUser)

	router.POST("/auth/login", loginHandler)

	body := map[string]interface{}{
		"email":    "suspended@test.com",
		"password": "testpassword123",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/auth/login", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)
	assert.Contains(t, w.Body.String(), "suspended")
}

func TestCreateDeviceHandler(t *testing.T) {
	router := setupTestServerHandlers(t)
	defer store.Close()

	router.POST("/devices", func(c *gin.Context) {
		c.Set("user_id", "regular-user-001")
		createDeviceHandler(c)
	})

	body := map[string]interface{}{
		"name": "Test Device",
		"type": "sensor",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/devices", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "device_id")
	assert.Contains(t, response, "api_key")
}

func TestListDevicesHandler(t *testing.T) {
	router := setupTestServerHandlers(t)
	defer store.Close()

	// Create a test device
	device := &storage.Device{
		ID:        "test-device-001",
		UserID:    "regular-user-001",
		Name:      "Test Device",
		Type:      "sensor",
		APIKey:    "sk_test_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.GET("/devices", func(c *gin.Context) {
		c.Set("user_id", "regular-user-001")
		listDevicesHandler(c)
	})

	req := httptest.NewRequest(http.MethodGet, "/devices", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "devices")
}

func TestUpdateDeviceHandler(t *testing.T) {
	router := setupTestServerHandlers(t)
	defer store.Close()

	// Create test device
	device := &storage.Device{
		ID:        "update-device-001",
		UserID:    "admin-user-001",
		Name:      "Old Name",
		Type:      "sensor",
		APIKey:    "sk_test_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.PUT("/admin/devices/:device_id", updateDeviceHandler)

	body := map[string]interface{}{
		"status": "suspended",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPut, "/admin/devices/update-device-001", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify update
	updatedDevice, _ := store.GetDevice("update-device-001")
	assert.Equal(t, "suspended", updatedDevice.Status)
}

func TestForceDeleteDeviceHandler(t *testing.T) {
	router := setupTestServerHandlers(t)
	defer store.Close()

	// Create test device
	device := &storage.Device{
		ID:        "delete-device-001",
		UserID:    "admin-user-001",
		Name:      "To Delete",
		Type:      "sensor",
		APIKey:    "sk_delete_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.DELETE("/admin/devices/:device_id/force", forceDeleteDeviceHandler)

	req := httptest.NewRequest(http.MethodDelete, "/admin/devices/delete-device-001/force", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify deletion
	_, err := store.GetDevice("delete-device-001")
	assert.Error(t, err)
}

func TestSecurityHeadersMiddlewareHandlers(t *testing.T) {
	gin.SetMode(gin.TestMode)
	router := gin.New()
	router.Use(securityHeadersMiddleware())
	router.GET("/test", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{"message": "ok"})
	})

	req := httptest.NewRequest(http.MethodGet, "/test", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, "DENY", w.Header().Get("X-Frame-Options"))
	assert.Equal(t, "nosniff", w.Header().Get("X-Content-Type-Options"))
	assert.Equal(t, "1; mode=block", w.Header().Get("X-XSS-Protection"))
	assert.Contains(t, w.Header().Get("Content-Security-Policy"), "default-src")
}

func TestMain(m *testing.M) {
	// Set JWT secret for tests
	os.Setenv("JWT_SECRET", "test-secret-key-for-testing-only")
	defer os.Unsetenv("JWT_SECRET")

	code := m.Run()
	os.Exit(code)
}
