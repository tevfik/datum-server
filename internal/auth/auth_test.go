package auth

import (
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/golang-jwt/jwt/v5"
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
	role := "user"

	token, err := GenerateToken(userID, email, role)

	assert.NoError(t, err)
	assert.NotEmpty(t, token)

	// validate content immediately to be sure
	claims, err := ValidateToken(token)
	assert.NoError(t, err)
	assert.Equal(t, userID, claims.UserID)
	assert.Equal(t, email, claims.Email)
	assert.Equal(t, role, claims.Role)
}

func TestGenerateTokenPair(t *testing.T) {
	userID := "usr_456"
	email := "pair@example.com"
	role := "admin"

	accessToken, refreshToken, err := GenerateTokenPair(userID, email, role)

	assert.NoError(t, err)
	assert.NotEmpty(t, accessToken)
	assert.NotEmpty(t, refreshToken)

	// Validate Access Token
	aClaims, err := ValidateToken(accessToken)
	assert.NoError(t, err)
	assert.Equal(t, userID, aClaims.UserID)
	assert.Equal(t, role, aClaims.Role)

	// Validate Refresh Token
	rClaims, err := ValidateToken(refreshToken)
	assert.NoError(t, err)
	assert.Equal(t, userID, rClaims.UserID)
	assert.Equal(t, "refresh", rClaims.Subject)
}

func TestValidateToken(t *testing.T) {
	userID := "usr_789"
	email := "valid@example.com"
	role := "user"

	token, _ := GenerateToken(userID, email, role)

	// Test valid token
	claims, err := ValidateToken(token)
	assert.NoError(t, err)
	assert.Equal(t, userID, claims.UserID)
	assert.Equal(t, email, claims.Email)
	assert.Equal(t, role, claims.Role)

	// Test invalid token string
	_, err = ValidateToken("invalid.token.here")
	assert.Error(t, err)
	// assert.Equal(t, ErrInvalidToken, err) // jwt library returns specific processing errors

	// Test expired token (manually create one)
	expiredClaims := Claims{
		UserID: userID,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(-1 * time.Hour)), // Expired 1 hour ago
		},
	}
	expiredToken := jwt.NewWithClaims(jwt.SigningMethodHS256, expiredClaims)
	expiredTokenString, _ := expiredToken.SignedString(jwtSecret)

	_, err = ValidateToken(expiredTokenString)
	assert.Error(t, err)
	// v5 returns specific error types, but our wrapper might map them or return generic error
}

func TestGenerateUserAPIKey(t *testing.T) {
	key, err := GenerateUserAPIKey()
	assert.NoError(t, err)
	assert.NotEmpty(t, key)
	assert.True(t, strings.HasPrefix(key, "ak_"), "User API key should start with ak_")
	assert.Equal(t, 35, len(key)) // ak_ + 32 hex chars = 3+32 = 35
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
