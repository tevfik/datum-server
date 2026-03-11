package handlers

import (
	"io"
	"net/http"
	"os"
	"strings"

	"datum-go/internal/logger"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

// WebSocket upgrader for log streaming
var wsUpgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024,
	CheckOrigin: func(r *http.Request) bool {
		origin := r.Header.Get("Origin")
		if origin == "" {
			return true // Allow non-browser clients (CLI, devices)
		}
		allowed := os.Getenv("CORS_ALLOWED_ORIGINS")
		if allowed == "" || allowed == "*" {
			return true
		}
		for _, a := range strings.Split(allowed, ",") {
			if strings.TrimSpace(a) == origin {
				return true
			}
		}
		return false
	},
}

// ============ Logs Management Handlers ============

// StreamLogsHandler handles WebSocket connections for real-time log streaming
func (h *AdminHandler) StreamLogsHandler(c *gin.Context) {
	conn, err := wsUpgrader.Upgrade(c.Writer, c.Request, nil)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Failed to upgrade to WebSocket: " + err.Error()})
		return
	}

	// Register client for log broadcast
	logger.Broadcaster.AddClient(conn)

	// Send initial message
	conn.WriteMessage(websocket.TextMessage, []byte(`{"type":"connected","message":"Log streaming started"}`))

	// Keep connection alive - read messages to detect disconnection
	for {
		_, _, err := conn.ReadMessage()
		if err != nil {
			logger.Broadcaster.RemoveClient(conn)
			break
		}
	}
}

func (h *AdminHandler) GetLogsHandler(c *gin.Context) {
	logType := c.DefaultQuery("type", "")
	level := c.DefaultQuery("level", "")
	search := c.DefaultQuery("search", "")

	logPath := logger.LogFilePath
	if logPath == "" {
		c.JSON(http.StatusOK, gin.H{"logs": []gin.H{}, "total": 0, "message": "File logging not enabled"})
		return
	}

	lines, err := readLastLines(logPath, 500)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to read logs: " + err.Error()})
		return
	}

	var logs []gin.H
	for _, line := range lines {
		if line == "" {
			continue
		}

		// Simple filtering on raw string
		if level != "" && !strings.Contains(strings.ToUpper(line), strings.ToUpper(level)) {
			continue
		}
		if search != "" && !strings.Contains(strings.ToLower(line), strings.ToLower(search)) {
			continue
		}
		if logType != "" && !strings.Contains(line, logType) {
			continue
		}

		logs = append(logs, gin.H{
			"raw": line,
		})
	}

	// Reverse logs to show newest first
	for i, j := 0, len(logs)-1; i < j; i, j = i+1, j-1 {
		logs[i], logs[j] = logs[j], logs[i]
	}

	c.JSON(http.StatusOK, gin.H{
		"logs":  logs,
		"total": len(logs),
	})
}

func (h *AdminHandler) ClearLogsHandler(c *gin.Context) {
	logPath := logger.LogFilePath
	if logPath == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "File logging not enabled"})
		return
	}

	if err := os.Truncate(logPath, 0); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to clear logs: " + err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "Logs cleared successfully",
	})
}

// readLastLines reads the last N lines from a file
// efficient enough for a few MBs
func readLastLines(filename string, n int) ([]string, error) {
	file, err := os.Open(filename)
	if err != nil {
		return nil, err
	}
	defer file.Close()

	// 1. Get file size
	stat, err := file.Stat()
	if err != nil {
		return nil, err
	}
	filesize := stat.Size()

	// 2. Read file (for now simple implementation: read all, split, take last N)
	// Optimization: seek to end and read backwards is better but more complex.
	// Given typical log sizes for this project, reading 1-5MB into memory is okay.
	// If file is huge (>10MB), we should limit.

	startPos := int64(0)
	if filesize > 5*1024*1024 { // If > 5MB
		startPos = filesize - 5*1024*1024 // Read last 5MB
	}

	buf := make([]byte, filesize-startPos)
	_, err = file.ReadAt(buf, startPos)
	if err != nil && err != io.EOF {
		return nil, err
	}

	strContent := string(buf)
	lines := strings.Split(strContent, "\n")

	if len(lines) > n {
		return lines[len(lines)-n:], nil
	}
	return lines, nil
}
