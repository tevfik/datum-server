package auth

import (
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
)

func TestHashPassword(t *testing.T) {
	password := "testpassword123"
	hash, err := HashPassword(password)

	assert.NoError(t, err)
	assert.NotEmpty(t, hash)
	assert.NotEqual(t, password, hash)
}

func TestCheckPassword(t *testing.T) {
	password := "testpassword123"
	hash, _ := HashPassword(password)

	// Test correct password
	ok := CheckPassword(hash, password)
	assert.True(t, ok)

	// Test incorrect password
	ok = CheckPassword(hash, "wrongpassword")
	assert.False(t, ok)
}

func TestGenerateToken(t *testing.T) {
	userID := "usr_123"
	email := "test@example.com"

	token, err := GenerateToken(userID, email)

	assert.NoError(t, err)
	assert.NotEmpty(t, token)
}

func TestValidateToken(t *testing.T) {
	userID := "usr_123"
	email := "test@example.com"

	token, _ := GenerateToken(userID, email)

	// Test valid token
	claims, err := ValidateToken(token)
	assert.NoError(t, err)
	assert.Equal(t, userID, claims.UserID)
	assert.Equal(t, email, claims.Email)

	// Test invalid token
	_, err = ValidateToken("invalid.token.here")
	assert.Error(t, err)
}

func TestRateLimitMiddleware(t *testing.T) {
	gin.SetMode(gin.TestMode)

	r := gin.New()
	r.Use(RateLimitMiddleware())
	r.GET("/test", func(c *gin.Context) {
		c.String(200, "OK")
	})

	// First request should succeed
	req, _ := http.NewRequest("GET", "/test", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	assert.Contains(t, w.Header().Get("X-RateLimit-Limit"), "100") // default limit
}

func TestRateLimiterTokenBucket(t *testing.T) {
	tb := newTokenBucket(5, time.Minute)

	// Should allow 5 requests
	for i := 0; i < 5; i++ {
		assert.True(t, tb.allow(), "Request %d should be allowed", i+1)
	}

	// 6th request should be denied
	assert.False(t, tb.allow(), "6th request should be denied")
}

func TestRateLimiterCleanup(t *testing.T) {
	rl := &RateLimiter{
		visitors: make(map[string]*visitor),
		rate:     100,
		window:   time.Minute,
	}

	// Add a visitor
	v := rl.getVisitor("192.168.1.1")
	assert.NotNil(t, v)
	assert.Equal(t, 1, len(rl.visitors))

	// Mark as old
	v.lastSeen = time.Now().Add(-15 * time.Minute)

	// Run cleanup
	rl.cleanup()

	// Should be removed
	assert.Equal(t, 0, len(rl.visitors))
}

func TestRateLimiterMultipleIPs(t *testing.T) {
	rl := &RateLimiter{
		visitors: make(map[string]*visitor),
		rate:     100,
		window:   time.Minute,
	}

	// Different IPs should have separate limits
	v1 := rl.getVisitor("192.168.1.1")
	v2 := rl.getVisitor("192.168.1.2")

	assert.NotNil(t, v1)
	assert.NotNil(t, v2)
	assert.Equal(t, 2, len(rl.visitors))
}
