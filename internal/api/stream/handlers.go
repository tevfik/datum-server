// Package stream provides HTTP handlers for video streaming.
package stream

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"strings"
	"sync"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

// Handler provides stream HTTP handlers.
type Handler struct {
	Store         storage.Provider
	StreamManager *StreamManager
}

// NewHandler creates a new stream handler with dependencies.
func NewHandler(store storage.Provider) *Handler {
	return &Handler{
		Store: store,
		StreamManager: &StreamManager{
			streams: make(map[string]*DeviceStream),
		},
	}
}

// RegisterDeviceRoutes registers stream routes that require device auth.
func (h *Handler) RegisterDeviceRoutes(r *gin.RouterGroup) {
	r.POST("/:device_id/stream/frame", h.UploadFrame)
}

// RegisterHybridRoutes registers stream routes that allow both User and Device auth.
func (h *Handler) RegisterHybridRoutes(r *gin.RouterGroup) {
	r.GET("/:device_id/stream/mjpeg", h.MJPEGStream)
	r.GET("/:device_id/stream/snapshot", h.StreamSnapshot)
	r.GET("/:device_id/stream/ws", h.WebSocketStream)
	r.GET("/:device_id/stream/info", h.StreamInfo)
}

// ============ Stream Manager ============

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

var wsUpgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool {
		allowedOrigins := os.Getenv("CORS_ALLOWED_ORIGINS")
		if allowedOrigins == "" || allowedOrigins == "*" {
			return true
		}
		origin := r.Header.Get("Origin")
		return strings.Contains(allowedOrigins, origin)
	},
	ReadBufferSize:  1024,
	WriteBufferSize: 1024 * 1024, // 1MB for video frames
}

// ============ Handlers ============

// UploadFrame receives video frames from devices.
// POST /dev/:device_id/stream/frame
func (h *Handler) UploadFrame(c *gin.Context) {
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
		// Token-based auth
		device, _, err = h.Store.GetDeviceByToken(apiKey.(string))
	} else {
		// API Key-based auth
		device, err = h.Store.GetDeviceByAPIKey(apiKey.(string))
	}

	if err != nil || device.ID != deviceID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Device not found or unauthorized"})
		return
	}

	// Read frame data
	var frameData []byte
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

	// Broadcast frame
	h.StreamManager.BroadcastFrame(deviceID, frameData)

	// Update device last seen
	go h.Store.UpdateDeviceLastSeen(deviceID)

	c.JSON(http.StatusOK, gin.H{
		"status":     "frame_received",
		"size_bytes": len(frameData),
		"clients":    h.StreamManager.GetClientCount(deviceID),
	})
}

// MJPEGStream serves MJPEG stream.
// GET /dev/:device_id/stream/mjpeg
func (h *Handler) MJPEGStream(c *gin.Context) {
	deviceID := c.Param("device_id")
	if !h.AuthorizeStreamAccess(c, deviceID) {
		return
	}

	c.Header("Content-Type", "multipart/x-mixed-replace; boundary=frame")
	c.Header("Cache-Control", "no-cache, no-store, must-revalidate")
	c.Header("Pragma", "no-cache")
	c.Header("Expires", "0")
	c.Header("Connection", "keep-alive")
	c.Header("X-Accel-Buffering", "no")

	clientChan := h.StreamManager.RegisterClient(deviceID)
	defer h.StreamManager.UnregisterClient(deviceID, clientChan)

	// Send initial frame
	if frame := h.StreamManager.GetLastFrame(deviceID); frame != nil {
		c.Writer.Write([]byte("--frame\r\n"))
		c.Writer.Write([]byte("Content-Type: image/jpeg\r\n"))
		c.Writer.Write([]byte(fmt.Sprintf("Content-Length: %d\r\n\r\n", len(frame))))
		c.Writer.Write(frame)
		c.Writer.Write([]byte("\r\n"))
		c.Writer.Flush()
	}

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
			c.Writer.Write([]byte("\r\n"))
			c.Writer.Flush()

		case <-c.Request.Context().Done():
			return
		}
	}
}

// WebSocketStream serves binary WebSocket stream.
// GET /dev/:device_id/stream/ws
func (h *Handler) WebSocketStream(c *gin.Context) {
	deviceID := c.Param("device_id")
	userID, _ := auth.GetUserID(c)

	device, err := h.Store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}
	if device.UserID != userID {
		role, _ := auth.GetUserRole(c)
		if role != "admin" {
			c.JSON(http.StatusForbidden, gin.H{"error": "Access denied"})
			return
		}
	}

	conn, err := wsUpgrader.Upgrade(c.Writer, c.Request, nil)
	if err != nil {
		return
	}
	defer conn.Close()

	clientChan := h.StreamManager.RegisterClient(deviceID)
	defer h.StreamManager.UnregisterClient(deviceID, clientChan)

	if frame := h.StreamManager.GetLastFrame(deviceID); frame != nil {
		conn.WriteMessage(websocket.BinaryMessage, frame)
	}

	metadata := map[string]interface{}{
		"device_id":   deviceID,
		"device_name": device.Name,
		"connected":   true,
	}
	metadataJSON, _ := json.Marshal(metadata)
	conn.WriteMessage(websocket.TextMessage, metadataJSON)

	done := make(chan bool)

	go func() {
		for {
			_, _, err := conn.ReadMessage()
			if err != nil {
				done <- true
				return
			}
		}
	}()

	ticker := time.NewTicker(30 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case frame := <-clientChan:
			if err := conn.WriteMessage(websocket.BinaryMessage, frame); err != nil {
				return
			}

		case <-ticker.C:
			if err := conn.WriteMessage(websocket.PingMessage, []byte{}); err != nil {
				return
			}

		case <-done:
			return
		}
	}
}

// StreamSnapshot returns current frame snapshot.
// GET /dev/:device_id/stream/snapshot
func (h *Handler) StreamSnapshot(c *gin.Context) {
	deviceID := c.Param("device_id")
	if !h.AuthorizeStreamAccess(c, deviceID) {
		return
	}

	frame := h.StreamManager.GetLastFrame(deviceID)
	if frame == nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "No stream active"})
		return
	}

	c.Header("Cache-Control", "no-cache, no-store, must-revalidate")
	c.Header("Pragma", "no-cache")
	c.Header("Expires", "0")
	c.Header("Content-Type", "image/jpeg")
	c.Header("Content-Length", fmt.Sprintf("%d", len(frame)))
	c.Writer.Write(frame)
}

// StreamInfo returns stream metadata.
// GET /dev/:device_id/stream/info
func (h *Handler) StreamInfo(c *gin.Context) {
	deviceID := c.Param("device_id")
	userID, _ := auth.GetUserID(c)

	device, err := h.Store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}
	if device.UserID != userID {
		role, _ := auth.GetUserRole(c)
		if role != "admin" {
			c.JSON(http.StatusForbidden, gin.H{"error": "Access denied"})
			return
		}
	}

	stream := h.StreamManager.GetStream(deviceID)
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

// AuthorizeStreamAccess checks if the requester can access the stream.
// Supports both User (Session/JWT) and Device (API Key) auth.
func (h *Handler) AuthorizeStreamAccess(c *gin.Context, deviceID string) bool {
	userID, _ := auth.GetUserID(c)
	role, _ := auth.GetUserRole(c)

	device, err := h.Store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return false
	}

	if role == "admin" {
		return true
	}
	if userID != "" && device.UserID == userID {
		return true
	}
	if val, exists := c.Get("api_key"); exists {
		apiKey := val.(string)
		if requesterDevice, err := h.Store.GetDeviceByAPIKey(apiKey); err == nil {
			if requesterDevice.ID == device.ID || requesterDevice.UserID == device.UserID {
				return true
			}
		}
	}

	c.JSON(http.StatusForbidden, gin.H{"error": "Access denied"})
	return false
}

// ============ Stream Manager Methods ============

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

	for _, client := range stream.Clients {
		select {
		case client.Chan <- frame:
		default:
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
	clientChan := make(chan []byte, 2)

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
