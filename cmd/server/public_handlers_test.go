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

func setupPublicTestServer(t *testing.T) (*gin.Engine, func()) {
	gin.SetMode(gin.TestMode)

	tmpDir := t.TempDir()
	var err error
	store, err = storage.New(tmpDir+"/meta.db", tmpDir+"/tsdata", 7*24*time.Hour)
	require.NoError(t, err)

	router := gin.New()
	cleanup := func() {
		store.Close()
	}

	return router, cleanup
}

func TestPostPublicDataHandler(t *testing.T) {
	router, cleanup := setupPublicTestServer(t)
	defer cleanup()

	router.POST("/pub/:device_id/data", postPublicDataHandler)

	body := map[string]interface{}{
		"temperature": 22.5,
		"humidity":    55.0,
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/pub/test-device-123/data", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "ok", response["status"])
	assert.Equal(t, "public", response["mode"])
	assert.Contains(t, response, "timestamp")
}

func TestPostPublicDataHandlerInvalidJSON(t *testing.T) {
	router, cleanup := setupPublicTestServer(t)
	defer cleanup()

	router.POST("/pub/:device_id/data", postPublicDataHandler)

	req := httptest.NewRequest(http.MethodPost, "/pub/test-device-123/data", bytes.NewReader([]byte("{invalid")))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestGetPublicDataHandler(t *testing.T) {
	router, cleanup := setupPublicTestServer(t)
	defer cleanup()

	// Insert test data
	point := &storage.DataPoint{
		DeviceID:  "public_test-device-456",
		Timestamp: time.Now(),
		Data: map[string]interface{}{
			"temperature": 23.5,
			"humidity":    60.0,
		},
	}
	store.StoreData(point)
	time.Sleep(100 * time.Millisecond) // Allow time for storage

	router.GET("/pub/:device_id/data", getPublicDataHandler)

	req := httptest.NewRequest(http.MethodGet, "/pub/test-device-456/data", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "test-device-456", response["device_id"])
	assert.Equal(t, "public", response["mode"])
	assert.Contains(t, response, "data")
	assert.Contains(t, response, "timestamp")
}

func TestGetPublicDataHandlerNoData(t *testing.T) {
	router, cleanup := setupPublicTestServer(t)
	defer cleanup()

	router.GET("/pub/:device_id/data", getPublicDataHandler)

	req := httptest.NewRequest(http.MethodGet, "/pub/nonexistent/data", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func TestGetPublicDataHistoryHandler(t *testing.T) {
	router, cleanup := setupPublicTestServer(t)
	defer cleanup()

	// Insert multiple test data points
	for i := 0; i < 5; i++ {
		point := &storage.DataPoint{
			DeviceID:  "public_history-device",
			Timestamp: time.Now().Add(time.Duration(-i) * time.Minute),
			Data: map[string]interface{}{
				"temperature": 20.0 + float64(i),
			},
		}
		store.StoreData(point)
	}
	time.Sleep(200 * time.Millisecond) // Allow time for storage

	router.GET("/pub/:device_id/history", getPublicDataHistoryHandler)

	req := httptest.NewRequest(http.MethodGet, "/pub/history-device/history", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "device_id")
	assert.Contains(t, response, "data")
	assert.Equal(t, "public", response["mode"])
}

func TestGetPublicDataHistoryHandlerWithLimit(t *testing.T) {
	router, cleanup := setupPublicTestServer(t)
	defer cleanup()

	// Insert multiple test data points
	for i := 0; i < 10; i++ {
		point := &storage.DataPoint{
			DeviceID:  "public_limit-device",
			Timestamp: time.Now().Add(time.Duration(-i) * time.Minute),
			Data: map[string]interface{}{
				"value": i,
			},
		}
		store.StoreData(point)
	}
	time.Sleep(200 * time.Millisecond)

	router.GET("/pub/:device_id/history", getPublicDataHistoryHandler)

	req := httptest.NewRequest(http.MethodGet, "/pub/limit-device/history?limit=3", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "data")

	data := response["data"].([]interface{})
	assert.LessOrEqual(t, len(data), 3)
}

// TestGenerateIDString tests helper function
func TestGenerateIDStringHelper(t *testing.T) {
	id1 := generateIDString(8)
	id2 := generateIDString(8)

	assert.NotEqual(t, id1, id2, "IDs should be unique")
	assert.Greater(t, len(id1), 10)
}

// TestGenerateCommandID tests command ID generation
func TestGenerateCommandIDHelper(t *testing.T) {
	id1 := generateCommandID()
	id2 := generateCommandID()

	assert.NotEqual(t, id1, id2, "Command IDs should be unique")
	assert.Contains(t, id1, "cmd_")
}

// TestSecurityHeadersMiddleware tests security headers
func TestSecurityHeadersMiddlewareApplied(t *testing.T) {
	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.Use(securityHeadersMiddleware())
	r.GET("/test", func(c *gin.Context) {
		c.String(http.StatusOK, "OK")
	})

	req, _ := http.NewRequest("GET", "/test", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	assert.Equal(t, "DENY", w.Header().Get("X-Frame-Options"))
	assert.Equal(t, "nosniff", w.Header().Get("X-Content-Type-Options"))
	assert.Equal(t, "1; mode=block", w.Header().Get("X-XSS-Protection"))
	assert.NotEmpty(t, w.Header().Get("Referrer-Policy"))
	assert.NotEmpty(t, w.Header().Get("Content-Security-Policy"))
}
