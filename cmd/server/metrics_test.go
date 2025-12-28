package main

import (
	"datum-go/internal/storage"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func setupMetricsTestServer(t *testing.T) (*gin.Engine, func()) {
	gin.SetMode(gin.TestMode)

	tmpDir := t.TempDir()
	var err error
	store, err = storage.New(tmpDir+"/meta.db", tmpDir+"/tsdata", 7*24*time.Hour)
	require.NoError(t, err)

	// Reset metrics for testing
	metrics = &Metrics{
		StartTime: metrics.StartTime,
	}

	router := gin.New()
	cleanup := func() {
		store.Close()
	}

	return router, cleanup
}

func TestMetricsMiddleware(t *testing.T) {
	router, cleanup := setupMetricsTestServer(t)
	defer cleanup()

	router.Use(metricsMiddleware())
	router.GET("/test-success", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{"status": "ok"})
	})
	router.GET("/test-error", func(c *gin.Context) {
		c.JSON(http.StatusBadRequest, gin.H{"error": "bad request"})
	})

	// Test successful request
	req := httptest.NewRequest(http.MethodGet, "/test-success", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	assert.Greater(t, metrics.RequestsTotal, uint64(0))
	assert.Greater(t, metrics.RequestsSuccess, uint64(0))

	// Test error request
	req = httptest.NewRequest(http.MethodGet, "/test-error", nil)
	w = httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
	assert.Greater(t, metrics.RequestsError, uint64(0))
}

func TestMetricsIncrement(t *testing.T) {
	m := &Metrics{}

	// Test IncrementRequests
	m.IncrementRequests()
	assert.Equal(t, uint64(1), m.RequestsTotal)

	m.IncrementRequests()
	assert.Equal(t, uint64(2), m.RequestsTotal)

	// Test IncrementSuccess
	m.IncrementSuccess()
	assert.Equal(t, uint64(1), m.RequestsSuccess)

	// Test IncrementError
	m.IncrementError()
	assert.Equal(t, uint64(1), m.RequestsError)

	// Test IncrementDataPoints
	m.IncrementDataPoints(5)
	assert.Equal(t, uint64(5), m.DataPointsReceived)

	m.IncrementDataPoints(3)
	assert.Equal(t, uint64(8), m.DataPointsReceived)

	// Test IncrementCommands
	m.IncrementCommands()
	assert.Equal(t, uint64(1), m.CommandsSent)
}
