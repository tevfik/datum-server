package handlers

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"testing"
	"time"

	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// Helper to setup test environment
func setupTestEnv(t *testing.T) (*AdminHandler, func()) {
	gin.SetMode(gin.TestMode)

	tmpDir := t.TempDir()
	metaPath := filepath.Join(tmpDir, "test.db")
	dataPath := filepath.Join(tmpDir, "tsdata")

	store, err := storage.New(metaPath, dataPath, 24*time.Hour)
	require.NoError(t, err)

	handler := &AdminHandler{
		Store: store,
	}

	cleanup := func() {
		store.Close()
		os.RemoveAll(tmpDir)
	}

	return handler, cleanup
}

func setupRouter(handler *AdminHandler) *gin.Engine {
	r := gin.New()
	// Mock auth middleware or context setting
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "admin_user_id")
		c.Next()
	})
	return r
}

func TestProvisionDeviceHandler(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	r := setupRouter(handler)
	r.POST("/devices", handler.ProvisionDeviceHandler)

	t.Run("Valid Request", func(t *testing.T) {
		reqBody := map[string]string{
			"name": "Test Device",
			"type": "sensor",
		}
		body, _ := json.Marshal(reqBody)

		req, _ := http.NewRequest("POST", "/devices", bytes.NewBuffer(body))
		req.Header.Set("Content-Type", "application/json")
		w := httptest.NewRecorder()

		r.ServeHTTP(w, req)

		assert.Equal(t, http.StatusCreated, w.Code)

		var resp map[string]interface{}
		err := json.Unmarshal(w.Body.Bytes(), &resp)
		assert.NoError(t, err)
		assert.Equal(t, "Test Device", resp["name"])
		assert.NotEmpty(t, resp["device_id"])
		assert.NotEmpty(t, resp["api_key"])
	})

	t.Run("Missing Name", func(t *testing.T) {
		reqBody := map[string]string{
			"type": "sensor",
		}
		body, _ := json.Marshal(reqBody)

		req, _ := http.NewRequest("POST", "/devices", bytes.NewBuffer(body))
		w := httptest.NewRecorder()

		r.ServeHTTP(w, req)

		assert.Equal(t, http.StatusBadRequest, w.Code)
	})
}

func TestListAllDevicesHandler(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	// Create a device first
	device := &storage.Device{
		ID:        "dev_list_test",
		UserID:    "admin_user_id",
		Name:      "List Test Device",
		Type:      "sensor",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	require.NoError(t, handler.Store.CreateDevice(device))

	r := setupRouter(handler)
	r.GET("/devices", handler.ListAllDevicesHandler)

	req, _ := http.NewRequest("GET", "/devices", nil)
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &resp)
	assert.NoError(t, err)

	devices := resp["devices"].([]interface{})
	assert.Len(t, devices, 1)

	dev := devices[0].(map[string]interface{})
	assert.Equal(t, "List Test Device", dev["name"])
}

func TestGetDeviceAdminHandler(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	device := &storage.Device{
		ID:        "dev_get_test",
		UserID:    "admin_user_id",
		Name:      "Get Test Device",
		Type:      "sensor",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	require.NoError(t, handler.Store.CreateDevice(device))

	r := setupRouter(handler)
	r.GET("/devices/:device_id", handler.GetDeviceAdminHandler)

	req, _ := http.NewRequest("GET", "/devices/dev_get_test", nil)
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &resp)
	assert.NoError(t, err)
	assert.Equal(t, "Get Test Device", resp["name"])
}

func TestUpdateDeviceHandler(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	device := &storage.Device{
		ID:     "dev_update_test",
		UserID: "admin_user_id",
		Status: "active",
	}
	require.NoError(t, handler.Store.CreateDevice(device))

	r := setupRouter(handler)
	r.PUT("/devices/:device_id", handler.UpdateDeviceHandler)

	reqBody := map[string]string{
		"status": "suspended",
	}
	body, _ := json.Marshal(reqBody)

	req, _ := http.NewRequest("PUT", "/devices/dev_update_test", bytes.NewBuffer(body))
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	updated, _ := handler.Store.GetDevice("dev_update_test")
	assert.Equal(t, "suspended", updated.Status)
}

func TestRevokeDeviceKeyHandler(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	device := &storage.Device{
		ID:     "dev_revoke_test",
		UserID: "admin_user_id",
		APIKey: "valid_key",
	}
	require.NoError(t, handler.Store.CreateDevice(device))

	r := setupRouter(handler)
	r.POST("/devices/:device_id/revoke-key", handler.RevokeDeviceKeyHandler)

	req, _ := http.NewRequest("POST", "/devices/dev_revoke_test/revoke-key", nil)
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	updated, _ := handler.Store.GetDevice("dev_revoke_test")
	assert.Empty(t, updated.APIKey)
}

func TestRotateDeviceKeyHandler(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	device := &storage.Device{
		ID:     "dev_rotate_admin_test",
		UserID: "admin_user_id",
		APIKey: "old_key",
	}
	require.NoError(t, handler.Store.CreateDevice(device))

	r := setupRouter(handler)
	r.POST("/devices/:device_id/rotate-key", handler.RotateDeviceKeyHandler)

	req, _ := http.NewRequest("POST", "/devices/dev_rotate_admin_test/rotate-key", nil)
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &resp)
	assert.NoError(t, err)
	assert.NotEqual(t, "old_key", resp["api_key"])

	updated, _ := handler.Store.GetDevice("dev_rotate_admin_test")
	assert.Equal(t, resp["api_key"], updated.APIKey)
}
