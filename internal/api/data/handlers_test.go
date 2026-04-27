package data

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"testing"
	"time"

	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func setupTestEnv(t *testing.T) (*Handler, storage.Provider, func()) {
	gin.SetMode(gin.TestMode)
	tmpDir := t.TempDir()
	store, err := storage.New(
		filepath.Join(tmpDir, "meta.db"),
		filepath.Join(tmpDir, "tsdata"),
		24*time.Hour,
	)
	require.NoError(t, err)

	handler := NewHandler(store, nil, nil) // no processor or broker for basic tests
	return handler, store, func() { store.Close() }
}

func createTestDevice(t *testing.T, store storage.Provider, userID, deviceID, apiKey string) {
	t.Helper()
	require.NoError(t, store.CreateDevice(&storage.Device{
		ID:        deviceID,
		UserID:    userID,
		Name:      "Test Device",
		Type:      "sensor",
		APIKey:    apiKey,
		CreatedAt: time.Now(),
	}))
}

// ---------- GetData ----------

func TestGetData_LatestAsOwner(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestDevice(t, store, "user1", "dev1", "sk_test_key")

	// Store a data point
	require.NoError(t, store.StoreData(&storage.DataPoint{
		DeviceID:  "dev1",
		Timestamp: time.Now(),
		Data:      map[string]interface{}{"temp": 22.5},
	}))

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "user1")
		c.Set("role", "user")
		c.Next()
	})
	r.GET("/data/:device_id", handler.GetData)

	req, _ := http.NewRequest("GET", "/data/dev1", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	assert.Equal(t, "dev1", resp["device_id"])
	assert.NotNil(t, resp["data"])
}

func TestGetData_ForbiddenForOtherUser(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestDevice(t, store, "user1", "dev1", "sk_test_key")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "user2")
		c.Set("role", "user")
		c.Next()
	})
	r.GET("/data/:device_id", handler.GetData)

	req, _ := http.NewRequest("GET", "/data/dev1", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)
}

func TestGetData_AdminCanAccessAnyDevice(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestDevice(t, store, "user1", "dev1", "sk_test_key")

	require.NoError(t, store.StoreData(&storage.DataPoint{
		DeviceID:  "dev1",
		Timestamp: time.Now(),
		Data:      map[string]interface{}{"temp": 23.0},
	}))

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "admin_user")
		c.Set("role", "admin")
		c.Next()
	})
	r.GET("/data/:device_id", handler.GetData)

	req, _ := http.NewRequest("GET", "/data/dev1", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestGetData_NotFound(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "user1")
		c.Set("role", "user")
		c.Next()
	})
	r.GET("/data/:device_id", handler.GetData)

	req, _ := http.NewRequest("GET", "/data/nonexistent", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func TestGetData_DeviceAuthBySameDevice(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestDevice(t, store, "user1", "dev1", "sk_test_key")
	require.NoError(t, store.StoreData(&storage.DataPoint{
		DeviceID:  "dev1",
		Timestamp: time.Now(),
		Data:      map[string]interface{}{"temp": 25.0},
	}))

	r := gin.New()
	r.Use(func(c *gin.Context) {
		// Simulate device auth (no user_id, but api_key present)
		c.Set("api_key", "sk_test_key")
		c.Next()
	})
	r.GET("/data/:device_id", handler.GetData)

	req, _ := http.NewRequest("GET", "/data/dev1", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestGetData_HistoryWithLimit(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestDevice(t, store, "user1", "dev1", "sk_test_key")

	// Store multiple data points
	for i := 0; i < 5; i++ {
		require.NoError(t, store.StoreData(&storage.DataPoint{
			DeviceID:  "dev1",
			Timestamp: time.Now().Add(time.Duration(i) * time.Second),
			Data:      map[string]interface{}{"temp": float64(20 + i)},
		}))
	}

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "user1")
		c.Set("role", "user")
		c.Next()
	})
	r.GET("/data/:device_id", handler.GetData)

	req, _ := http.NewRequest("GET", "/data/dev1?limit=3", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	data, ok := resp["data"].([]interface{})
	assert.True(t, ok)
	assert.LessOrEqual(t, len(data), 3)
}

// ---------- PostData ----------

func TestPostData_MissingAPIKey(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.POST("/data/:device_id", handler.PostData)

	body := bytes.NewBufferString(`{"temp": 22.5}`)
	req, _ := http.NewRequest("POST", "/data/dev1", body)
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func TestPostData_InvalidAPIKey(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestDevice(t, store, "user1", "dev1", "sk_valid_key")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("api_key", "sk_wrong_key")
		c.Next()
	})
	r.POST("/data/:device_id", handler.PostData)

	body := bytes.NewBufferString(`{"temp": 22.5}`)
	req, _ := http.NewRequest("POST", "/data/dev1", body)
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func TestPostData_InvalidJSON(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestDevice(t, store, "user1", "dev1", "sk_test_key")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("api_key", "sk_test_key")
		c.Next()
	})
	r.POST("/data/:device_id", handler.PostData)

	body := bytes.NewBufferString(`not-json`)
	req, _ := http.NewRequest("POST", "/data/dev1", body)
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}
