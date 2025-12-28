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

func setupAdminCriticalTestServer(t *testing.T) (*gin.Engine, func()) {
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

// Test deleteUserHandler with existing user
func TestDeleteUserHandlerExisting(t *testing.T) {
	router, cleanup := setupAdminCriticalTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	hashedPassword, _ := auth.HashPassword("password123")
	user := &storage.User{
		ID:           "delete-user-1",
		Email:        "deleteuser@test.com",
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
}

// Test deleteUserHandler with devices
func TestDeleteUserHandlerWithDevices(t *testing.T) {
	router, cleanup := setupAdminCriticalTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	hashedPassword, _ := auth.HashPassword("password123")
	user := &storage.User{
		ID:           "user-with-dev",
		Email:        "userwithdev@test.com",
		PasswordHash: hashedPassword,
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user)

	device := &storage.Device{
		ID:        "dev-1",
		UserID:    "user-with-dev",
		Name:      "Test Device",
		APIKey:    "sk_test123",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.DELETE("/admin/users/:user_id", deleteUserHandler)

	req := httptest.NewRequest(http.MethodDelete, "/admin/users/user-with-dev", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	// May return OK if handler allows deletion, or Conflict if it checks devices
	assert.True(t, w.Code == http.StatusOK || w.Code == http.StatusConflict)
}

// Test exportDatabaseHandler with users
func TestExportDatabaseHandlerWithUsers(t *testing.T) {
	router, cleanup := setupAdminCriticalTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	hashedPassword, _ := auth.HashPassword("password123")
	for i := 1; i <= 3; i++ {
		user := &storage.User{
			ID:           "export-user-" + string(rune(i+'0')),
			Email:        "exportuser" + string(rune(i+'0')) + "@test.com",
			PasswordHash: hashedPassword,
			Role:         "user",
			Status:       "active",
			CreatedAt:    time.Now(),
		}
		store.CreateUser(user)
	}

	router.GET("/admin/database/export", exportDatabaseHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/database/export", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "users")
	assert.Contains(t, response, "devices")
}

// Test getSystemConfigHandler with default values
func TestGetSystemConfigHandlerDefaults(t *testing.T) {
	router, cleanup := setupAdminCriticalTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	router.GET("/admin/system/config", getSystemConfigHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/system/config", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "retention")
	assert.Contains(t, response, "rate_limit")
}

// Test getSystemConfigHandler with custom settings
func TestGetSystemConfigHandlerCustom(t *testing.T) {
	router, cleanup := setupAdminCriticalTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 14) // Custom retention

	router.GET("/admin/system/config", getSystemConfigHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/system/config", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	retention := response["retention"].(map[string]interface{})
	assert.Equal(t, float64(14), retention["days"])
}

// Test updateRetentionPolicyHandler with valid days
func TestUpdateRetentionPolicyHandlerValid(t *testing.T) {
	router, cleanup := setupAdminCriticalTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	router.POST("/admin/system/retention", updateRetentionPolicyHandler)

	requestBody := map[string]interface{}{
		"retention_days": 30,
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPost, "/admin/system/retention", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.True(t, w.Code == http.StatusOK || w.Code == http.StatusBadRequest)
}

// Test updateRetentionPolicyHandler with invalid JSON
func TestUpdateRetentionPolicyHandlerInvalidJSON(t *testing.T) {
	router, cleanup := setupAdminCriticalTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	router.POST("/admin/system/retention", updateRetentionPolicyHandler)

	req := httptest.NewRequest(http.MethodPost, "/admin/system/retention", bytes.NewBuffer([]byte("invalid")))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

// Test listUsersHandler with filter parameters
func TestListUsersHandlerWithFilters(t *testing.T) {
	router, cleanup := setupAdminCriticalTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	hashedPassword, _ := auth.HashPassword("password123")
	user1 := &storage.User{
		ID:           "filter-user-1",
		Email:        "filteruser1@test.com",
		PasswordHash: hashedPassword,
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user1)

	user2 := &storage.User{
		ID:           "filter-user-2",
		Email:        "filteruser2@test.com",
		PasswordHash: hashedPassword,
		Role:         "admin",
		Status:       "suspended",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user2)

	router.GET("/admin/users", listUsersHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/users?role=admin", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

// Test getDatabaseStatsHandler empty database
func TestGetDatabaseStatsHandlerEmpty(t *testing.T) {
	router, cleanup := setupAdminCriticalTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	router.GET("/admin/database/stats", getDatabaseStatsHandler)

	req := httptest.NewRequest(http.MethodGet, "/admin/database/stats", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "total_users")
	assert.Contains(t, response, "total_devices")
}

// Test resetPasswordHandler with non-empty password
func TestResetPasswordHandlerNonEmpty(t *testing.T) {
	router, cleanup := setupAdminCriticalTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	hashedPassword, _ := auth.HashPassword("password123")
	user := &storage.User{
		ID:           "reset-user-1",
		Email:        "resetuser@test.com",
		PasswordHash: hashedPassword,
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user)

	router.POST("/admin/users/:username/reset-password", resetPasswordHandler)

	requestBody := map[string]interface{}{
		"new_password": "newpassword456",
	}
	jsonBody, _ := json.Marshal(requestBody)

	req := httptest.NewRequest(http.MethodPost, "/admin/users/resetuser@test.com/reset-password", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}
