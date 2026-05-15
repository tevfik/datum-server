package system

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"runtime"
	"strings"
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

	handler := NewHandler("1.0.0-test", "2024-01-01", store)
	return handler, store, func() { store.Close() }
}

func TestGetSystemIP(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.GET("/sys/ip", handler.GetSystemIP)

	req, _ := http.NewRequest("GET", "/sys/ip", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	// Response is the client IP (empty or loopback in tests)
}

func TestGetSystemInfo(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.GET("/sys/info", handler.GetSystemInfo)

	req, _ := http.NewRequest("GET", "/sys/info", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	assert.Equal(t, "1.0.0-test", resp["version"])
	assert.Equal(t, "2024-01-01", resp["build_date"])
	assert.Equal(t, runtime.Version(), resp["go_version"])
}

func TestGetSystemTime(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.GET("/sys/time", handler.GetSystemTime)

	req, _ := http.NewRequest("GET", "/sys/time", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	assert.NotZero(t, resp["unix"])
	assert.NotEmpty(t, resp["iso8601"])
}

func TestGetSystemConfig_Initialized(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	require.NoError(t, store.InitializeSystem("test", true, 7))

	r := gin.New()
	r.GET("/admin/sys/config", handler.GetSystemConfig)

	req, _ := http.NewRequest("GET", "/admin/sys/config", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestGetSystemConfig_NilStore(t *testing.T) {
	gin.SetMode(gin.TestMode)
	handler := &Handler{Store: nil}

	r := gin.New()
	r.GET("/admin/sys/config", handler.GetSystemConfig)

	req, _ := http.NewRequest("GET", "/admin/sys/config", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)
}

func TestUpdateRetention(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.PUT("/admin/sys/retention", handler.UpdateRetention)

	payload := `{"days": 30, "check_interval_hours": 24}`
	req, _ := http.NewRequest("PUT", "/admin/sys/retention", strings.NewReader(payload))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestUpdateRegistration(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.PUT("/admin/sys/registration", handler.UpdateRegistration)

	payload := `{"allow_register": false}`
	req, _ := http.NewRequest("PUT", "/admin/sys/registration", strings.NewReader(payload))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestUpdateRateLimit(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.PUT("/admin/sys/rate-limit", handler.UpdateRateLimit)

	payload := `{"max_requests": 100, "window_seconds": 60}`
	req, _ := http.NewRequest("PUT", "/admin/sys/rate-limit", strings.NewReader(payload))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestUpdateAlerts(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.PUT("/admin/sys/alerts", handler.UpdateAlerts)

	payload := `{"email_enabled": true, "disk_threshold": 80, "memory_threshold": 85}`
	req, _ := http.NewRequest("PUT", "/admin/sys/alerts", strings.NewReader(payload))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestGetLogs(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.GET("/admin/sys/logs", handler.GetLogs)

	req, _ := http.NewRequest("GET", "/admin/sys/logs", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestClearLogs(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.DELETE("/admin/sys/logs", handler.ClearLogs)

	req, _ := http.NewRequest("DELETE", "/admin/sys/logs", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}
