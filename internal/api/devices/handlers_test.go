package devices

import (
	"bytes"
	"encoding/json"
	"fmt"
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
func setupTestEnv(t *testing.T) (*Handler, func()) {
	gin.SetMode(gin.TestMode)

	tmpDir := t.TempDir()
	metaPath := filepath.Join(tmpDir, "test.db")
	dataPath := filepath.Join(tmpDir, "tsdata")

	store, err := storage.New(metaPath, dataPath, 24*time.Hour)
	require.NoError(t, err)

	handler := NewHandler(store, nil) // Null MQTT broker

	cleanup := func() {
		store.Close()
		os.RemoveAll(tmpDir)
	}

	return handler, cleanup
}

func setupRouter(handler *Handler) *gin.Engine {
	r := gin.New()
	// Mock auth context
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "test_user_id")
		c.Next()
	})
	return r
}

func TestRotateKey(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	// 1. Create Device
	device := &storage.Device{
		ID:        "dev_rotate_test",
		UserID:    "test_user_id",
		Name:      "Test Device",
		Type:      "sensor",
		APIKey:    "old_key",
		CreatedAt: time.Now(),
	}
	require.NoError(t, handler.Store.CreateDevice(device))

	r := setupRouter(handler)
	r.POST("/:device_id/rotate-key", handler.RotateKey)

	// 2. Rotate Key
	req, _ := http.NewRequest("POST", "/dev_rotate_test/rotate-key", nil)
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	// 3. Verify
	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &resp)
	assert.NoError(t, err)
	assert.NotEqual(t, "old_key", resp["api_key"])

	updated, _ := handler.Store.GetDevice("dev_rotate_test")
	assert.Equal(t, resp["api_key"], updated.APIKey)
}

func TestRotateKeyUnauthorized(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	device := &storage.Device{
		ID:        "dev_other_user",
		UserID:    "other_user",
		Name:      "Other Device",
		Type:      "sensor",
		APIKey:    "key",
		CreatedAt: time.Now(),
	}
	require.NoError(t, handler.Store.CreateDevice(device))

	r := setupRouter(handler)
	r.POST("/:device_id/rotate-key", handler.RotateKey)

	req, _ := http.NewRequest("POST", "/dev_other_user/rotate-key", nil)
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)
}

func TestRotateKeyNotFound(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	r := setupRouter(handler)
	r.POST("/:device_id/rotate-key", handler.RotateKey)

	req, _ := http.NewRequest("POST", "/non_existent_device/rotate-key", nil)
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

// ---------- CreateDevice ----------

func TestCreateDevice_Success(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	r := setupRouter(handler)
	r.POST("/", handler.CreateDevice)

	body := bytes.NewBufferString(`{"name":"Sensor A","type":"temperature"}`)
	req, _ := http.NewRequest("POST", "/", body)
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	assert.NotEmpty(t, resp["device_id"])
	assert.NotEmpty(t, resp["api_key"])
}

// ---------- ListDevices ----------

func TestListDevices_AsOwner(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	// Create devices
	for i := 0; i < 3; i++ {
		require.NoError(t, handler.Store.CreateDevice(&storage.Device{
			ID:        fmt.Sprintf("dev_%d", i),
			UserID:    "test_user_id",
			Name:      fmt.Sprintf("Device %d", i),
			Type:      "sensor",
			APIKey:    fmt.Sprintf("sk_key_%d", i),
			CreatedAt: time.Now(),
		}))
	}

	r := setupRouter(handler)
	r.GET("/", handler.ListDevices)

	req, _ := http.NewRequest("GET", "/", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	devices := resp["devices"].([]interface{})
	assert.Len(t, devices, 3)
}

func TestListDevices_AdminSeesAll(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	require.NoError(t, handler.Store.CreateDevice(&storage.Device{
		ID: "dev_other", UserID: "other_user", Name: "Other", Type: "sensor",
		APIKey: "sk_other", CreatedAt: time.Now(),
	}))

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "admin_id")
		c.Set("role", "admin")
		c.Next()
	})
	r.GET("/", handler.ListDevices)

	req, _ := http.NewRequest("GET", "/", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

// ---------- GetDevice ----------

func TestGetDevice_AsOwner(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	require.NoError(t, handler.Store.CreateDevice(&storage.Device{
		ID: "dev_get", UserID: "test_user_id", Name: "My Device", Type: "sensor",
		APIKey: "sk_get", CreatedAt: time.Now(),
	}))

	r := setupRouter(handler)
	r.GET("/:device_id", handler.GetDevice)

	req, _ := http.NewRequest("GET", "/dev_get", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	assert.Equal(t, "dev_get", resp["id"])
}

func TestGetDevice_NotFound(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	r := setupRouter(handler)
	r.GET("/:device_id", handler.GetDevice)

	req, _ := http.NewRequest("GET", "/nonexistent", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

// ---------- DeleteDevice ----------

func TestDeleteDevice_AsOwner(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	require.NoError(t, handler.Store.CreateDevice(&storage.Device{
		ID: "dev_del", UserID: "test_user_id", Name: "Delete Me", Type: "sensor",
		APIKey: "sk_del", CreatedAt: time.Now(),
	}))

	r := setupRouter(handler)
	r.DELETE("/:device_id", handler.DeleteDevice)

	req, _ := http.NewRequest("DELETE", "/dev_del", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify gone
	_, err := handler.Store.GetDevice("dev_del")
	assert.Error(t, err)
}

func TestDeleteDevice_Forbidden(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	require.NoError(t, handler.Store.CreateDevice(&storage.Device{
		ID: "dev_other_del", UserID: "other_user", Name: "Other", Type: "sensor",
		APIKey: "sk_other_del", CreatedAt: time.Now(),
	}))

	r := setupRouter(handler) // sets user_id = test_user_id (not owner)
	r.DELETE("/:device_id", handler.DeleteDevice)

	req, _ := http.NewRequest("DELETE", "/dev_other_del", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)
}
