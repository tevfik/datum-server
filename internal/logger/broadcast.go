package logger

import (
	"sync"

	"github.com/gorilla/websocket"
)

// LogBroadcaster manages WebSocket connections for log streaming
type LogBroadcaster struct {
	clients map[*websocket.Conn]bool
	mu      sync.RWMutex
}

var Broadcaster = &LogBroadcaster{
	clients: make(map[*websocket.Conn]bool),
}

// AddClient registers a new WebSocket client for log streaming
func (b *LogBroadcaster) AddClient(conn *websocket.Conn) {
	b.mu.Lock()
	defer b.mu.Unlock()
	b.clients[conn] = true
}

// RemoveClient unregisters a WebSocket client
func (b *LogBroadcaster) RemoveClient(conn *websocket.Conn) {
	b.mu.Lock()
	defer b.mu.Unlock()
	delete(b.clients, conn)
	conn.Close()
}

// Broadcast sends a log message to all connected clients
func (b *LogBroadcaster) Broadcast(message []byte) {
	b.mu.RLock()
	defer b.mu.RUnlock()

	for conn := range b.clients {
		err := conn.WriteMessage(websocket.TextMessage, message)
		if err != nil {
			// Client disconnected, will be cleaned up on next operation
			go b.RemoveClient(conn)
		}
	}
}

// ClientCount returns the number of connected clients
func (b *LogBroadcaster) ClientCount() int {
	b.mu.RLock()
	defer b.mu.RUnlock()
	return len(b.clients)
}

// BroadcastWriter implements io.Writer and broadcasts logs to WebSocket clients
type BroadcastWriter struct{}

func (w BroadcastWriter) Write(p []byte) (n int, err error) {
	// Only broadcast if there are clients connected
	if Broadcaster.ClientCount() > 0 {
		// Make a copy to avoid race conditions
		msg := make([]byte, len(p))
		copy(msg, p)
		Broadcaster.Broadcast(msg)
	}
	return len(p), nil
}
