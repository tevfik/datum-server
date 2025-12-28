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

func setupCommandTestServer(t *testing.T) (*gin.Engine, func()) {
	gin.SetMode(gin.TestMode)

	tmpDir := t.TempDir()
	var err error
	store, err = storage.New(tmpDir+"/meta.db", tmpDir+"/tsdata", 7*24*time.Hour)
	require.NoError(t, err)

	// Create test user and device
	user := &storage.User{
		ID:           "cmd-user-001",
		Email:        "cmduser@test.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user)

	device := &storage.Device{
		ID:        "cmd-device-001",
		UserID:    "cmd-user-001",
		Name:      "Command Test Device",
		Type:      "sensor",
		APIKey:    "sk_cmd_test_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router := gin.New()
	cleanup := func() {
		store.Close()
	}

	return router, cleanup
}

func TestSendCommandHandler(t *testing.T) {
	router, cleanup := setupCommandTestServer(t)
	defer cleanup()

	router.POST("/devices/:device_id/commands", func(c *gin.Context) {
		c.Set("user_id", "cmd-user-001")
		sendCommandHandler(c)
	})

	body := map[string]interface{}{
		"action": "reboot",
		"params": map[string]interface{}{
			"delay": 10,
		},
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/devices/cmd-device-001/commands", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusAccepted, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "command_id")
	assert.Equal(t, "pending", response["status"])
}

func TestSendCommandHandlerDeviceNotFound(t *testing.T) {
	router, cleanup := setupCommandTestServer(t)
	defer cleanup()

	router.POST("/devices/:device_id/commands", func(c *gin.Context) {
		c.Set("user_id", "cmd-user-001")
		sendCommandHandler(c)
	})

	body := map[string]interface{}{
		"action": "reboot",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/devices/nonexistent/commands", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func TestSendCommandHandlerAccessDenied(t *testing.T) {
	router, cleanup := setupCommandTestServer(t)
	defer cleanup()

	router.POST("/devices/:device_id/commands", func(c *gin.Context) {
		c.Set("user_id", "different-user")
		sendCommandHandler(c)
	})

	body := map[string]interface{}{
		"action": "reboot",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/devices/cmd-device-001/commands", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)
}

func TestSendCommandHandlerInvalidJSON(t *testing.T) {
	router, cleanup := setupCommandTestServer(t)
	defer cleanup()

	router.POST("/devices/:device_id/commands", func(c *gin.Context) {
		c.Set("user_id", "cmd-user-001")
		sendCommandHandler(c)
	})

	req := httptest.NewRequest(http.MethodPost, "/devices/cmd-device-001/commands", bytes.NewReader([]byte("{invalid")))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestListCommandsHandler(t *testing.T) {
	router, cleanup := setupCommandTestServer(t)
	defer cleanup()

	// Create a command
	cmd := &storage.Command{
		ID:        "cmd_test_001",
		DeviceID:  "cmd-device-001",
		Action:    "reboot",
		Status:    "pending",
		CreatedAt: time.Now(),
	}
	store.CreateCommand(cmd)

	router.GET("/devices/:device_id/commands", func(c *gin.Context) {
		c.Set("user_id", "cmd-user-001")
		listCommandsHandler(c)
	})

	req := httptest.NewRequest(http.MethodGet, "/devices/cmd-device-001/commands", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "commands")
}

func TestListCommandsHandlerAccessDenied(t *testing.T) {
	router, cleanup := setupCommandTestServer(t)
	defer cleanup()

	router.GET("/devices/:device_id/commands", func(c *gin.Context) {
		c.Set("user_id", "different-user")
		listCommandsHandler(c)
	})

	req := httptest.NewRequest(http.MethodGet, "/devices/cmd-device-001/commands", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)
}

func TestPollCommandsHandler(t *testing.T) {
	router, cleanup := setupCommandTestServer(t)
	defer cleanup()

	// Create a command
	cmd := &storage.Command{
		ID:        "cmd_poll_001",
		DeviceID:  "cmd-device-001",
		Action:    "update",
		Params:    map[string]interface{}{"version": "1.0.0"},
		Status:    "pending",
		CreatedAt: time.Now(),
	}
	store.CreateCommand(cmd)

	router.GET("/devices/:device_id/commands/poll", func(c *gin.Context) {
		c.Set("api_key", "sk_cmd_test_key")
		pollCommandsHandler(c)
	})

	req := httptest.NewRequest(http.MethodGet, "/devices/cmd-device-001/commands/poll", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "commands")

	commands := response["commands"].([]interface{})
	assert.NotEmpty(t, commands)
}

func TestPollCommandsHandlerUnauthorized(t *testing.T) {
	router, cleanup := setupCommandTestServer(t)
	defer cleanup()

	router.GET("/devices/:device_id/commands/poll", func(c *gin.Context) {
		c.Set("api_key", "invalid_key")
		pollCommandsHandler(c)
	})

	req := httptest.NewRequest(http.MethodGet, "/devices/cmd-device-001/commands/poll", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func TestAckCommandHandler(t *testing.T) {
	router, cleanup := setupCommandTestServer(t)
	defer cleanup()

	// Create a command
	cmd := &storage.Command{
		ID:        "cmd_ack_001",
		DeviceID:  "cmd-device-001",
		Action:    "reboot",
		Status:    "pending",
		CreatedAt: time.Now(),
	}
	store.CreateCommand(cmd)

	router.POST("/devices/:device_id/commands/:command_id/ack", func(c *gin.Context) {
		c.Set("api_key", "sk_cmd_test_key")
		ackCommandHandler(c)
	})

	body := map[string]interface{}{
		"status": "completed",
		"result": map[string]interface{}{
			"success": true,
		},
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/devices/cmd-device-001/commands/cmd_ack_001/ack", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestAckCommandHandlerUnauthorized(t *testing.T) {
	router, cleanup := setupCommandTestServer(t)
	defer cleanup()

	router.POST("/devices/:device_id/commands/:command_id/ack", func(c *gin.Context) {
		c.Set("api_key", "invalid_key")
		ackCommandHandler(c)
	})

	body := map[string]interface{}{
		"status": "completed",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/devices/cmd-device-001/commands/cmd_001/ack", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func TestAckCommandHandlerInvalidJSON(t *testing.T) {
	router, cleanup := setupCommandTestServer(t)
	defer cleanup()

	router.POST("/devices/:device_id/commands/:command_id/ack", func(c *gin.Context) {
		c.Set("api_key", "sk_cmd_test_key")
		ackCommandHandler(c)
	})

	req := httptest.NewRequest(http.MethodPost, "/devices/cmd-device-001/commands/cmd_001/ack", bytes.NewReader([]byte("{invalid")))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestDeleteDeviceHandler(t *testing.T) {
	router, cleanup := setupCommandTestServer(t)
	defer cleanup()

	router.DELETE("/devices/:device_id", func(c *gin.Context) {
		c.Set("user_id", "cmd-user-001")
		deleteDeviceHandler(c)
	})

	req := httptest.NewRequest(http.MethodDelete, "/devices/cmd-device-001", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNoContent, w.Code)
}

func TestDeleteDeviceHandlerNotFound(t *testing.T) {
	router, cleanup := setupCommandTestServer(t)
	defer cleanup()

	router.DELETE("/devices/:device_id", func(c *gin.Context) {
		c.Set("user_id", "cmd-user-001")
		deleteDeviceHandler(c)
	})

	req := httptest.NewRequest(http.MethodDelete, "/devices/nonexistent", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func TestDeleteDeviceHandlerAccessDenied(t *testing.T) {
	router, cleanup := setupCommandTestServer(t)
	defer cleanup()

	router.DELETE("/devices/:device_id", func(c *gin.Context) {
		c.Set("user_id", "different-user")
		deleteDeviceHandler(c)
	})

	req := httptest.NewRequest(http.MethodDelete, "/devices/cmd-device-001", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)
}

func TestGenerateCommandID(t *testing.T) {
	id1 := generateCommandID()
	id2 := generateCommandID()

	assert.NotEqual(t, id1, id2)
	assert.Contains(t, id1, "cmd_")
	assert.Greater(t, len(id1), 10)
}
