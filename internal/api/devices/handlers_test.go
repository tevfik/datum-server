package devices

import (
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
