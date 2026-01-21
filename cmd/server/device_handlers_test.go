package main

import (
	"bytes"
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

// setupDeviceTestServer initializes the test server and creating a user
func setupDeviceTestServer(t *testing.T) (*gin.Engine, func(), *storage.User) {
	gin.SetMode(gin.TestMode)

	tmpDir := t.TempDir()
	var err error
	store, err = storage.New(tmpDir+"/meta.db", tmpDir+"/tsdata", 7*24*time.Hour)
	require.NoError(t, err)

	router := gin.New()

	cleanup := func() {
		store.Close()
	}

	// Create a test user
	user := &storage.User{
		ID:           "test-device-user",
		Email:        "device@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	require.NoError(t, store.CreateUser(user))

	return router, cleanup, user
}

func TestCreateDeviceHandler(t *testing.T) {
	router, cleanup, user := setupDeviceTestServer(t)
	defer cleanup()

	// Register route
	router.POST("/device", func(c *gin.Context) {
		c.Set("user_id", user.ID)
		createDeviceHandler(c)
	})

	t.Run("Create Device - Success", func(t *testing.T) {
		payload := map[string]string{
			"name": "Living Room Sensor",
			"type": "sensor",
		}
		body, _ := json.Marshal(payload)

		req := httptest.NewRequest(http.MethodPost, "/device", bytes.NewBuffer(body))
		req.Header.Set("Content-Type", "application/json")
		w := httptest.NewRecorder()
		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusCreated, w.Code)

		var resp map[string]interface{}
		json.Unmarshal(w.Body.Bytes(), &resp)
		assert.Contains(t, resp, "device_id")
		assert.Contains(t, resp, "api_key")

		// Verify in DB
		deviceID := resp["device_id"].(string)
		device, err := store.GetDevice(deviceID)
		assert.NoError(t, err)
		assert.Equal(t, "Living Room Sensor", device.Name)
		assert.Equal(t, "sensor", device.Type)
		assert.Equal(t, user.ID, device.UserID)
	})

	t.Run("Create Device - Invalid JSON", func(t *testing.T) {
		req := httptest.NewRequest(http.MethodPost, "/device", bytes.NewBuffer([]byte("invalid")))
		req.Header.Set("Content-Type", "application/json")
		w := httptest.NewRecorder()
		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusBadRequest, w.Code)
	})
}

func TestListDevicesHandler(t *testing.T) {
	router, cleanup, user := setupDeviceTestServer(t)
	defer cleanup()

	// Register route
	router.GET("/device", func(c *gin.Context) {
		c.Set("user_id", user.ID)
		c.Set("role", user.Role)
		listDevicesHandler(c)
	})

	// Add devices
	d1 := &storage.Device{ID: "dev_d1", UserID: user.ID, Name: "D1", LastSeen: time.Now()}
	d2 := &storage.Device{ID: "dev_d2", UserID: user.ID, Name: "D2", LastSeen: time.Now().Add(-10 * time.Minute)}
	require.NoError(t, store.CreateDevice(d1))
	require.NoError(t, store.CreateDevice(d2))

	// Add device for another user
	d3 := &storage.Device{ID: "dev_d3", UserID: "other", Name: "D3"}
	require.NoError(t, store.CreateDevice(d3))

	t.Run("List User Devices", func(t *testing.T) {
		req := httptest.NewRequest(http.MethodGet, "/device", nil)
		w := httptest.NewRecorder()
		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)

		var resp map[string][]DeviceResponse
		json.Unmarshal(w.Body.Bytes(), &resp)

		devices := resp["devices"]
		assert.Len(t, devices, 2)

		// Check statuses
		var d1Status, d2Status string
		for _, d := range devices {
			if d.ID == "dev_d1" {
				d1Status = d.Status
			}
			if d.ID == "dev_d2" {
				d2Status = d.Status
			}
		}
		assert.Equal(t, "online", d1Status)
		assert.Equal(t, "offline", d2Status)
	})

	t.Run("List All Devices as Admin", func(t *testing.T) {
		// Admin router setup
		adminRouter := gin.New()
		adminRouter.GET("/device", func(c *gin.Context) {
			c.Set("user_id", "admin")
			c.Set("role", "admin")
			listDevicesHandler(c)
		})

		req := httptest.NewRequest(http.MethodGet, "/device", nil)
		w := httptest.NewRecorder()
		adminRouter.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)

		var resp map[string][]DeviceResponse
		json.Unmarshal(w.Body.Bytes(), &resp)

		assert.Len(t, resp["devices"], 3) // Should see all 3 devices
	})
}

func TestUpdateDeviceConfigHandler(t *testing.T) {
router, cleanup, user := setupDeviceTestServer(t)
defer cleanup()

// Register route
router.PATCH("/device/:device_id/config", func(c *gin.Context) {
// Mock Auth Middleware behavior
if val := c.GetHeader("X-Role"); val != "" {
c.Set("role", val)
} else {
c.Set("role", user.Role)
}

if val := c.GetHeader("X-User-ID"); val != "" {
c.Set("user_id", val)
} else {
c.Set("user_id", user.ID)
}

updateDeviceConfigHandler(c)
})

// Create device owned by user
deviceID := "test-config-dev"
device := &storage.Device{ID: deviceID, UserID: user.ID, Name: "Config Dev"}
require.NoError(t, store.CreateDevice(device))

// Create device NOT owned by user
otherDeviceID := "other-dev"
otherDevice := &storage.Device{ID: otherDeviceID, UserID: "other-user", Name: "Other Dev"}
require.NoError(t, store.CreateDevice(otherDevice))

t.Run("Update Config - Own Device Success", func(t *testing.T) {
config := map[string]interface{}{
"resolution": "1080p",
"fps":        30,
}
body, _ := json.Marshal(config)
req := httptest.NewRequest(http.MethodPatch, "/device/"+deviceID+"/config", bytes.NewBuffer(body))
req.Header.Set("Content-Type", "application/json")
w := httptest.NewRecorder()
router.ServeHTTP(w, req)

assert.Equal(t, http.StatusOK, w.Code)

// Verify DB
updatedDev, err := store.GetDevice(deviceID)
assert.NoError(t, err)
assert.Equal(t, "1080p", updatedDev.DesiredState["resolution"])
// JSON numbers are float64
assert.Equal(t, float64(30), updatedDev.DesiredState["fps"])
})

t.Run("Update Config - Other Device Forbidden", func(t *testing.T) {
config := map[string]interface{}{"foo": "bar"}
body, _ := json.Marshal(config)
req := httptest.NewRequest(http.MethodPatch, "/device/"+otherDeviceID+"/config", bytes.NewBuffer(body))
req.Header.Set("Content-Type", "application/json")
w := httptest.NewRecorder()
router.ServeHTTP(w, req)

assert.Equal(t, http.StatusForbidden, w.Code)
})

t.Run("Update Config - Admin Success on Other Device", func(t *testing.T) {
config := map[string]interface{}{"admin_override": true}
body, _ := json.Marshal(config)
req := httptest.NewRequest(http.MethodPatch, "/device/"+otherDeviceID+"/config", bytes.NewBuffer(body))
req.Header.Set("Content-Type", "application/json")
req.Header.Set("X-Role", "admin") // Mock middleware picks this up
w := httptest.NewRecorder()
router.ServeHTTP(w, req)

assert.Equal(t, http.StatusOK, w.Code)

updatedDev, err := store.GetDevice(otherDeviceID)
assert.NoError(t, err)
assert.Equal(t, true, updatedDev.DesiredState["admin_override"])
})

t.Run("Update Config - Device Not Found", func(t *testing.T) {
req := httptest.NewRequest(http.MethodPatch, "/device/non-existent/config", bytes.NewBuffer([]byte("{}")))
req.Header.Set("Content-Type", "application/json")
w := httptest.NewRecorder()
router.ServeHTTP(w, req)

assert.Equal(t, http.StatusNotFound, w.Code)
})
}
