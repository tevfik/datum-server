// Package data provides HTTP handlers for telemetry data endpoints.
package data

import (
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"sort"
	"strconv"
	"strings"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/mqtt"
	"datum-go/internal/processing"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// Handler provides data HTTP handlers.
type Handler struct {
	Store      storage.Provider
	Processor  *processing.TelemetryProcessor
	MQTTBroker *mqtt.Broker
}

// NewHandler creates a new data handler with dependencies.
func NewHandler(store storage.Provider, processor *processing.TelemetryProcessor, broker *mqtt.Broker) *Handler {
	return &Handler{
		Store:      store,
		Processor:  processor,
		MQTTBroker: broker,
	}
}

// RegisterRoutes registers data routes that require device auth.
func (h *Handler) RegisterRoutes(r *gin.RouterGroup) {
	r.POST("/:device_id", h.PostData)
}

// RegisterUserRoutes registers data routes that require user auth.
func (h *Handler) RegisterUserRoutes(r *gin.RouterGroup) {
	r.GET("/:device_id", h.GetData)
}

// RegisterHybridRoutes registers routes compatible with hybrid auth.
func (h *Handler) RegisterHybridRoutes(r *gin.RouterGroup) {
	r.GET("/:device_id/data", h.GetData)
}

// ============ Response types ============

// DataResponse represents a single data point response.
type DataResponse struct {
	Timestamp   string                 `json:"timestamp"`
	TimestampMs int64                  `json:"timestamp_ms"`
	Data        map[string]interface{} `json:"data"`
}

// ============ Handlers ============

// PostData handles data ingestion from devices.
// POST /data/:device_id
func (h *Handler) PostData(c *gin.Context) {
	deviceID := c.Param("device_id")
	apiKeyVal, exists := c.Get("api_key")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Missing API key"})
		return
	}
	apiKey, ok := apiKeyVal.(string)
	if !ok {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid API key format"})
		return
	}

	device, err := h.Store.GetDeviceByAPIKey(apiKey)
	if err != nil || device.ID != deviceID {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid device credentials"})
		return
	}

	var data map[string]interface{}
	if err := c.ShouldBindJSON(&data); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	result, err := h.Processor.Process(deviceID, data)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	// Bridge to MQTT for Live UI Updates
	if h.MQTTBroker != nil {
		if jsonBytes, err := json.Marshal(data); err == nil {
			go func() {
				topic := fmt.Sprintf("dev/%s/data", deviceID)
				h.MQTTBroker.Publish(topic, jsonBytes, false)
			}()
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"status":           "ok",
		"timestamp":        result.Timestamp.Format(time.RFC3339),
		"commands_pending": result.CommandsPending,
	})
}

// GetData returns data for a device.
// GET /data/:device_id
func (h *Handler) GetData(c *gin.Context) {
	deviceID := c.Param("device_id")
	userID, userErr := auth.GetUserID(c)

	device, err := h.Store.GetDevice(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Device not found"})
		return
	}
	if userErr != nil || device.UserID != userID {
		role, _ := auth.GetUserRole(c)
		isAuthorized := false

		if role == "admin" {
			isAuthorized = true
		} else if val, exists := c.Get("api_key"); exists {
			// Device Auth check
			apiKey := val.(string)
			if requesterDevice, err := h.Store.GetDeviceByAPIKey(apiKey); err == nil {
				if requesterDevice.ID == device.ID {
					isAuthorized = true
				}
			}
		}

		if !isAuthorized {
			c.JSON(http.StatusForbidden, gin.H{"error": "Access denied"})
			return
		}
	}

	// Check if this is a history query
	hasHistoryParams := false
	for _, param := range []string{"limit", "start", "stop", "range", "start_rfc", "end_rfc", "int"} {
		if c.Query(param) != "" {
			hasHistoryParams = true
			break
		}
	}

	// === LATEST DATA MODE ===
	if !hasHistoryParams {
		point, err := h.Store.GetLatestData(deviceID)
		if err != nil {
			c.JSON(http.StatusNotFound, gin.H{"error": "No data found"})
			return
		}

		if data := point.Data; data != nil {
			if ip, ok := data["public_ip"].(string); ok && strings.HasPrefix(ip, "172.") {
				data["public_ip"] = ""
			}
		}

		c.JSON(http.StatusOK, gin.H{
			"device_id": deviceID,
			"timestamp": point.Timestamp.Format(time.RFC3339),
			"data":      point.Data,
		})
		return
	}

	// === HISTORY MODE ===
	maxLimit := 10000
	if v := os.Getenv("QUERY_MAX_LIMIT"); v != "" {
		if parsed, err := strconv.Atoi(v); err == nil && parsed > 0 {
			maxLimit = parsed
		}
	}
	limit := 1000
	if limitStr := c.Query("limit"); limitStr != "" {
		fmt.Sscanf(limitStr, "%d", &limit)
	}
	if limit <= 0 {
		limit = 1000
	}
	if limit > maxLimit {
		limit = maxLimit
	}

	end := time.Now()
	start := end.Add(-7 * 24 * time.Hour)
	interval := time.Duration(0)

	// Parse duration helper
	parseDuration := func(s string) time.Duration {
		switch s {
		case "1s":
			return time.Second
		case "1m":
			return time.Minute
		case "1h":
			return time.Hour
		case "1d":
			return 24 * time.Hour
		case "7d":
			return 7 * 24 * time.Hour
		case "30d":
			return 30 * 24 * time.Hour
		case "24h":
			return 24 * time.Hour
		default:
			return 0
		}
	}

	if startMs := c.Query("start"); startMs != "" {
		if ms, err := strconv.ParseInt(startMs, 10, 64); err == nil {
			start = time.UnixMilli(ms)
		}
	}

	if stopStr := c.Query("stop"); stopStr != "" {
		if dur := parseDuration(stopStr); dur > 0 {
			end = start.Add(dur)
		}
	}

	if rangeStr := c.Query("range"); rangeStr != "" {
		if dur := parseDuration(rangeStr); dur > 0 {
			end = time.Now()
			start = end.Add(-dur)
		}
	}

	if startRFC := c.Query("start_rfc"); startRFC != "" {
		if t, err := time.Parse(time.RFC3339, startRFC); err == nil {
			start = t
		}
	}
	if endRFC := c.Query("end_rfc"); endRFC != "" {
		if t, err := time.Parse(time.RFC3339, endRFC); err == nil {
			end = t
		}
	}

	if intStr := c.Query("int"); intStr != "" {
		interval = parseDuration(intStr)
	}

	points, err := h.Store.GetDataHistoryWithRange(deviceID, start, end, limit)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	var response []DataResponse

	if interval > 0 && len(points) > 0 {
		buckets := make(map[int64][]storage.DataPoint)

		for _, p := range points {
			bucket := p.Timestamp.UnixMilli() / int64(interval/time.Millisecond) * int64(interval/time.Millisecond)
			buckets[bucket] = append(buckets[bucket], p)
		}

		var bucketKeys []int64
		for k := range buckets {
			bucketKeys = append(bucketKeys, k)
		}
		sort.Slice(bucketKeys, func(i, j int) bool {
			return bucketKeys[i] > bucketKeys[j] // Descending (newest first)
		})

		for _, bucket := range bucketKeys {
			pts := buckets[bucket]
			avgData := make(map[string]interface{})
			fieldSums := make(map[string]float64)
			fieldCounts := make(map[string]int)

			for _, p := range pts {
				for k, v := range p.Data {
					if fv, ok := v.(float64); ok {
						fieldSums[k] += fv
						fieldCounts[k]++
					}
				}
			}

			for k, sum := range fieldSums {
				avgData[k] = sum / float64(fieldCounts[k])
			}

			response = append(response, DataResponse{
				Timestamp:   time.UnixMilli(bucket).Format(time.RFC3339),
				TimestampMs: bucket,
				Data:        avgData,
			})
		}
	} else {
		for _, p := range points {
			data := p.Data
			if ip, ok := data["public_ip"].(string); ok && strings.HasPrefix(ip, "172.") {
				data["public_ip"] = ""
			}
			response = append(response, DataResponse{
				Timestamp:   p.Timestamp.Format(time.RFC3339),
				TimestampMs: p.Timestamp.UnixMilli(),
				Data:        data,
			})
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"device_id": deviceID,
		"count":     len(response),
		"data":      response,
		"range": gin.H{
			"start":    start.Format(time.RFC3339),
			"start_ms": start.UnixMilli(),
			"end":      end.Format(time.RFC3339),
			"end_ms":   end.UnixMilli(),
		},
		"interval": interval.String(),
	})
}
