package main

import (
	"bytes"
	"datum-go/internal/auth"
	"datum-go/internal/handlers"
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

func setupComprehensiveTestServer(t *testing.T) (*gin.Engine, func()) {
	gin.SetMode(gin.TestMode)

	tmpDir := t.TempDir()
	var err error
	store, err = storage.New(tmpDir+"/meta.db", tmpDir+"/tsdata", 7*24*time.Hour)
	require.NoError(t, err)

	router := gin.New()
	cleanup := func() {
		store.Close()
	}

	return router, cleanup
}

// Test createUserHandler with missing fields
func TestCreateUserHandlerMissingEmail(t *testing.T) {
	router, cleanup := setupComprehensiveTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	h := handlers.NewAdminHandler(store, nil)
	router.POST("/admin/users", h.CreateUserHandler)

	requestBody := map[string]interface{}{
		"password": "password123",
		"role":     "user",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPost, "/admin/users", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

// Test createUserHandler with short password
func TestCreateUserHandlerShortPassword(t *testing.T) {
	router, cleanup := setupComprehensiveTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	h := handlers.NewAdminHandler(store, nil)
	router.POST("/admin/users", h.CreateUserHandler)

	requestBody := map[string]interface{}{
		"email":    "test@test.com",
		"password": "short",
		"role":     "user",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPost, "/admin/users", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

// Test deleteUserHandler with non-existent user
func TestDeleteUserHandlerNonExistent(t *testing.T) {
	router, cleanup := setupComprehensiveTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	h := handlers.NewAdminHandler(store, nil)
	router.DELETE("/admin/users/:user_id", h.DeleteUserHandler)

	req := httptest.NewRequest(http.MethodDelete, "/admin/users/does-not-exist", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

// Test resetDatabaseHandler with confirmation
func TestResetDatabaseHandlerConfirm(t *testing.T) {
	router, cleanup := setupComprehensiveTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	h := handlers.NewAdminHandler(store, nil)
	router.POST("/admin/database/reset", h.ResetDatabaseHandler)

	requestBody := map[string]interface{}{
		"confirm": "RESET",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPost, "/admin/database/reset", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

// Test resetDatabaseHandler without confirmation
func TestResetDatabaseHandlerNoConfirm(t *testing.T) {
	router, cleanup := setupComprehensiveTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	h := handlers.NewAdminHandler(store, nil)
	router.POST("/admin/database/reset", h.ResetDatabaseHandler)

	requestBody := map[string]interface{}{
		"confirm": "wrong",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPost, "/admin/database/reset", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

// Test resetDatabaseHandler invalid JSON
func TestResetDatabaseHandlerInvalidJSON(t *testing.T) {
	router, cleanup := setupComprehensiveTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	h := handlers.NewAdminHandler(store, nil)
	router.POST("/admin/database/reset", h.ResetDatabaseHandler)

	req := httptest.NewRequest(http.MethodPost, "/admin/database/reset", bytes.NewBuffer([]byte("invalid json")))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

// Test setupSystemHandler with missing platform name
func TestSetupSystemHandlerMissingPlatformName(t *testing.T) {
	router, cleanup := setupComprehensiveTestServer(t)
	defer cleanup()

	h := handlers.NewAdminHandler(store, nil)
	router.POST("/system/setup", h.SetupSystemHandler)

	requestBody := map[string]interface{}{
		"admin_email":    "admin@test.com",
		"admin_password": "password123",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPost, "/system/setup", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

// Test setupSystemHandler with invalid JSON
func TestSetupSystemHandlerInvalidJSON(t *testing.T) {
	router, cleanup := setupComprehensiveTestServer(t)
	defer cleanup()

	h := handlers.NewAdminHandler(store, nil)
	router.POST("/system/setup", h.SetupSystemHandler)

	req := httptest.NewRequest(http.MethodPost, "/system/setup", bytes.NewBuffer([]byte("invalid")))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

// Test listUsersHandler with pagination
func TestListUsersHandlerPagination(t *testing.T) {
	router, cleanup := setupComprehensiveTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	// Create multiple users
	for i := 1; i <= 5; i++ {
		hashedPassword, _ := auth.HashPassword("password123")
		user := &storage.User{
			ID:           "page-user-" + string(rune(i+'0')),
			Email:        "pageuser" + string(rune(i+'0')) + "@test.com",
			PasswordHash: hashedPassword,
			Role:         "user",
			Status:       "active",
			CreatedAt:    time.Now(),
		}
		store.CreateUser(user)
	}

	h := handlers.NewAdminHandler(store, nil)
	router.GET("/admin/users", h.ListUsersHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/users?limit=3", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

// Test getDatabaseStatsHandler with multiple entities
func TestGetDatabaseStatsHandlerWithData(t *testing.T) {
	router, cleanup := setupComprehensiveTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	// Create users
	for i := 1; i <= 3; i++ {
		hashedPassword, _ := auth.HashPassword("password123")
		user := &storage.User{
			ID:           "stats-user-" + string(rune(i+'0')),
			Email:        "statsuser" + string(rune(i+'0')) + "@test.com",
			PasswordHash: hashedPassword,
			Role:         "user",
			Status:       "active",
			CreatedAt:    time.Now(),
		}
		store.CreateUser(user)
	}

	// Create devices
	for i := 1; i <= 2; i++ {
		device := &storage.Device{
			ID:        "stats-dev-" + string(rune(i+'0')),
			UserID:    "stats-user-1",
			Name:      "Stats Device " + string(rune(i+'0')),
			APIKey:    "sk_stats" + string(rune(i+'0')),
			Status:    "active",
			CreatedAt: time.Now(),
		}
		store.CreateDevice(device)
	}

	h := handlers.NewAdminHandler(store, nil)
	router.GET("/admin/database/stats", h.GetDatabaseStatsHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/database/stats", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "total_users")
	assert.Contains(t, response, "total_devices")
}

// Test updateUserHandler with only status change
func TestUpdateUserHandlerOnlyStatus(t *testing.T) {
	router, cleanup := setupComprehensiveTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	hashedPassword, _ := auth.HashPassword("password123")
	user := &storage.User{
		ID:           "status-user-1",
		Email:        "statususer@test.com",
		PasswordHash: hashedPassword,
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user)

	h := handlers.NewAdminHandler(store, nil)
	router.PUT("/admin/users/:user_id", h.UpdateUserHandler)

	requestBody := map[string]interface{}{
		"status": "suspended",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPut, "/admin/users/status-user-1", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

// Test updateUserHandler with only role change
func TestUpdateUserHandlerOnlyRole(t *testing.T) {
	router, cleanup := setupComprehensiveTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	hashedPassword, _ := auth.HashPassword("password123")
	user := &storage.User{
		ID:           "role-user-1",
		Email:        "roleuser@test.com",
		PasswordHash: hashedPassword,
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user)

	h := handlers.NewAdminHandler(store, nil)
	router.PUT("/admin/users/:user_id", h.UpdateUserHandler)

	requestBody := map[string]interface{}{
		"role": "admin",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPut, "/admin/users/role-user-1", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

// Test updateUserHandler invalid JSON
func TestUpdateUserHandlerInvalidJSON(t *testing.T) {
	router, cleanup := setupComprehensiveTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	h := handlers.NewAdminHandler(store, nil)
	router.PUT("/admin/users/:user_id", h.UpdateUserHandler)

	req := httptest.NewRequest(http.MethodPut, "/admin/users/some-user", bytes.NewBuffer([]byte("invalid")))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

// Test loginHandler with wrong password
func TestLoginHandlerWrongPassword(t *testing.T) {
	router, cleanup := setupComprehensiveTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	hashedPassword, _ := auth.HashPassword("correctpassword")
	user := &storage.User{
		ID:           "login-user-1",
		Email:        "loginuser@test.com",
		PasswordHash: hashedPassword,
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user)

	router.POST("/auth/login", loginHandler)

	requestBody := map[string]interface{}{
		"email":    "loginuser@test.com",
		"password": "wrongpassword",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPost, "/auth/login", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

// Test exportDatabaseHandler empty database
func TestExportDatabaseHandlerEmpty(t *testing.T) {
	router, cleanup := setupComprehensiveTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	h := handlers.NewAdminHandler(store, nil)
	router.GET("/admin/database/export", h.ExportDatabaseHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/database/export", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

// Test forceDeleteDeviceHandler with non-existent device
func TestForceDeleteDeviceHandlerNotFound(t *testing.T) {
	router, cleanup := setupComprehensiveTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	h := handlers.NewAdminHandler(store, nil)
	router.DELETE("/admin/devices/:device_id/force", h.ForceDeleteDeviceHandler)

	req := httptest.NewRequest(http.MethodDelete, "/admin/devices/nonexistent/force", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}
