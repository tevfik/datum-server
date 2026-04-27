package system

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"runtime"
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
