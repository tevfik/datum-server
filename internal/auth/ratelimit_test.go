package auth

import (
	"net/http"
	"net/http/httptest"
	"os"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
)

func TestNewRateLimiterDefault(t *testing.T) {
	os.Unsetenv("RATE_LIMIT_REQUESTS")
	os.Unsetenv("RATE_LIMIT_WINDOW_SECONDS")

	limiter := NewRateLimiter()

	assert.NotNil(t, limiter)
	assert.Equal(t, 100, limiter.rate)
	assert.Equal(t, 60*time.Second, limiter.window)
	assert.NotNil(t, limiter.visitors)
}

func TestNewRateLimiterCustom(t *testing.T) {
	os.Setenv("RATE_LIMIT_REQUESTS", "50")
	os.Setenv("RATE_LIMIT_WINDOW_SECONDS", "30")
	defer os.Unsetenv("RATE_LIMIT_REQUESTS")
	defer os.Unsetenv("RATE_LIMIT_WINDOW_SECONDS")

	limiter := NewRateLimiter()

	assert.Equal(t, 50, limiter.rate)
	assert.Equal(t, 30*time.Second, limiter.window)
}

func TestNewRateLimiterInvalidValues(t *testing.T) {
	os.Setenv("RATE_LIMIT_REQUESTS", "invalid")
	os.Setenv("RATE_LIMIT_WINDOW_SECONDS", "invalid")
	defer os.Unsetenv("RATE_LIMIT_REQUESTS")
	defer os.Unsetenv("RATE_LIMIT_WINDOW_SECONDS")

	limiter := NewRateLimiter()

	// Should use defaults for invalid values
	assert.Equal(t, 100, limiter.rate)
	assert.Equal(t, 60*time.Second, limiter.window)
}

func TestTokenBucketAllow(t *testing.T) {
	bucket := newTokenBucket(3, time.Minute)

	// Should allow 3 requests
	assert.True(t, bucket.allow())
	assert.True(t, bucket.allow())
	assert.True(t, bucket.allow())

	// Fourth request should be denied
	assert.False(t, bucket.allow())
}

func TestTokenBucketRefill(t *testing.T) {
	bucket := newTokenBucket(2, 100*time.Millisecond)

	// Use up tokens
	assert.True(t, bucket.allow())
	assert.True(t, bucket.allow())
	assert.False(t, bucket.allow())

	// Wait for refill
	time.Sleep(150 * time.Millisecond)

	// Should allow again
	assert.True(t, bucket.allow())
	assert.True(t, bucket.allow())
}

func TestGetVisitor(t *testing.T) {
	limiter := NewRateLimiter()

	// First call creates visitor
	v1 := limiter.getVisitor("192.168.1.1")
	assert.NotNil(t, v1)
	assert.NotNil(t, v1.limiter)

	// Second call returns same visitor
	v2 := limiter.getVisitor("192.168.1.1")
	assert.Equal(t, v1, v2)

	// Different IP creates different visitor
	v3 := limiter.getVisitor("192.168.1.2")
	assert.NotEqual(t, v1, v3)
}

func TestCleanup(t *testing.T) {
	limiter := NewRateLimiter()

	// Add old visitor
	limiter.visitors["old-ip"] = &visitor{
		limiter:  newTokenBucket(100, time.Minute),
		lastSeen: time.Now().Add(-15 * time.Minute), // 15 minutes ago
	}

	// Add recent visitor
	limiter.visitors["recent-ip"] = &visitor{
		limiter:  newTokenBucket(100, time.Minute),
		lastSeen: time.Now(),
	}

	assert.Len(t, limiter.visitors, 2)

	limiter.cleanup()

	// Old visitor should be removed, recent should remain
	assert.Len(t, limiter.visitors, 1)
	assert.Contains(t, limiter.visitors, "recent-ip")
	assert.NotContains(t, limiter.visitors, "old-ip")
}

func TestRateLimitMiddlewareAllow(t *testing.T) {
	gin.SetMode(gin.TestMode)

	os.Setenv("RATE_LIMIT_REQUESTS", "5")
	defer os.Unsetenv("RATE_LIMIT_REQUESTS")

	router := gin.New()
	router.Use(RateLimitMiddleware())
	router.GET("/test", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{"message": "ok"})
	})

	// Make 5 requests - all should succeed
	for i := 0; i < 5; i++ {
		req := httptest.NewRequest(http.MethodGet, "/test", nil)
		req.RemoteAddr = "192.168.1.1:1234"
		w := httptest.NewRecorder()
		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)
	}
}

func TestRateLimitMiddlewareBlock(t *testing.T) {
	gin.SetMode(gin.TestMode)

	os.Setenv("RATE_LIMIT_REQUESTS", "3")
	defer os.Unsetenv("RATE_LIMIT_REQUESTS")

	router := gin.New()
	router.Use(RateLimitMiddleware())
	router.GET("/test", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{"message": "ok"})
	})

	// Make 3 requests - should succeed
	for i := 0; i < 3; i++ {
		req := httptest.NewRequest(http.MethodGet, "/test", nil)
		req.RemoteAddr = "192.168.1.1:1234"
		w := httptest.NewRecorder()
		router.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)
	}

	// 4th request should be blocked
	req := httptest.NewRequest(http.MethodGet, "/test", nil)
	req.RemoteAddr = "192.168.1.1:1234"
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusTooManyRequests, w.Code)
	assert.Contains(t, w.Body.String(), "Rate limit exceeded")
}

func TestRateLimitMiddlewareDifferentIPs(t *testing.T) {
	gin.SetMode(gin.TestMode)

	os.Setenv("RATE_LIMIT_REQUESTS", "2")
	defer os.Unsetenv("RATE_LIMIT_REQUESTS")

	router := gin.New()
	router.Use(RateLimitMiddleware())
	router.GET("/test", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{"message": "ok"})
	})

	// IP 1: use up limit
	for i := 0; i < 2; i++ {
		req := httptest.NewRequest(http.MethodGet, "/test", nil)
		req.RemoteAddr = "192.168.1.1:1234"
		w := httptest.NewRecorder()
		router.ServeHTTP(w, req)
		assert.Equal(t, http.StatusOK, w.Code)
	}

	// IP 1: should be blocked
	req1 := httptest.NewRequest(http.MethodGet, "/test", nil)
	req1.RemoteAddr = "192.168.1.1:1234"
	w1 := httptest.NewRecorder()
	router.ServeHTTP(w1, req1)
	assert.Equal(t, http.StatusTooManyRequests, w1.Code)

	// IP 2: should still work (different limit)
	req2 := httptest.NewRequest(http.MethodGet, "/test", nil)
	req2.RemoteAddr = "192.168.1.2:1234"
	w2 := httptest.NewRecorder()
	router.ServeHTTP(w2, req2)
	assert.Equal(t, http.StatusOK, w2.Code)
}
