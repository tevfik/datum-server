package main

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func setupHybridTestServer(t *testing.T) *gin.Engine {
	gin.SetMode(gin.TestMode)

	// Create temporary storage
	tmpDir := t.TempDir()
	var err error
	store, err = storage.New(tmpDir+"/meta.db", tmpDir+"/tsdata", 7*24*time.Hour)
	require.NoError(t, err)

	r := gin.New()
	return r
}

// TestHybridAuthThingDescriptionUpload verifies that both Users and Devices can upload TD
func TestHybridAuthThingDescriptionUpload(t *testing.T) {
	r := setupHybridTestServer(t)
	defer store.Close()

	// Register the Hybrid route
	r.PUT("/dev/:device_id/thing-description", auth.HybridAuthMiddleware(store), updateDeviceThingDescriptionHandler)

	// Create Data
	userID := "user_123"
	deviceID := "dev_123"
	deviceKey := "sk_test_key_123"

	// Create User
	user := &storage.User{
		ID:     userID,
		Email:  "test@example.com",
		Role:   "user",
		Status: "active",
	}
	err := store.CreateUser(user)
	require.NoError(t, err)

	// Create Device
	device := &storage.Device{
		ID:     deviceID,
		UserID: userID,
		APIKey: deviceKey,
		Name:   "Test Device",
		Status: "active",
	}
	err = store.CreateDevice(device)
	require.NoError(t, err)

	deviceID2 := "dev_logic_test_2"
	device2 := &storage.Device{
		ID:     deviceID2,
		UserID: "owner_user",
		APIKey: "sk_logic_test_2",
		Name:   "Logic Test Device 2",
	}
	err = store.CreateDevice(device2)
	require.NoError(t, err)

	// ==========================================
	// Scenario 1: User Authentication (JWT)
	// ==========================================
	t.Run("User Auth Success", func(t *testing.T) {
		w := httptest.NewRecorder()
		data := map[string]interface{}{"title": "My Device (User Update)"}
		body, _ := json.Marshal(data)
		req, _ := http.NewRequest("PUT", "/dev/"+deviceID+"/thing-description", bytes.NewBuffer(body))

		// Generate valid User Token
		token, _ := auth.GenerateToken(userID, "test@example.com", "user")
		req.Header.Set("Authorization", "Bearer "+token)

		r.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)

		// Verify DB
		d, err := store.GetDevice(deviceID)
		require.NoError(t, err)
		assert.Equal(t, "My Device (User Update)", d.ThingDescription["title"])
	})
}

func TestUpdateDeviceThingDescriptionHandler_Logic(t *testing.T) {
	// Unit test just the Handler logic (bypassing middleware) to ensure it respects context
	r := setupHybridTestServer(t)
	defer store.Close()

	// Register Handler directly without middleware
	// We'll use a temporary route with a custom handler to set context for testing

	deviceID := "dev_logic_test"
	deviceKey := "sk_logic_test"

	device := &storage.Device{
		ID:     deviceID,
		UserID: "owner_user",
		APIKey: deviceKey,
		Name:   "Logic Test Device",
	}
	err := store.CreateDevice(device)
	require.NoError(t, err)

	deviceID2 := "dev_logic_test_2"
	device2 := &storage.Device{
		ID:     deviceID2,
		UserID: "owner_user",
		APIKey: "sk_logic_test_2",
		Name:   "Logic Test Device 2",
	}
	err = store.CreateDevice(device2)
	require.NoError(t, err)

	// 1. Device Auth Context Success
	t.Run("Device Auth Context Success", func(t *testing.T) {
		w := httptest.NewRecorder()
		data := map[string]interface{}{"title": "Device Self Update"}
		body, _ := json.Marshal(data)
		reqManual, _ := http.NewRequest("PUT", "/manual/dev/"+deviceID+"/thing-description", bytes.NewBuffer(body))

		// Manually set context as if Middleware ran
		// The actual Gin context is created by r.ServeHTTP, so we need to ensure the handler
		// on the temporary route sets the context correctly.
		// For this test, the `updateDeviceThingDescriptionHandler` will read `c.Param("device_id")`
		// and `c.Get("device_id")`. We need to ensure both are consistent.
		// The temporary route handler above will call `updateDeviceThingDescriptionHandler(c)`
		// and `c.Param("device_id")` will be `deviceID`.
		// We need to ensure `c.Get("device_id")` is also `deviceID`.
		// Let's modify the temporary route handler to set `c.Set("device_id", c.Param("device_id"))`
		// to properly simulate the middleware.

		// Re-register the route with the correct context setting logic for this test
		r.PUT("/manual/dev/:device_id/thing-description", func(c *gin.Context) {
			c.Set("device_id", c.Param("device_id")) // Simulate middleware setting context
			updateDeviceThingDescriptionHandler(c)
		})

		r.ServeHTTP(w, reqManual)

		assert.Equal(t, http.StatusOK, w.Code)

		// Verify DB
		d, err := store.GetDevice(deviceID)
		require.NoError(t, err)
		assert.Equal(t, "Device Self Update", d.ThingDescription["title"])
	})

	// 2. Device Auth Context Mismatch (Hack attempt)
	t.Run("Device Auth Context Mismatch", func(t *testing.T) {
		w := httptest.NewRecorder()
		data := map[string]interface{}{"title": "Hacked Update"}
		body, _ := json.Marshal(data)

		// Re-register the route with the correct context setting logic for this test
		r.PUT("/manual_hacker/dev/:device_id/thing-description", func(c *gin.Context) {
			c.Set("device_id", "dev_other_hacker") // Authenticated as DIFFERENT device
			updateDeviceThingDescriptionHandler(c)
		})

		reqManual, _ := http.NewRequest("PUT", "/manual_hacker/dev/"+deviceID2+"/thing-description", bytes.NewBuffer(body))

		r.ServeHTTP(w, reqManual)

		assert.Equal(t, http.StatusForbidden, w.Code)

		// Verify DB was NOT updated
		d, err := store.GetDevice(deviceID2)
		require.NoError(t, err)
		assert.Nil(t, d.ThingDescription) // Should still be nil or original if it had one
	})
}
