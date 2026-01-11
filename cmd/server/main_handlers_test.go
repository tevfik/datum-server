package main

import (
	"bytes"
	"datum-go/internal/processing"
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

func setupMainTestServer(t *testing.T) (*gin.Engine, func()) {
	gin.SetMode(gin.TestMode)

	tmpDir := t.TempDir()
	var err error
	store, err = storage.New(tmpDir+"/meta.db", tmpDir+"/tsdata", 7*24*time.Hour)
	require.NoError(t, err)
	telemetryProcessor = processing.NewTelemetryProcessor(store)

	router := gin.New()
	cleanup := func() {
		store.Close()
	}

	return router, cleanup
}

func TestRegisterHandler(t *testing.T) {
	router, cleanup := setupMainTestServer(t)
	defer cleanup()

	// Initialize system to allow registration
	store.InitializeSystem("Test Platform", true, 7)

	router.POST("/auth/register", registerHandler)

	body := map[string]interface{}{
		"email":    "newuser@test.com",
		"password": "SecurePass123!",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/auth/register", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response, "token")
	assert.Contains(t, response, "user_id")
}

func TestRegisterHandlerInvalidJSON(t *testing.T) {
	router, cleanup := setupMainTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	router.POST("/auth/register", registerHandler)

	req := httptest.NewRequest(http.MethodPost, "/auth/register", bytes.NewReader([]byte("{invalid")))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestRegisterHandlerDuplicateEmail(t *testing.T) {
	router, cleanup := setupMainTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	// Create existing user
	user := &storage.User{
		ID:           "existing",
		Email:        "existing@test.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user)

	router.POST("/auth/register", registerHandler)

	body := map[string]interface{}{
		"email":    "existing@test.com",
		"password": "password123",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/auth/register", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusConflict, w.Code)
}

func TestPostDataHandler(t *testing.T) {
	router, cleanup := setupMainTestServer(t)
	defer cleanup()

	// Create test device
	device := &storage.Device{
		ID:        "data-device-001",
		UserID:    "data-user",
		Name:      "Data Device",
		Type:      "sensor",
		APIKey:    "sk_data_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.POST("/api/dev/:device_id/data", func(c *gin.Context) {
		c.Set("api_key", "sk_data_key")
		postDataHandler(c)
	})

	body := map[string]interface{}{
		"temperature": 25.5,
		"humidity":    65.0,
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/api/dev/data-device-001/data", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestPostDataHandlerInvalidAPIKey(t *testing.T) {
	router, cleanup := setupMainTestServer(t)
	defer cleanup()

	device := &storage.Device{
		ID:        "data-device-002",
		UserID:    "owner-user",
		Name:      "Device",
		Type:      "sensor",
		APIKey:    "sk_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.POST("/api/dev/:device_id/data", func(c *gin.Context) {
		c.Set("api_key", "wrong_key")
		postDataHandler(c)
	})

	body := map[string]interface{}{
		"temperature": 25.5,
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/api/dev/data-device-002/data", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func TestGetLatestDataHandler(t *testing.T) {
	router, cleanup := setupMainTestServer(t)
	defer cleanup()

	device := &storage.Device{
		ID:        "latest-device",
		UserID:    "latest-user",
		Name:      "Latest Device",
		Type:      "sensor",
		APIKey:    "sk_latest",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	// Store some data
	point := &storage.DataPoint{
		DeviceID:  "latest-device",
		Timestamp: time.Now(),
		Data: map[string]interface{}{
			"temperature": 22.5,
		},
	}
	store.StoreData(point)
	time.Sleep(100 * time.Millisecond)

	router.GET("/api/dev/:device_id/data/latest", func(c *gin.Context) {
		c.Set("user_id", "latest-user")
		getLatestDataHandler(c)
	})

	req := httptest.NewRequest(http.MethodGet, "/api/dev/latest-device/data/latest", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestGetLatestDataHandlerNoData(t *testing.T) {
	router, cleanup := setupMainTestServer(t)
	defer cleanup()

	device := &storage.Device{
		ID:        "nodata-device",
		UserID:    "nodata-user",
		Name:      "No Data Device",
		Type:      "sensor",
		APIKey:    "sk_nodata",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.GET("/api/dev/:device_id/data/latest", func(c *gin.Context) {
		c.Set("user_id", "nodata-user")
		getLatestDataHandler(c)
	})

	req := httptest.NewRequest(http.MethodGet, "/api/dev/nodata-device/data/latest", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func TestGetDataHistoryHandler(t *testing.T) {
	router, cleanup := setupMainTestServer(t)
	defer cleanup()

	device := &storage.Device{
		ID:        "history-device",
		UserID:    "history-user",
		Name:      "History Device",
		Type:      "sensor",
		APIKey:    "sk_history",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	// Store multiple data points
	for i := 0; i < 3; i++ {
		point := &storage.DataPoint{
			DeviceID:  "history-device",
			Timestamp: time.Now().Add(time.Duration(-i) * time.Minute),
			Data: map[string]interface{}{
				"value": i,
			},
		}
		store.StoreData(point)
	}
	time.Sleep(200 * time.Millisecond)

	router.GET("/api/dev/:device_id/data/history", func(c *gin.Context) {
		c.Set("user_id", "history-user")
		getDataHistoryHandler(c)
	})

	req := httptest.NewRequest(http.MethodGet, "/api/dev/history-device/data/history", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestGetDataHistoryHandlerWithLimit(t *testing.T) {
	router, cleanup := setupMainTestServer(t)
	defer cleanup()

	device := &storage.Device{
		ID:        "limit-history-device",
		UserID:    "limit-user",
		Name:      "Limit Device",
		Type:      "sensor",
		APIKey:    "sk_limit",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	for i := 0; i < 10; i++ {
		point := &storage.DataPoint{
			DeviceID:  "limit-history-device",
			Timestamp: time.Now().Add(time.Duration(-i) * time.Minute),
			Data:      map[string]interface{}{"value": i},
		}
		store.StoreData(point)
	}
	time.Sleep(200 * time.Millisecond)

	router.GET("/api/dev/:device_id/data/history", func(c *gin.Context) {
		c.Set("user_id", "limit-user")
		getDataHistoryHandler(c)
	})

	req := httptest.NewRequest(http.MethodGet, "/api/dev/limit-history-device/data/history?limit=5", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestGetDataHistoryHandlerWithRange(t *testing.T) {
	router, cleanup := setupMainTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	device := &storage.Device{
		ID:        "range-history-device",
		UserID:    "range-user",
		Name:      "Range History Device",
		Type:      "sensor",
		APIKey:    "sk_range_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	// Add some test data
	for i := 0; i < 5; i++ {
		point := &storage.DataPoint{
			DeviceID:  "range-history-device",
			Timestamp: time.Now().Add(time.Duration(-i) * time.Hour),
			Data:      map[string]interface{}{"temp": 20 + i},
		}
		store.StoreData(point)
	}
	time.Sleep(200 * time.Millisecond)

	router.GET("/api/dev/:device_id/data/history", func(c *gin.Context) {
		c.Set("user_id", "range-user")
		getDataHistoryHandler(c)
	})

	req := httptest.NewRequest(http.MethodGet, "/api/dev/range-history-device/data/history?range=24h", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestGetDataHistoryHandlerWithStartStop(t *testing.T) {
	router, cleanup := setupMainTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	device := &storage.Device{
		ID:        "startstop-device",
		UserID:    "startstop-user",
		Name:      "Start/Stop Device",
		Type:      "sensor",
		APIKey:    "sk_startstop_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.GET("/api/dev/:device_id/data/history", func(c *gin.Context) {
		c.Set("user_id", "startstop-user")
		getDataHistoryHandler(c)
	})

	startMs := time.Now().Add(-2 * time.Hour).UnixMilli()
	req := httptest.NewRequest(http.MethodGet,
		"/api/dev/startstop-device/data/history?start="+string(rune(startMs))+"&stop=1h", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestGetDataHistoryHandlerWithInterval(t *testing.T) {
	router, cleanup := setupMainTestServer(t)
	defer cleanup()

	store.InitializeSystem("Test", true, 7)

	device := &storage.Device{
		ID:        "interval-device",
		UserID:    "interval-user",
		Name:      "Interval Device",
		Type:      "sensor",
		APIKey:    "sk_interval_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	router.GET("/api/dev/:device_id/data/history", func(c *gin.Context) {
		c.Set("user_id", "interval-user")
		getDataHistoryHandler(c)
	})

	req := httptest.NewRequest(http.MethodGet,
		"/api/dev/interval-device/data/history?range=7d&interval=1h", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}
