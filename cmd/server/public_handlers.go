package main

import (
	"datum-go/internal/storage"
	"fmt"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
)

// Public data handlers (NO authentication required)

func postPublicDataHandler(c *gin.Context) {
	deviceID := c.Param("device_id")

	// Parse data
	var data map[string]interface{}
	if err := c.ShouldBindJSON(&data); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Store data (public namespace)
	point := &storage.DataPoint{
		DeviceID:  "public_" + deviceID, // Prefix to separate from authenticated devices
		Timestamp: time.Now(),
		Data:      data,
	}

	if err := store.StoreData(point); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":    "ok",
		"timestamp": point.Timestamp.Format(time.RFC3339),
		"mode":      "public",
	})
}

func getPublicDataHandler(c *gin.Context) {
	deviceID := "public_" + c.Param("device_id")

	// Check for history params
	hasHistoryParams := false
	for _, param := range []string{"limit"} { // Public data currently only supports limit (based on previous implementation)
		if c.Query(param) != "" {
			hasHistoryParams = true
			break
		}
	}

	// === LATEST DATA ===
	if !hasHistoryParams {
		point, err := store.GetLatestData(deviceID)
		if err != nil {
			c.JSON(http.StatusNotFound, gin.H{"error": "No data found"})
			return
		}

		c.JSON(http.StatusOK, gin.H{
			"device_id": c.Param("device_id"),
			"timestamp": point.Timestamp.Format(time.RFC3339),
			"data":      point.Data,
			"mode":      "public",
		})
		return
	}

	// === HISTORY ===
	limit := 100
	if limitStr := c.Query("limit"); limitStr != "" {
		fmt.Sscanf(limitStr, "%d", &limit)
	}

	points, err := store.GetDataHistory(deviceID, limit)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	type DataResponse struct {
		Timestamp string                 `json:"timestamp"`
		Data      map[string]interface{} `json:"data"`
	}

	var response []DataResponse
	for _, p := range points {
		response = append(response, DataResponse{
			Timestamp: p.Timestamp.Format(time.RFC3339),
			Data:      p.Data,
		})
	}

	c.JSON(http.StatusOK, gin.H{
		"device_id": c.Param("device_id"),
		"data":      response,
		"mode":      "public",
	})
}
