package main

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"net/http"
	"sync"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

// StreamManager manages active video streams from devices
type StreamManager struct {
	mu      sync.RWMutex
	streams map[string]*DeviceStream
}

type DeviceStream struct {
	DeviceID    string
	Clients     map[string]*StreamClient
	LastFrame   []byte
	LastUpdated time.Time
	mu          sync.RWMutex
}

type StreamClient struct {
	ID   string
	Chan chan []byte
}

var streamManager = &StreamManager{
	streams: make(map[string]*DeviceStream),
}

var wsUpgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool {
		return true // TODO: Add proper CORS validation
	},
	ReadBufferSize:  1024,
	WriteBufferSize: 1024 * 1024, // 1MB for video frames
}

// Device uploads video frames (MJPEG or individual JPEGs)
// POST /devices/:device_id/stream/frame
func uploadFrameHandler(c *gin.Context) {
	deviceID := c.Param("device_id")

	// Verify device auth
	apiKey, exists := c.Get("api_key")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

	var device *storage.Device
	var err error

	isToken, _ := c.Get("is_token")
	if isToken == true {
		// Token-based auth (Device uses DK token)
		var isGracePeriod bool
		device, isGracePeriod, err = store.GetDeviceByToken(apiKey.(string))
		if err == nil && isGracePeriod {
			// Optional: Log warning if using grace period token for high-bandwidth stream
		}
	} else {
		// API Key-based auth (Legacy)
		device, err = store.GetDeviceByAPIKey(apiKey.(string))
	}

	if err != nil || device.ID != deviceID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Device not found or unauthorized"})
		return
	}

	// Read frame data
	var frameData []byte

	// Check content type
	contentType := c.GetHeader("Content-Type")

	if contentType == "application/json" {
		// JSON with base64 encoded image
		var req struct {
			Image  string `json:"image"`
			Format string `json:"format"`
		}
		if err := c.ShouldBindJSON(&req); err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid JSON"})
			return
		}

		frameData, err = base64.StdEncoding.DecodeString(req.Image)
		if err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid base64 image"})
			return
		}
	} else {
		// Raw binary JPEG
		frameData, err = c.GetRawData()
		if err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": "Failed to read frame"})
			return
		}
	}

	// Validate JPEG header
	if len(frameData) < 2 || frameData[0] != 0xFF || frameData[1] != 0xD8 {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid JPEG format"})
		return
	}

	// Broadcast frame to all connected clients
	streamManager.BroadcastFrame(deviceID, frameData)

	// Update device last seen
	go store.UpdateDeviceLastSeen(deviceID)

	c.JSON(http.StatusOK, gin.H{
		"status":     "frame_received",
		"size_bytes": len(frameData),
		"clients":    streamManager.GetClientCount(deviceID),
	})
}

// MJPEG stream endpoint for browsers (HTTP)
// GET /devices/:device_id/stream/mjpeg
func mjpegStreamHandler(c *gin.Context) {
	deviceID := c.Param("device_id")
	userID, _ := auth.GetUserID(c)

	// Verify device ownership
	device, err := store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}
	if device.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Access denied"})
		return
	}

	// Set MJPEG headers
	c.Header("Content-Type", "multipart/x-mixed-replace; boundary=frame")
	c.Header("Cache-Control", "no-cache, no-store, must-revalidate")
	c.Header("Pragma", "no-cache")
	c.Header("Expires", "0")
	c.Header("Connection", "keep-alive")

	// Register client
	clientChan := streamManager.RegisterClient(deviceID)
	defer streamManager.UnregisterClient(deviceID, clientChan)

	// Send initial frame if available
	if frame := streamManager.GetLastFrame(deviceID); frame != nil {
		c.Writer.Write([]byte("--frame\r\n"))
		c.Writer.Write([]byte("Content-Type: image/jpeg\r\n"))
		c.Writer.Write([]byte(fmt.Sprintf("Content-Length: %d\r\n\r\n", len(frame))))
		c.Writer.Write(frame)
		c.Writer.Write([]byte("\r\n"))
		c.Writer.Flush()
	}

	// Stream frames
	ticker := time.NewTicker(30 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case frame := <-clientChan:
			c.Writer.Write([]byte("--frame\r\n"))
			c.Writer.Write([]byte("Content-Type: image/jpeg\r\n"))
			c.Writer.Write([]byte(fmt.Sprintf("Content-Length: %d\r\n\r\n", len(frame))))
			c.Writer.Write(frame)
			c.Writer.Write([]byte("\r\n"))
			c.Writer.Flush()

		case <-ticker.C:
			// Keepalive
			c.Writer.Write([]byte("\r\n"))
			c.Writer.Flush()

		case <-c.Request.Context().Done():
			return
		}
	}
}

// WebSocket stream endpoint (binary, low latency)
// GET /devices/:device_id/stream/ws
func websocketStreamHandler(c *gin.Context) {
	deviceID := c.Param("device_id")
	userID, _ := auth.GetUserID(c)

	// Verify device ownership
	device, err := store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}
	if device.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Access denied"})
		return
	}

	// Upgrade to WebSocket
	conn, err := wsUpgrader.Upgrade(c.Writer, c.Request, nil)
	if err != nil {
		return
	}
	defer conn.Close()

	// Register client
	clientChan := streamManager.RegisterClient(deviceID)
	defer streamManager.UnregisterClient(deviceID, clientChan)

	// Send initial frame if available
	if frame := streamManager.GetLastFrame(deviceID); frame != nil {
		conn.WriteMessage(websocket.BinaryMessage, frame)
	}

	// Send metadata
	metadata := map[string]interface{}{
		"device_id":   deviceID,
		"device_name": device.Name,
		"connected":   true,
	}
	metadataJSON, _ := json.Marshal(metadata)
	conn.WriteMessage(websocket.TextMessage, metadataJSON)

	// Stream frames
	done := make(chan bool)

	// Read messages from client (for keepalive)
	go func() {
		for {
			_, _, err := conn.ReadMessage()
			if err != nil {
				done <- true
				return
			}
		}
	}()

	// Send frames
	ticker := time.NewTicker(30 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case frame := <-clientChan:
			if err := conn.WriteMessage(websocket.BinaryMessage, frame); err != nil {
				return
			}

		case <-ticker.C:
			// Keepalive ping
			if err := conn.WriteMessage(websocket.PingMessage, []byte{}); err != nil {
				return
			}

		case <-done:
			return
		}
	}
}

// Stream info endpoint
// GET /devices/:device_id/stream/snapshot
func streamSnapshotHandler(c *gin.Context) {
	deviceID := c.Param("device_id")
	userID, _ := auth.GetUserID(c)

	// Verify device ownership
	device, err := store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}
	if device.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Access denied"})
		return
	}

	frame := streamManager.GetLastFrame(deviceID)
	if frame == nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "No stream active"})
		return
	}

	// Force no-cache to ensure fresh frame
	c.Header("Cache-Control", "no-cache, no-store, must-revalidate")
	c.Header("Pragma", "no-cache")
	c.Header("Expires", "0")
	c.Data(http.StatusOK, "image/jpeg", frame)
}

// Stream info endpoint
// GET /devices/:device_id/stream/info
func streamInfoHandler(c *gin.Context) {
	deviceID := c.Param("device_id")
	userID, _ := auth.GetUserID(c)

	// Verify device ownership
	device, err := store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}
	if device.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Access denied"})
		return
	}

	stream := streamManager.GetStream(deviceID)
	if stream == nil {
		c.JSON(http.StatusOK, gin.H{
			"device_id": deviceID,
			"active":    false,
			"clients":   0,
		})
		return
	}

	stream.mu.RLock()
	defer stream.mu.RUnlock()

	c.JSON(http.StatusOK, gin.H{
		"device_id":    deviceID,
		"active":       true,
		"clients":      len(stream.Clients),
		"last_updated": stream.LastUpdated,
		"has_frame":    stream.LastFrame != nil,
		"frame_size":   len(stream.LastFrame),
	})
}

// StreamManager methods

func (sm *StreamManager) BroadcastFrame(deviceID string, frame []byte) {
	sm.mu.Lock()
	stream, exists := sm.streams[deviceID]
	if !exists {
		stream = &DeviceStream{
			DeviceID: deviceID,
			Clients:  make(map[string]*StreamClient),
		}
		sm.streams[deviceID] = stream
	}
	sm.mu.Unlock()

	stream.mu.Lock()
	stream.LastFrame = frame
	stream.LastUpdated = time.Now()

	// Broadcast to all clients
	for _, client := range stream.Clients {
		select {
		case client.Chan <- frame:
		default:
			// Drop frame if client buffer is full
		}
	}
	stream.mu.Unlock()
}

func (sm *StreamManager) RegisterClient(deviceID string) chan []byte {
	sm.mu.Lock()
	stream, exists := sm.streams[deviceID]
	if !exists {
		stream = &DeviceStream{
			DeviceID: deviceID,
			Clients:  make(map[string]*StreamClient),
		}
		sm.streams[deviceID] = stream
	}
	sm.mu.Unlock()

	stream.mu.Lock()
	defer stream.mu.Unlock()

	clientID := fmt.Sprintf("client_%d", time.Now().UnixNano())
	clientChan := make(chan []byte, 30) // Buffer 30 frames

	stream.Clients[clientID] = &StreamClient{
		ID:   clientID,
		Chan: clientChan,
	}

	return clientChan
}

func (sm *StreamManager) UnregisterClient(deviceID string, clientChan chan []byte) {
	sm.mu.RLock()
	stream, exists := sm.streams[deviceID]
	sm.mu.RUnlock()

	if !exists {
		return
	}

	stream.mu.Lock()
	defer stream.mu.Unlock()

	for id, client := range stream.Clients {
		if client.Chan == clientChan {
			close(client.Chan)
			delete(stream.Clients, id)
			break
		}
	}
}

func (sm *StreamManager) GetLastFrame(deviceID string) []byte {
	sm.mu.RLock()
	stream, exists := sm.streams[deviceID]
	sm.mu.RUnlock()

	if !exists {
		return nil
	}

	stream.mu.RLock()
	defer stream.mu.RUnlock()

	return stream.LastFrame
}

func (sm *StreamManager) GetClientCount(deviceID string) int {
	sm.mu.RLock()
	stream, exists := sm.streams[deviceID]
	sm.mu.RUnlock()

	if !exists {
		return 0
	}

	stream.mu.RLock()
	defer stream.mu.RUnlock()

	return len(stream.Clients)
}

func (sm *StreamManager) GetStream(deviceID string) *DeviceStream {
	sm.mu.RLock()
	defer sm.mu.RUnlock()

	return sm.streams[deviceID]
}
