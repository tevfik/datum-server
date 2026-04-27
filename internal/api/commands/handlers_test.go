package commands

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"testing"
	"time"

	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func setupTestEnv(t *testing.T) (*Handler, storage.Provider, func()) {
	gin.SetMode(gin.TestMode)
	tmpDir := t.TempDir()
	store, err := storage.New(
		filepath.Join(tmpDir, "meta.db"),
		filepath.Join(tmpDir, "tsdata"),
		24*time.Hour,
	)
	require.NoError(t, err)

	handler := NewHandler(store, nil)
	return handler, store, func() { store.Close() }
}

func createTestDevice(t *testing.T, store storage.Provider, userID, deviceID, apiKey string) {
	t.Helper()
	require.NoError(t, store.CreateDevice(&storage.Device{
		ID:        deviceID,
		UserID:    userID,
		Name:      "Test Device",
		Type:      "sensor",
		APIKey:    apiKey,
		CreatedAt: time.Now(),
	}))
}

// ---------- SendCommand ----------

func TestSendCommand_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestDevice(t, store, "user1", "dev1", "sk_key1")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "user1")
		c.Set("role", "user")
		c.Next()
	})
	r.POST("/dev/:device_id/cmd", handler.SendCommand)

	body, _ := json.Marshal(SendCommandRequest{
		Action: "reboot",
		Params: map[string]interface{}{"delay": 5},
	})
	req, _ := http.NewRequest("POST", "/dev/dev1/cmd", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusAccepted, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	assert.NotEmpty(t, resp["command_id"])
	assert.Equal(t, "pending", resp["status"])
}

func TestSendCommand_DeviceNotFound(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "user1")
		c.Set("role", "user")
		c.Next()
	})
	r.POST("/dev/:device_id/cmd", handler.SendCommand)

	body, _ := json.Marshal(SendCommandRequest{Action: "reboot"})
	req, _ := http.NewRequest("POST", "/dev/nonexistent/cmd", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func TestSendCommand_Forbidden(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestDevice(t, store, "user1", "dev1", "sk_key1")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "user2") // different user
		c.Set("role", "user")
		c.Next()
	})
	r.POST("/dev/:device_id/cmd", handler.SendCommand)

	body, _ := json.Marshal(SendCommandRequest{Action: "reboot"})
	req, _ := http.NewRequest("POST", "/dev/dev1/cmd", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)
}

func TestSendCommand_InvalidBody(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestDevice(t, store, "user1", "dev1", "sk_key1")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "user1")
		c.Set("role", "user")
		c.Next()
	})
	r.POST("/dev/:device_id/cmd", handler.SendCommand)

	req, _ := http.NewRequest("POST", "/dev/dev1/cmd", bytes.NewBufferString(`{}`))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

// ---------- ListCommands ----------

func TestListCommands_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestDevice(t, store, "user1", "dev1", "sk_key1")

	// Create a pending command
	require.NoError(t, store.CreateCommand(&storage.Command{
		ID:        "cmd_1",
		DeviceID:  "dev1",
		Action:    "reboot",
		Status:    "pending",
		CreatedAt: time.Now(),
		ExpiresAt: time.Now().Add(time.Hour),
	}))

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "user1")
		c.Set("role", "user")
		c.Next()
	})
	r.GET("/dev/:device_id/cmd", handler.ListCommands)

	req, _ := http.NewRequest("GET", "/dev/dev1/cmd", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	assert.NotNil(t, resp["commands"])
}

func TestListCommands_Forbidden(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestDevice(t, store, "user1", "dev1", "sk_key1")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "user2")
		c.Set("role", "user")
		c.Next()
	})
	r.GET("/dev/:device_id/cmd", handler.ListCommands)

	req, _ := http.NewRequest("GET", "/dev/dev1/cmd", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)
}

// ---------- PollCommands ----------

func TestPollCommands_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestDevice(t, store, "user1", "dev1", "sk_key1")

	require.NoError(t, store.CreateCommand(&storage.Command{
		ID:        "cmd_2",
		DeviceID:  "dev1",
		Action:    "update",
		Status:    "pending",
		CreatedAt: time.Now(),
		ExpiresAt: time.Now().Add(time.Hour),
	}))

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("api_key", "sk_key1")
		c.Next()
	})
	r.GET("/dev/:device_id/cmd/pending", handler.PollCommands)

	req, _ := http.NewRequest("GET", "/dev/dev1/cmd/pending", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	cmds := resp["commands"].([]interface{})
	assert.GreaterOrEqual(t, len(cmds), 1)
}

func TestPollCommands_InvalidAPIKey(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestDevice(t, store, "user1", "dev1", "sk_key1")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("api_key", "sk_wrong")
		c.Next()
	})
	r.GET("/dev/:device_id/cmd/pending", handler.PollCommands)

	req, _ := http.NewRequest("GET", "/dev/dev1/cmd/pending", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

// ---------- AckCommand ----------

func TestAckCommand_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestDevice(t, store, "user1", "dev1", "sk_key1")

	require.NoError(t, store.CreateCommand(&storage.Command{
		ID:        "cmd_ack",
		DeviceID:  "dev1",
		Action:    "reboot",
		Status:    "pending",
		CreatedAt: time.Now(),
		ExpiresAt: time.Now().Add(time.Hour),
	}))

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("api_key", "sk_key1")
		c.Next()
	})
	r.POST("/dev/:device_id/cmd/:command_id/ack", handler.AckCommand)

	body, _ := json.Marshal(AckCommandRequest{Status: "completed"})
	req, _ := http.NewRequest("POST", "/dev/dev1/cmd/cmd_ack/ack", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestAckCommand_InvalidDevice(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestDevice(t, store, "user1", "dev1", "sk_key1")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("api_key", "sk_wrong")
		c.Next()
	})
	r.POST("/dev/:device_id/cmd/:command_id/ack", handler.AckCommand)

	body, _ := json.Marshal(AckCommandRequest{Status: "completed"})
	req, _ := http.NewRequest("POST", "/dev/dev1/cmd/cmd_ack/ack", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}
