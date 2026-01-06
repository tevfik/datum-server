package main

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"datum-go/internal/storage"

	"github.com/stretchr/testify/assert"
)

func TestRotateDeviceKeyHandler(t *testing.T) {
	// Setup with admin token
	r, testStore, token := setupTestEnvironment(t)
	defer testStore.Close()

	// Create a test device
	deviceID := "dev_key_rotate_test"
	initialKey := "initial-api-key-123"
	testStore.CreateDevice(&storage.Device{
		ID:        deviceID,
		APIKey:    initialKey,
		Status:    "active",
		CreatedAt: time.Now(),
		UserID:    "test-user-id",
		DeviceUID: "uid-rotate-test",
	})

	// Perform rotation
	req, _ := http.NewRequest("POST", "/admin/devices/"+deviceID+"/rotate-key", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	// Assertions
	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]string
	json.Unmarshal(w.Body.Bytes(), &response)
	newKey := response["api_key"]

	assert.NotEmpty(t, newKey)
	assert.NotEqual(t, initialKey, newKey)

	// Verify storage update
	device, _ := testStore.GetDevice(deviceID)
	assert.Equal(t, newKey, device.APIKey)
}

func TestRevokeDeviceKeyHandler(t *testing.T) {
	// Setup
	r, testStore, token := setupTestEnvironment(t)
	defer testStore.Close()

	// Create test device
	deviceID := "dev_key_revoke_test"
	testStore.CreateDevice(&storage.Device{
		ID:        deviceID,
		APIKey:    "key-to-revoke",
		Status:    "active",
		CreatedAt: time.Now(),
		UserID:    "test-user-id",
		DeviceUID: "uid-revoke-test",
	})

	// Perform revocation
	req, _ := http.NewRequest("POST", "/admin/devices/"+deviceID+"/revoke-key", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	// Assertions
	assert.Equal(t, http.StatusOK, w.Code)

	// Verify storage update
	device, _ := testStore.GetDevice(deviceID)
	assert.Empty(t, device.APIKey)
}
