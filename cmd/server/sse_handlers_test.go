package main

import (
	"datum-go/internal/storage"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func setupSSETestServer(t *testing.T) (*gin.Engine, func()) {
	gin.SetMode(gin.TestMode)

	tmpDir := t.TempDir()
	var err error
	store, err = storage.New(tmpDir+"/meta.db", tmpDir+"/tsdata", 7*24*time.Hour)
	require.NoError(t, err)

	store.InitializeSystem("Test Platform", true, 7)

	router := gin.New()
	cleanup := func() {
		store.Close()
	}

	return router, cleanup
}

func TestWebhookPollHandlerWithCommands(t *testing.T) {
	router, cleanup := setupSSETestServer(t)
	defer cleanup()

	device := &storage.Device{
		ID:        "webhook-device-001",
		UserID:    "user-001",
		Name:      "Webhook Device",
		Type:      "actuator",
		APIKey:    "sk_webhook_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	// Add a pending command
	command := &storage.Command{
		ID:        "cmd-001",
		DeviceID:  device.ID,
		Action:    "restart",
		Params:    map[string]interface{}{"delay": 5},
		Status:    "pending",
		CreatedAt: time.Now(),
	}
	store.CreateCommand(command)

	router.GET("/webhook/:device_id/poll", func(c *gin.Context) {
		c.Set("api_key", "sk_webhook_key")
		webhookPollHandler(c)
	})

	req := httptest.NewRequest(http.MethodGet, "/webhook/webhook-device-001/poll?wait=0", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	assert.Contains(t, w.Body.String(), "cmd-001")
	assert.Contains(t, w.Body.String(), "restart")
}

func TestWebhookPollHandlerNoCommands(t *testing.T) {
	router, cleanup := setupSSETestServer(t)
	defer cleanup()

	device := &storage.Device{
		ID:        "webhook-device-002",
		UserID:    "user-002",
		Name:      "Webhook Device 2",
		Type:      "sensor",
		APIKey:    "sk_webhook_key_2",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.GET("/webhook/:device_id/poll", func(c *gin.Context) {
		c.Set("api_key", "sk_webhook_key_2")
		webhookPollHandler(c)
	})

	req := httptest.NewRequest(http.MethodGet, "/webhook/webhook-device-002/poll?wait=0", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	assert.Contains(t, w.Body.String(), "commands")
}

func TestWebhookPollHandlerUnauthorized(t *testing.T) {
	router, cleanup := setupSSETestServer(t)
	defer cleanup()

	device := &storage.Device{
		ID:        "webhook-device-003",
		UserID:    "user-003",
		Name:      "Webhook Device 3",
		Type:      "sensor",
		APIKey:    "sk_valid_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.GET("/webhook/:device_id/poll", func(c *gin.Context) {
		c.Set("api_key", "sk_invalid_key")
		webhookPollHandler(c)
	})

	req := httptest.NewRequest(http.MethodGet, "/webhook/webhook-device-003/poll", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func TestFormatCommands(t *testing.T) {
	commands := []storage.Command{
		{
			ID:        "cmd-001",
			DeviceID:  "dev-001",
			Action:    "restart",
			Params:    map[string]interface{}{"delay": 10},
			Status:    "pending",
			CreatedAt: time.Now(),
		},
		{
			ID:        "cmd-002",
			DeviceID:  "dev-001",
			Action:    "update",
			Params:    map[string]interface{}{"version": "1.2.3"},
			Status:    "pending",
			CreatedAt: time.Now(),
		},
	}

	result := formatCommands(commands)

	// Type assertion to check the result
	resultSlice, ok := result.([]map[string]interface{})
	assert.True(t, ok)
	assert.Len(t, resultSlice, 2)
	assert.Equal(t, "cmd-001", resultSlice[0]["command_id"])
	assert.Equal(t, "restart", resultSlice[0]["action"])
	assert.Equal(t, "cmd-002", resultSlice[1]["command_id"])
	assert.Equal(t, "update", resultSlice[1]["action"])
}

// Test sseCommandsHandler unauthorized
func TestSSECommandsHandlerUnauthorized(t *testing.T) {
	router, cleanup := setupSSETestServer(t)
	defer cleanup()

	// Create test device
	device := &storage.Device{
		ID:     "sse-device-2",
		UserID: "user-2",
		APIKey: "sk_sse_test2",
		Status: "active",
	}
	store.CreateDevice(device)

	router.GET("/dev/:device_id/commands/sse", func(c *gin.Context) {
		c.Set("api_key", "sk_wrong_key")
		sseCommandsHandler(c)
	})

	req := httptest.NewRequest(http.MethodGet, "/dev/sse-device-2/commands/sse", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func TestSSECommandsHandler_Success(t *testing.T) {
	router, cleanup := setupSSETestServer(t)
	defer cleanup()

	// Create test device
	device := &storage.Device{
		ID:     "sse-device-success",
		UserID: "user-sse",
		APIKey: "sk_sse_success",
		Status: "active",
	}
	store.CreateDevice(device)

	// Register route
	router.GET("/dev/:device_id/commands/sse", func(c *gin.Context) {
		c.Set("api_key", "sk_sse_success")
		sseCommandsHandler(c)
	})

	// Pre-populate a command so it returns immediately (or quickly)
	command := &storage.Command{
		ID:       "cmd-immediate",
		DeviceID: device.ID,
		Action:   "ping",
		Status:   "pending",
	}
	store.CreateCommand(command)

	req := httptest.NewRequest(http.MethodGet, "/dev/sse-device-success/commands/sse?wait=1", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	assert.Contains(t, w.Header().Get("Content-Type"), "text/event-stream")
	assert.Contains(t, w.Body.String(), "event:keepalive")
	assert.Contains(t, w.Body.String(), "event:command")
	assert.Contains(t, w.Body.String(), "cmd-immediate")
}

func TestSSECommandsHandler_Timeout(t *testing.T) {
	router, cleanup := setupSSETestServer(t)
	defer cleanup()

	// Create test device
	device := &storage.Device{
		ID:     "sse-device-timeout",
		UserID: "user-sse",
		APIKey: "sk_sse_timeout",
		Status: "active",
	}
	store.CreateDevice(device)

	// Register route
	router.GET("/dev/:device_id/commands/sse", func(c *gin.Context) {
		c.Set("api_key", "sk_sse_timeout")
		sseCommandsHandler(c)
	})

	// No commands, wait=1 second
	req := httptest.NewRequest(http.MethodGet, "/dev/sse-device-timeout/commands/sse?wait=1", nil)
	w := httptest.NewRecorder()

	start := time.Now()
	router.ServeHTTP(w, req)
	duration := time.Since(start)

	assert.Equal(t, http.StatusOK, w.Code)
	// Should wait at least 1 second (approx)
	assert.GreaterOrEqual(t, duration.Milliseconds(), int64(900))

	assert.Contains(t, w.Body.String(), "event:keepalive")
	assert.Contains(t, w.Body.String(), "event:timeout")
	assert.Contains(t, w.Body.String(), "no commands")
}
