package main

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
)

func setupTestRouter() (*gin.Engine, *storage.Storage) {
	gin.SetMode(gin.TestMode)

	// Create in-memory storage for testing
	testStore, _ := storage.New(":memory:", "", 0)

	r := gin.New()
	return r, testStore
}

func TestRootHandler(t *testing.T) {
	r, _ := setupTestRouter()
	r.GET("/", rootHandler)

	req, _ := http.NewRequest("GET", "/", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.Equal(t, "Datum IoT Platform", response["service"])
}

func TestHealthHandler(t *testing.T) {
	r, testStore := setupTestRouter()
	store = testStore // Set global store for health check
	r.GET("/health", healthHandler)

	req, _ := http.NewRequest("GET", "/health", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.Equal(t, "healthy", response["status"])
	assert.Equal(t, "connected", response["storage"])
	assert.Equal(t, Version, response["version"])
}

func TestHealthHandlerUnhealthy(t *testing.T) {
	r, _ := setupTestRouter()
	store = nil // Set store to nil to simulate disconnected storage
	r.GET("/health", healthHandler)

	req, _ := http.NewRequest("GET", "/health", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusServiceUnavailable, w.Code)

	var response map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.Equal(t, "unhealthy", response["status"])
	assert.Equal(t, "disconnected", response["storage"])
}

func TestLivenessHandler(t *testing.T) {
	r, _ := setupTestRouter()
	r.GET("/health/live", livenessHandler)

	req, _ := http.NewRequest("GET", "/health/live", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.Equal(t, true, response["alive"])
}

func TestReadinessHandler(t *testing.T) {
	r, testStore := setupTestRouter()
	store = testStore // Set global store
	r.GET("/health/ready", readinessHandler)

	req, _ := http.NewRequest("GET", "/health/ready", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.Equal(t, true, response["ready"])
}

func TestMetricsHandler(t *testing.T) {
	r, _ := setupTestRouter()
	r.GET("/metrics", metricsHandler)

	req, _ := http.NewRequest("GET", "/metrics", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.Equal(t, "datumpy-api", response["service"])
	assert.Contains(t, response, "requests")
	assert.Contains(t, response, "system")
}

func TestMetricsHandlerPrometheusFormat(t *testing.T) {
	r, _ := setupTestRouter()
	r.GET("/metrics", metricsHandler)

	req, _ := http.NewRequest("GET", "/metrics?format=prometheus", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	assert.Equal(t, "text/plain", w.Header().Get("Content-Type"))
	assert.Contains(t, w.Body.String(), "datumpy_requests_total")
	assert.Contains(t, w.Body.String(), "datumpy_uptime_seconds")
}

func TestRegisterHandlerSystemNotInitialized(t *testing.T) {
	r, testStore := setupTestRouter()
	store = testStore
	r.POST("/auth/register", registerHandler)

	reqBody := RegisterRequest{
		Email:    "test@example.com",
		Password: "password123",
	}
	jsonBody, _ := json.Marshal(reqBody)

	req, _ := http.NewRequest("POST", "/auth/register", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)

	var response map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.Contains(t, response["error"], "System not initialized")
}

func TestSecurityHeadersMiddleware(t *testing.T) {
	r := gin.New()
	r.Use(securityHeadersMiddleware())
	r.GET("/test", func(c *gin.Context) {
		c.String(200, "OK")
	})

	req, _ := http.NewRequest("GET", "/test", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, "DENY", w.Header().Get("X-Frame-Options"))
	assert.Equal(t, "nosniff", w.Header().Get("X-Content-Type-Options"))
	assert.Equal(t, "1; mode=block", w.Header().Get("X-XSS-Protection"))
	assert.Equal(t, "strict-origin-when-cross-origin", w.Header().Get("Referrer-Policy"))
	assert.Contains(t, w.Header().Get("Content-Security-Policy"), "default-src 'self'")
}

func TestGenerateID(t *testing.T) {
	id1 := generateID("usr")
	id2 := generateID("dev")

	assert.NotEqual(t, id1, id2)
	assert.Contains(t, id1, "usr_")
	assert.Contains(t, id2, "dev_")
	assert.True(t, len(id1) > 4)
}
