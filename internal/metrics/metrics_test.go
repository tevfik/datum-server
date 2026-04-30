package metrics

import (
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
)

func TestMetricsIncrement(t *testing.T) {
	m := GetGlobalMetrics()
	before := m.RequestsTotal

	m.IncrementRequests()
	if m.RequestsTotal != before+1 {
		t.Fatal("IncrementRequests did not increment")
	}

	m.IncrementSuccess()
	m.IncrementError()
	m.IncrementDataPoints(10)
	m.IncrementCommands()
	m.IncrementDropped(5)

	if m.DataPointsReceived < 10 {
		t.Fatal("IncrementDataPoints not working")
	}
	if m.DataPointsDropped < 5 {
		t.Fatal("IncrementDropped not working")
	}
}

func TestMiddleware(t *testing.T) {
	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.Use(Middleware())
	r.GET("/test", func(c *gin.Context) {
		c.String(200, "ok")
	})

	before := globalMetrics.RequestsTotal
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("GET", "/test", nil)
	r.ServeHTTP(w, req)

	if w.Code != 200 {
		t.Fatalf("expected 200, got %d", w.Code)
	}
	if globalMetrics.RequestsTotal <= before {
		t.Fatal("middleware should have incremented request count")
	}
}

func TestHandlerJSON(t *testing.T) {
	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.GET("/sys/metrics", Handler)

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("GET", "/sys/metrics", nil)
	r.ServeHTTP(w, req)

	if w.Code != 200 {
		t.Fatalf("expected 200, got %d", w.Code)
	}
	if !strings.Contains(w.Body.String(), "uptime_seconds") {
		t.Fatal("response should contain uptime_seconds")
	}
}

func TestHandlerPrometheus(t *testing.T) {
	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.GET("/sys/metrics", Handler)

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("GET", "/sys/metrics?format=prometheus", nil)
	r.ServeHTTP(w, req)

	if w.Code != 200 {
		t.Fatalf("expected 200, got %d", w.Code)
	}
	body := w.Body.String()
	if !strings.Contains(body, "datum_requests_total") {
		t.Fatal("response should contain datum_requests_total")
	}
	if !strings.Contains(body, "datum_uptime_seconds") {
		t.Fatal("response should contain datum_uptime_seconds")
	}
}
