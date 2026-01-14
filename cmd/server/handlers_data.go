package main

import (
	"encoding/json"
	"fmt"
	"net/http"
	"strconv"
	"strings"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// ============ Data Response types ============

type DataResponse struct {
	Timestamp   string                 `json:"timestamp"`
	TimestampMs int64                  `json:"timestamp_ms"`
	Data        map[string]interface{} `json:"data"`
}

// ============ Data Handlers ============

// postDataHandler handles data ingestion from devices
// POST /data/:device_id
func postDataHandler(c *gin.Context) {
	deviceID := c.Param("device_id")
	apiKey, _ := c.Get("api_key")

	device, err := store.GetDeviceByAPIKey(apiKey.(string))
	if err != nil || device.ID != deviceID {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid device credentials"})
		return
	}

	var data map[string]interface{}
	if err := c.ShouldBindJSON(&data); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	result, err := telemetryProcessor.Process(deviceID, data)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	// Bridge to MQTT for Live UI Updates
	if mqttBroker != nil {
		// Serialize data to JSON
		if jsonBytes, err := json.Marshal(data); err == nil {
			// Fire and forget - don't block HTTP response
			go func() {
				topic := fmt.Sprintf("dev/%s/data", deviceID)
				mqttBroker.Publish(topic, jsonBytes, false)
			}()
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"status":           "ok",
		"timestamp":        result.Timestamp.Format(time.RFC3339),
		"commands_pending": result.CommandsPending,
	})
}

// getDataHandler returns data for a device
// GET /data/:device_id
// If no query parameters are provided, it returns the latest data point (single object).
// If query parameters (limit, start, stop, range, etc.) are provided, it returns historical data (list).
func getDataHandler(c *gin.Context) {
	deviceID := c.Param("device_id")
	userID, _ := auth.GetUserID(c)

	device, err := store.GetDevice(deviceID)
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
		point, err := store.GetLatestData(deviceID)
		if err != nil {
			c.JSON(http.StatusNotFound, gin.H{"error": "No data found"})
			return
		}

		// Sanitize Public IP (fix for Docker/Internal IP leak)
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
	limit := 1000
	if limitStr := c.Query("limit"); limitStr != "" {
		fmt.Sscanf(limitStr, "%d", &limit)
	}

	end := time.Now()
	start := end.Add(-7 * 24 * time.Hour) // Default 7 days
	interval := time.Duration(0)          // No aggregation by default

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

	// Method 1: start (unix_ms) + stop (duration)
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

	// Method 2: range shorthand (1h, 24h, 7d, 30d) - backwards from now
	if rangeStr := c.Query("range"); rangeStr != "" {
		if dur := parseDuration(rangeStr); dur > 0 {
			end = time.Now()
			start = end.Add(-dur)
		}
	}

	// Method 3: RFC3339 start/end
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

	// Parse interval for aggregation
	if intStr := c.Query("int"); intStr != "" {
		interval = parseDuration(intStr)
	}

	// Get data
	points, err := store.GetDataHistoryWithRange(deviceID, start, end, limit)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	var response []DataResponse

	// If interval is specified, aggregate data
	if interval > 0 && len(points) > 0 {
		// Group by interval buckets
		buckets := make(map[int64][]storage.DataPoint)

		for _, p := range points {
			bucket := p.Timestamp.UnixMilli() / int64(interval/time.Millisecond) * int64(interval/time.Millisecond)
			buckets[bucket] = append(buckets[bucket], p)
		}

		// Compute averages for each bucket
		var bucketKeys []int64
		for k := range buckets {
			bucketKeys = append(bucketKeys, k)
		}
		// Sort buckets descending
		for i := 0; i < len(bucketKeys)-1; i++ {
			for j := i + 1; j < len(bucketKeys); j++ {
				if bucketKeys[i] < bucketKeys[j] {
					bucketKeys[i], bucketKeys[j] = bucketKeys[j], bucketKeys[i]
				}
			}
		}

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
