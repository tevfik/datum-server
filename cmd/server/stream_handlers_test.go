package main

import (
	"bytes"
	"encoding/base64"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"testing"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
)

func TestUploadFrameHandler(t *testing.T) {
	gin.SetMode(gin.TestMode)
	router, testStore := setupTestRouter()
	store = testStore // Set global store

	// Create test user and device
	testUser := &storage.User{
		ID:           "test-user-stream",
		Email:        "stream@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
		UpdatedAt:    time.Now(),
	}
	store.CreateUser(testUser)

	testDevice := &storage.Device{
		ID:     "test-device-stream",
		UserID: testUser.ID,
		Name:   "Stream Camera",
		APIKey: "test-stream-api-key",
	}
	store.CreateDevice(testDevice)

	// Register route
	router.POST("/dev/:device_id/stream/frame", func(c *gin.Context) {
		// Simulate API key auth
		apiKey := c.GetHeader("X-API-Key")
		if apiKey != "" {
			c.Set("api_key", apiKey)
		}
		uploadFrameHandler(c)
	})

	// Create a valid JPEG header (minimal JPEG)
	validJPEG := []byte{0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F', 0xFF, 0xD9}

	t.Run("upload frame with JSON base64 encoded image", func(t *testing.T) {
		base64Image := base64.StdEncoding.EncodeToString(validJPEG)
		payload := map[string]string{
			"image":  base64Image,
			"format": "jpeg",
		}
		body, _ := json.Marshal(payload)

		req := httptest.NewRequest("POST", "/dev/test-device-stream/stream/frame", bytes.NewBuffer(body))
		req.Header.Set("Content-Type", "application/json")
		req.Header.Set("X-API-Key", testDevice.APIKey)
		w := httptest.NewRecorder()

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)
		var response map[string]interface{}
		json.Unmarshal(w.Body.Bytes(), &response)
		assert.Equal(t, "frame_received", response["status"])
		assert.Equal(t, float64(len(validJPEG)), response["size_bytes"])
	})

	t.Run("upload frame with raw binary JPEG", func(t *testing.T) {
		req := httptest.NewRequest("POST", "/dev/test-device-stream/stream/frame", bytes.NewBuffer(validJPEG))
		req.Header.Set("Content-Type", "image/jpeg")
		req.Header.Set("X-API-Key", testDevice.APIKey)
		w := httptest.NewRecorder()

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)
		var response map[string]interface{}
		json.Unmarshal(w.Body.Bytes(), &response)
		assert.Equal(t, "frame_received", response["status"])
	})

	t.Run("unauthorized - no API key", func(t *testing.T) {
		req := httptest.NewRequest("POST", "/dev/test-device-stream/stream/frame", bytes.NewBuffer(validJPEG))
		w := httptest.NewRecorder()

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusUnauthorized, w.Code)
	})

	t.Run("forbidden - wrong device ID", func(t *testing.T) {
		req := httptest.NewRequest("POST", "/dev/wrong-device-id/stream/frame", bytes.NewBuffer(validJPEG))
		req.Header.Set("X-API-Key", testDevice.APIKey)
		w := httptest.NewRecorder()

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusForbidden, w.Code)
	})

	t.Run("invalid JSON payload", func(t *testing.T) {
		req := httptest.NewRequest("POST", "/dev/test-device-stream/stream/frame", bytes.NewBuffer([]byte("invalid-json")))
		req.Header.Set("Content-Type", "application/json")
		req.Header.Set("X-API-Key", testDevice.APIKey)
		w := httptest.NewRecorder()

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusBadRequest, w.Code)
	})

	t.Run("invalid base64 in JSON", func(t *testing.T) {
		payload := map[string]string{
			"image":  "not-valid-base64!!!",
			"format": "jpeg",
		}
		body, _ := json.Marshal(payload)

		req := httptest.NewRequest("POST", "/dev/test-device-stream/stream/frame", bytes.NewBuffer(body))
		req.Header.Set("Content-Type", "application/json")
		req.Header.Set("X-API-Key", testDevice.APIKey)
		w := httptest.NewRecorder()

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusBadRequest, w.Code)
	})

	t.Run("invalid JPEG format", func(t *testing.T) {
		invalidJPEG := []byte{0x00, 0x00, 0x00, 0x00} // Not a JPEG

		req := httptest.NewRequest("POST", "/dev/test-device-stream/stream/frame", bytes.NewBuffer(invalidJPEG))
		req.Header.Set("Content-Type", "image/jpeg")
		req.Header.Set("X-API-Key", testDevice.APIKey)
		w := httptest.NewRecorder()

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusBadRequest, w.Code)
	})
}

func TestStreamInfoHandler(t *testing.T) {
	gin.SetMode(gin.TestMode)
	router, testStore := setupTestRouter()
	store = testStore

	// Create test user and device
	testUser := &storage.User{
		ID:           "test-user-streaminfo",
		Email:        "streaminfo@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
		UpdatedAt:    time.Now(),
	}
	store.CreateUser(testUser)

	testDevice := &storage.Device{
		ID:     "test-device-streaminfo",
		UserID: testUser.ID,
		Name:   "Info Camera",
		APIKey: "test-streaminfo-api-key",
	}
	store.CreateDevice(testDevice)

	// Generate admin token
	adminToken, _ := auth.GenerateToken(testUser.ID, testUser.Email, "user")

	// Register route
	router.GET("/dev/:device_id/stream/info", func(c *gin.Context) {
		// Simulate auth middleware
		token := c.GetHeader("Authorization")
		if len(token) > 7 && token[:7] == "Bearer " {
			c.Set("user_id", testUser.ID)
		}
		streamInfoHandler(c)
	})

	t.Run("get stream info - no active stream", func(t *testing.T) {
		req := httptest.NewRequest("GET", "/dev/test-device-streaminfo/stream/info", nil)
		req.Header.Set("Authorization", "Bearer "+adminToken)
		w := httptest.NewRecorder()

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)
		var response map[string]interface{}
		json.Unmarshal(w.Body.Bytes(), &response)
		assert.Equal(t, "test-device-streaminfo", response["device_id"])
		assert.Equal(t, false, response["active"])
		assert.Equal(t, float64(0), response["clients"])
	})

	t.Run("get stream info - active stream", func(t *testing.T) {
		// Simulate an active stream
		validJPEG := []byte{0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F', 0xFF, 0xD9}
		streamManager.BroadcastFrame("test-device-streaminfo", validJPEG)

		req := httptest.NewRequest("GET", "/dev/test-device-streaminfo/stream/info", nil)
		req.Header.Set("Authorization", "Bearer "+adminToken)
		w := httptest.NewRecorder()

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)
		var response map[string]interface{}
		json.Unmarshal(w.Body.Bytes(), &response)
		assert.Equal(t, "test-device-streaminfo", response["device_id"])
		assert.Equal(t, true, response["active"])
		assert.Equal(t, true, response["has_frame"])
		assert.Equal(t, float64(len(validJPEG)), response["frame_size"])
	})

	t.Run("device not found", func(t *testing.T) {
		req := httptest.NewRequest("GET", "/dev/nonexistent-device/stream/info", nil)
		req.Header.Set("Authorization", "Bearer "+adminToken)
		w := httptest.NewRecorder()

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusNotFound, w.Code)
	})

	t.Run("access denied - different user", func(t *testing.T) {
		// Create another user
		otherUser := &storage.User{
			ID:           "other-streaminfo",
			Email:        "other@streaminfo.com",
			PasswordHash: "hashed",
			Role:         "user",
			Status:       "active",
			CreatedAt:    time.Now(),
			UpdatedAt:    time.Now(),
		}
		store.CreateUser(otherUser)
		otherToken, _ := auth.GenerateToken(otherUser.ID, otherUser.Email, "user")

		// Create a separate router for this test that sets the other user's ID
		otherRouter, _ := setupTestRouter()
		otherRouter.GET("/dev/:device_id/stream/info", func(c *gin.Context) {
			c.Set("user_id", otherUser.ID) // Set different user ID
			streamInfoHandler(c)
		})

		req := httptest.NewRequest("GET", "/dev/test-device-streaminfo/stream/info", nil)
		req.Header.Set("Authorization", "Bearer "+otherToken)
		w := httptest.NewRecorder()

		otherRouter.ServeHTTP(w, req)

		assert.Equal(t, http.StatusForbidden, w.Code)
	})
}

func TestStreamManagerBroadcastFrame(t *testing.T) {
	sm := &StreamManager{
		streams: make(map[string]*DeviceStream),
	}

	deviceID := "test-device-broadcast"
	frame := []byte("test-frame-data")

	t.Run("broadcast frame creates new stream", func(t *testing.T) {
		sm.BroadcastFrame(deviceID, frame)

		stream := sm.GetStream(deviceID)
		assert.NotNil(t, stream)
		assert.Equal(t, deviceID, stream.DeviceID)
		assert.Equal(t, frame, stream.LastFrame)
	})

	t.Run("broadcast frame to existing stream", func(t *testing.T) {
		newFrame := []byte("new-frame-data")
		sm.BroadcastFrame(deviceID, newFrame)

		stream := sm.GetStream(deviceID)
		assert.Equal(t, newFrame, stream.LastFrame)
	})

	t.Run("broadcast frame to clients", func(t *testing.T) {
		// Register a client
		clientChan := sm.RegisterClient(deviceID)
		defer sm.UnregisterClient(deviceID, clientChan)

		frameData := []byte("broadcast-to-client")
		sm.BroadcastFrame(deviceID, frameData)

		// Client should receive the frame
		select {
		case receivedFrame := <-clientChan:
			assert.Equal(t, frameData, receivedFrame)
		case <-time.After(1 * time.Second):
			t.Fatal("Client did not receive frame")
		}
	})
}

func TestStreamManagerRegisterUnregisterClient(t *testing.T) {
	sm := &StreamManager{
		streams: make(map[string]*DeviceStream),
	}

	deviceID := "test-device-client"

	t.Run("register client creates channel", func(t *testing.T) {
		clientChan := sm.RegisterClient(deviceID)
		assert.NotNil(t, clientChan)

		count := sm.GetClientCount(deviceID)
		assert.Equal(t, 1, count)

		sm.UnregisterClient(deviceID, clientChan)
	})

	t.Run("register multiple clients", func(t *testing.T) {
		client1 := sm.RegisterClient(deviceID)
		client2 := sm.RegisterClient(deviceID)
		client3 := sm.RegisterClient(deviceID)

		count := sm.GetClientCount(deviceID)
		assert.Equal(t, 3, count)

		sm.UnregisterClient(deviceID, client1)
		sm.UnregisterClient(deviceID, client2)
		sm.UnregisterClient(deviceID, client3)
	})

	t.Run("unregister client removes from stream", func(t *testing.T) {
		clientChan := sm.RegisterClient(deviceID)
		assert.Equal(t, 1, sm.GetClientCount(deviceID))

		sm.UnregisterClient(deviceID, clientChan)
		assert.Equal(t, 0, sm.GetClientCount(deviceID))
	})

	t.Run("unregister nonexistent client", func(t *testing.T) {
		fakeChan := make(chan []byte)
		// Should not panic
		sm.UnregisterClient("nonexistent-device", fakeChan)
	})
}

func TestStreamManagerGetters(t *testing.T) {
	sm := &StreamManager{
		streams: make(map[string]*DeviceStream),
	}

	deviceID := "test-device-getters"
	frame := []byte("test-frame")

	t.Run("GetLastFrame - no stream", func(t *testing.T) {
		result := sm.GetLastFrame("nonexistent")
		assert.Nil(t, result)
	})

	t.Run("GetLastFrame - with stream", func(t *testing.T) {
		sm.BroadcastFrame(deviceID, frame)
		result := sm.GetLastFrame(deviceID)
		assert.Equal(t, frame, result)
	})

	t.Run("GetClientCount - no stream", func(t *testing.T) {
		count := sm.GetClientCount("nonexistent")
		assert.Equal(t, 0, count)
	})

	t.Run("GetClientCount - with clients", func(t *testing.T) {
		client1 := sm.RegisterClient(deviceID)
		client2 := sm.RegisterClient(deviceID)

		count := sm.GetClientCount(deviceID)
		assert.Equal(t, 2, count)

		sm.UnregisterClient(deviceID, client1)
		sm.UnregisterClient(deviceID, client2)
	})

	t.Run("GetStream - existing stream", func(t *testing.T) {
		stream := sm.GetStream(deviceID)
		assert.NotNil(t, stream)
		assert.Equal(t, deviceID, stream.DeviceID)
	})

	t.Run("GetStream - nonexistent stream", func(t *testing.T) {
		stream := sm.GetStream("nonexistent")
		assert.Nil(t, stream)
	})
}

func TestMJPEGStreamHandler(t *testing.T) {
	gin.SetMode(gin.TestMode)
	router, testStore := setupTestRouter()
	store = testStore

	// Create test user and device
	testUser := &storage.User{
		ID:           "test-user-mjpeg",
		Email:        "mjpeg@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
		UpdatedAt:    time.Now(),
	}
	store.CreateUser(testUser)

	testDevice := &storage.Device{
		ID:     "test-device-mjpeg",
		UserID: testUser.ID,
		Name:   "MJPEG Camera",
		APIKey: "test-mjpeg-api-key",
	}
	store.CreateDevice(testDevice)

	adminToken, _ := auth.GenerateToken(testUser.ID, testUser.Email, "user")

	// Register route
	router.GET("/dev/:device_id/stream/mjpeg", func(c *gin.Context) {
		token := c.GetHeader("Authorization")
		if len(token) > 7 && token[:7] == "Bearer " {
			c.Set("user_id", testUser.ID)
		}
		mjpegStreamHandler(c)
	})

	t.Run("device not found", func(t *testing.T) {
		req := httptest.NewRequest("GET", "/dev/nonexistent/stream/mjpeg", nil)
		req.Header.Set("Authorization", "Bearer "+adminToken)
		w := httptest.NewRecorder()

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusNotFound, w.Code)
	})

	t.Run("access denied - different user", func(t *testing.T) {
		otherUser := &storage.User{
			ID:           "other-mjpeg",
			Email:        "other@mjpeg.com",
			PasswordHash: "hashed",
			Role:         "user",
			Status:       "active",
			CreatedAt:    time.Now(),
			UpdatedAt:    time.Now(),
		}
		store.CreateUser(otherUser)

		// Create device for other user
		otherDevice := &storage.Device{
			ID:     "other-device-mjpeg",
			UserID: otherUser.ID,
			Name:   "Other Camera",
			APIKey: "other-api-key",
		}
		store.CreateDevice(otherDevice)

		otherToken, _ := auth.GenerateToken(otherUser.ID, otherUser.Email, "user")

		// Create router that sets otherUser's ID
		otherRouter, _ := setupTestRouter()
		otherRouter.GET("/dev/:device_id/stream/mjpeg", func(c *gin.Context) {
			c.Set("user_id", otherUser.ID)
			mjpegStreamHandler(c)
		})

		// Try to access the first user's device
		req := httptest.NewRequest("GET", "/dev/test-device-mjpeg/stream/mjpeg", nil)
		req.Header.Set("Authorization", "Bearer "+otherToken)
		w := httptest.NewRecorder()

		otherRouter.ServeHTTP(w, req)

		assert.Equal(t, http.StatusForbidden, w.Code)
	})

	// Note: Full MJPEG streaming test would require a more complex setup
	// with concurrent goroutines and actual frame broadcasting
}

func TestWebSocketStreamHandler(t *testing.T) {
	gin.SetMode(gin.TestMode)
	router, testStore := setupTestRouter()
	store = testStore

	// Create test user and device
	testUser := &storage.User{
		ID:           "test-user-ws",
		Email:        "ws@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
		UpdatedAt:    time.Now(),
	}
	store.CreateUser(testUser)

	testDevice := &storage.Device{
		ID:     "test-device-ws",
		UserID: testUser.ID,
		Name:   "WS Camera",
		APIKey: "test-ws-api-key",
	}
	store.CreateDevice(testDevice)

	adminToken, _ := auth.GenerateToken(testUser.ID, testUser.Email, "user")

	// Register route
	router.GET("/dev/:device_id/stream/ws", func(c *gin.Context) {
		token := c.GetHeader("Authorization")
		if len(token) > 7 && token[:7] == "Bearer " {
			c.Set("user_id", testUser.ID)
		}
		websocketStreamHandler(c)
	})

	t.Run("device not found for websocket", func(t *testing.T) {
		req := httptest.NewRequest("GET", "/dev/nonexistent/stream/ws", nil)
		req.Header.Set("Authorization", "Bearer "+adminToken)
		w := httptest.NewRecorder()

		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusNotFound, w.Code)
	})

	t.Run("access denied for websocket", func(t *testing.T) {
		otherUser := &storage.User{
			ID:           "other-ws",
			Email:        "other@ws.com",
			PasswordHash: "hashed",
			Role:         "user",
			Status:       "active",
			CreatedAt:    time.Now(),
			UpdatedAt:    time.Now(),
		}
		store.CreateUser(otherUser)
		otherToken, _ := auth.GenerateToken(otherUser.ID, otherUser.Email, "user")

		// Create router that sets otherUser's ID
		otherRouter, _ := setupTestRouter()
		otherRouter.GET("/dev/:device_id/stream/ws", func(c *gin.Context) {
			c.Set("user_id", otherUser.ID)
			websocketStreamHandler(c)
		})

		req := httptest.NewRequest("GET", "/dev/test-device-ws/stream/ws", nil)
		req.Header.Set("Authorization", "Bearer "+otherToken)
		w := httptest.NewRecorder()

		otherRouter.ServeHTTP(w, req)
		assert.Equal(t, http.StatusForbidden, w.Code)
	})

	// Note: Full WebSocket test would require starting an actual server
	// and using websocket.Dial to connect. This is complex for unit tests.
}

func TestWebSocketUpgraderConfig(t *testing.T) {
	// Save original env var and restore after test
	originalCors := os.Getenv("CORS_ALLOWED_ORIGINS")
	t.Cleanup(func() {
		os.Setenv("CORS_ALLOWED_ORIGINS", originalCors)
	})

	t.Run("wsUpgrader has correct buffer sizes", func(t *testing.T) {
		assert.Equal(t, 1024, wsUpgrader.ReadBufferSize)
		assert.Equal(t, 1024*1024, wsUpgrader.WriteBufferSize)
	})

	t.Run("Default allows all (empty env)", func(t *testing.T) {
		os.Setenv("CORS_ALLOWED_ORIGINS", "")
		req := httptest.NewRequest("GET", "/test", nil)
		req.Header.Set("Origin", "http://example.com")
		allowed := wsUpgrader.CheckOrigin(req)
		assert.True(t, allowed)
	})

	t.Run("Explicit wildcard allows all", func(t *testing.T) {
		os.Setenv("CORS_ALLOWED_ORIGINS", "*")
		req := httptest.NewRequest("GET", "/test", nil)
		req.Header.Set("Origin", "http://malicious.com")
		allowed := wsUpgrader.CheckOrigin(req)
		assert.True(t, allowed)
	})

	t.Run("Specific origin allowed", func(t *testing.T) {
		os.Setenv("CORS_ALLOWED_ORIGINS", "https://app.example.com,https://admin.example.com")
		req := httptest.NewRequest("GET", "/test", nil)
		req.Header.Set("Origin", "https://app.example.com")
		allowed := wsUpgrader.CheckOrigin(req)
		assert.True(t, allowed)
	})

	t.Run("Disallowed origin rejected", func(t *testing.T) {
		os.Setenv("CORS_ALLOWED_ORIGINS", "https://app.example.com")
		req := httptest.NewRequest("GET", "/test", nil)
		req.Header.Set("Origin", "http://evil.com")
		allowed := wsUpgrader.CheckOrigin(req)
		assert.False(t, allowed)
	})
}

func TestStreamConcurrentClients(t *testing.T) {
	sm := &StreamManager{
		streams: make(map[string]*DeviceStream),
	}

	deviceID := "test-concurrent"
	numClients := 10

	t.Run("concurrent client registration", func(t *testing.T) {
		clients := make([]chan []byte, numClients)

		// Register multiple clients concurrently
		for i := 0; i < numClients; i++ {
			clients[i] = sm.RegisterClient(deviceID)
		}

		assert.Equal(t, numClients, sm.GetClientCount(deviceID))

		// Broadcast frame to all
		frame := []byte("concurrent-frame")
		sm.BroadcastFrame(deviceID, frame)

		// All clients should receive
		for i := 0; i < numClients; i++ {
			select {
			case receivedFrame := <-clients[i]:
				assert.Equal(t, frame, receivedFrame)
			case <-time.After(1 * time.Second):
				t.Fatalf("Client %d did not receive frame", i)
			}
		}

		// Cleanup
		for i := 0; i < numClients; i++ {
			sm.UnregisterClient(deviceID, clients[i])
		}

		assert.Equal(t, 0, sm.GetClientCount(deviceID))
	})
}

func TestStreamSnapshotHandler(t *testing.T) {
	gin.SetMode(gin.TestMode)
	router, testStore := setupTestRouter()
	store = testStore

	// Create test user and device
	testUser := &storage.User{
		ID:           "test-user-snapshot",
		Email:        "snapshot@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(testUser)

	testDevice := &storage.Device{
		ID:     "test-device-snapshot",
		UserID: testUser.ID,
		Name:   "Snapshot Camera",
		APIKey: "test-snapshot-api-key",
	}
	store.CreateDevice(testDevice)

	adminToken, _ := auth.GenerateToken(testUser.ID, testUser.Email, "user")

	router.GET("/dev/:device_id/stream/snapshot", func(c *gin.Context) {
		token := c.GetHeader("Authorization")
		if len(token) > 7 && token[:7] == "Bearer " {
			c.Set("user_id", testUser.ID)
		}
		streamSnapshotHandler(c)
	})

	t.Run("snapshot - no stream active", func(t *testing.T) {
		req := httptest.NewRequest("GET", "/dev/test-device-snapshot/stream/snapshot", nil)
		req.Header.Set("Authorization", "Bearer "+adminToken)
		w := httptest.NewRecorder()

		router.ServeHTTP(w, req)
		assert.Equal(t, http.StatusNotFound, w.Code)
	})

	t.Run("snapshot - success", func(t *testing.T) {
		// Broadcast a frame
		validJPEG := []byte{0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F', 0xFF, 0xD9}
		streamManager.BroadcastFrame("test-device-snapshot", validJPEG)

		req := httptest.NewRequest("GET", "/dev/test-device-snapshot/stream/snapshot", nil)
		req.Header.Set("Authorization", "Bearer "+adminToken)
		w := httptest.NewRecorder()

		router.ServeHTTP(w, req)
		assert.Equal(t, http.StatusOK, w.Code)
		assert.Equal(t, "image/jpeg", w.Header().Get("Content-Type"))
		assert.Equal(t, validJPEG, w.Body.Bytes())
	})

	t.Run("snapshot - access denied", func(t *testing.T) {
		otherUser := &storage.User{ID: "other", Email: "other@x.com", Role: "user"}
		store.CreateUser(otherUser)
		otherToken, _ := auth.GenerateToken(otherUser.ID, otherUser.Email, "user")

		otherRouter, _ := setupTestRouter()
		otherRouter.GET("/dev/:device_id/stream/snapshot", func(c *gin.Context) {
			c.Set("user_id", otherUser.ID)
			streamSnapshotHandler(c)
		})

		req := httptest.NewRequest("GET", "/dev/test-device-snapshot/stream/snapshot", nil)
		req.Header.Set("Authorization", "Bearer "+otherToken)
		w := httptest.NewRecorder()

		otherRouter.ServeHTTP(w, req)
		assert.Equal(t, http.StatusForbidden, w.Code)
	})
}
