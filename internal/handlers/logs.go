package handlers

import (
	"bytes"
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
		return true // Allow all origins for admin access
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

	stat, err := file.Stat()
	if err != nil {
		return nil, err
	}
	filesize := stat.Size()

	if filesize == 0 {
		return []string{}, nil
	}

	// Buffer size for chunks
	const bufSize = 4096
	// Safety limit to prevent reading entire huge files
	const maxBytesToRead = 5 * 1024 * 1024

	var lines []string
	// Pre-allocate lines slice to avoid resizing if n is large
	lines = make([]string, 0, n)

	var cursor int64 = filesize
	var totalRead int64 = 0
	var remainder []byte

	for cursor > 0 && len(lines) < n && totalRead < maxBytesToRead {
		chunkSize := int64(bufSize)
		if cursor < chunkSize {
			chunkSize = cursor
		}

		cursor -= chunkSize
		totalRead += chunkSize

		buf := make([]byte, chunkSize)
		_, err := file.ReadAt(buf, cursor)
		if err != nil {
			return nil, err
		}

		// Prepend chunk to remainder
		// data = chunk + remainder
		data := append(buf, remainder...)

		// Reset remainder, we will compute new remainder
		remainder = nil

		// Process data from end to start
		right := len(data)
		for {
			// Find newline from right
			// We scan data[:right]
			if right == 0 {
				break
			}

			// bytes.LastIndexByte is efficient
			idx := bytes.LastIndexByte(data[:right], '\n')

			if idx == -1 {
				// No more newlines in this data
				remainder = data[:right]
				break
			}

			// Found newline at idx
			// Line is data[idx+1 : right]
			lineBytes := data[idx+1 : right]
			lineStr := string(lineBytes)

			lines = append(lines, lineStr)

			if len(lines) >= n {
				break
			}

			// Move right cursor
			right = idx
		}

		if len(lines) >= n {
			break
		}
	}

	// If we have leftover remainder and we haven't reached limit of N
	if len(remainder) > 0 && len(lines) < n {
		lines = append(lines, string(remainder))
	}

	// Reverse lines to restore file order (Oldest -> Newest)
	// Currently lines is [Newest, ..., Oldest]
	for i, j := 0, len(lines)-1; i < j; i, j = i+1, j-1 {
		lines[i], lines[j] = lines[j], lines[i]
	}

	return lines, nil
}
