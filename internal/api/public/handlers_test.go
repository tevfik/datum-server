package public

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

	handler := NewHandler(store)
	return handler, store, func() { store.Close() }
}

// ---------- PostPublicData ----------

func TestPostPublicData_Success(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.POST("/pub/:device_id", handler.PostPublicData)

	body, _ := json.Marshal(map[string]interface{}{"temp": 22.5, "humidity": 60})
	req, _ := http.NewRequest("POST", "/pub/my_sensor", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	assert.Equal(t, "ok", resp["status"])
	assert.Equal(t, "public", resp["mode"])
}

func TestPostPublicData_InvalidJSON(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.POST("/pub/:device_id", handler.PostPublicData)

	req, _ := http.NewRequest("POST", "/pub/my_sensor", bytes.NewBufferString("invalid"))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

// ---------- GetPublicData ----------

func TestGetPublicData_WithData(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	// Initialize system to enable public data
	require.NoError(t, store.InitializeSystem("test", true, 7))

	// Store public data
	require.NoError(t, store.StoreData(&storage.DataPoint{
		DeviceID:  "public_my_sensor",
		Timestamp: time.Now(),
		Data:      map[string]interface{}{"temp": 23.0},
	}))

	r := gin.New()
	r.GET("/pub/:device_id", handler.GetPublicData)

	req, _ := http.NewRequest("GET", "/pub/my_sensor", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestGetPublicData_NoData(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	require.NoError(t, store.InitializeSystem("test", true, 7))

	r := gin.New()
	r.GET("/pub/:device_id", handler.GetPublicData)

	req, _ := http.NewRequest("GET", "/pub/no_data_sensor", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	// Should return 404 or empty data
	assert.Contains(t, []int{http.StatusOK, http.StatusNotFound}, w.Code)
}
