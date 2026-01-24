package handlers

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"testing"
	"time"

	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// setupTestEnv creates a temporary environment for testing
func setupTestEnv(t *testing.T) (storage.Provider, string) {
	// Create temp directory for tstorage
	tempDir, err := os.MkdirTemp("", "datum-test-*")
	require.NoError(t, err)

	// Initialize storage with in-memory BuntDB and temp dir for tstorage
	store, err := storage.New(":memory:", tempDir, 24*time.Hour)
	require.NoError(t, err)

	return store, tempDir
}

func cleanupTestEnv(t *testing.T, store storage.Provider, tempDir string) {
	store.Close()
	os.RemoveAll(tempDir)
}

func TestProvisionDeviceHandler(t *testing.T) {
	store, tempDir := setupTestEnv(t)
	defer cleanupTestEnv(t, store, tempDir)

	h := &AdminHandler{Store: store}

	// Setup Gin
	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "admin_user")
		c.Next()
	})
	r.POST("/devices", h.ProvisionDeviceHandler)

	// Test Case: Success
	reqBody := ProvisionDeviceRequest{
		Name: "Test Device",
		Type: "sensor",
	}
	body, _ := json.Marshal(reqBody)
	req, _ := http.NewRequest("POST", "/devices", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var resp map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &resp)
	require.NoError(t, err)
	assert.Equal(t, "Test Device", resp["name"])
	assert.NotEmpty(t, resp["device_id"])
	assert.NotEmpty(t, resp["api_key"])
}

func TestListAllDevicesHandler(t *testing.T) {
	store, tempDir := setupTestEnv(t)
	defer cleanupTestEnv(t, store, tempDir)

	// Pre-create a device
	device := &storage.Device{
		ID:        "dev_123",
		UserID:    "user_1",
		Name:      "Existing Device",
		Type:      "camera",
		Status:    "active",
		CreatedAt: time.Now(),
		LastSeen:  time.Now(),
	}
	err := store.CreateDevice(device)
	require.NoError(t, err)

	h := &AdminHandler{Store: store}

	// Setup Gin
	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.GET("/devices", h.ListAllDevicesHandler)

	// Test Case: List Devices
	req, _ := http.NewRequest("GET", "/devices", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp gin.H
	err = json.Unmarshal(w.Body.Bytes(), &resp)
	require.NoError(t, err)

	devicesList, ok := resp["devices"].([]interface{})
	assert.True(t, ok)
	assert.Len(t, devicesList, 1)

	dev := devicesList[0].(map[string]interface{})
	assert.Equal(t, "dev_123", dev["id"])
	assert.Equal(t, "Existing Device", dev["name"])
}

func TestUpdateDeviceHandler(t *testing.T) {
	store, tempDir := setupTestEnv(t)
	defer cleanupTestEnv(t, store, tempDir)

	// Pre-create a device
	device := &storage.Device{
		ID:        "dev_to_update",
		UserID:    "user_1",
		Name:      "To Update",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	err := store.CreateDevice(device)
	require.NoError(t, err)

	h := &AdminHandler{Store: store}

	r := gin.New()
	r.PATCH("/devices/:device_id", h.UpdateDeviceHandler)

	// Test Case: Update Status
	updateReq := UpdateDeviceRequest{Status: "suspended"}
	body, _ := json.Marshal(updateReq)
	req, _ := http.NewRequest("PATCH", "/devices/dev_to_update", bytes.NewBuffer(body))
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify update in store
	updatedDev, err := store.GetDevice("dev_to_update")
	require.NoError(t, err)
	assert.Equal(t, "suspended", updatedDev.Status)
}

func TestForceDeleteDeviceHandler(t *testing.T) {
	store, tempDir := setupTestEnv(t)
	defer cleanupTestEnv(t, store, tempDir)

	device := &storage.Device{
		ID:     "dev_delete",
		UserID: "user_1",
		Status: "active",
	}
	err := store.CreateDevice(device)
	require.NoError(t, err)

	h := &AdminHandler{Store: store}
	r := gin.New()
	r.DELETE("/devices/:device_id", h.ForceDeleteDeviceHandler)

	req, _ := http.NewRequest("DELETE", "/devices/dev_delete", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	_, err = store.GetDevice("dev_delete")
	assert.Error(t, err) // Should not exist
}
