package auth

import (
	"net/http/httptest"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
)

func TestDeviceAuthMiddlewareValidAPIKey(t *testing.T) {
	gin.SetMode(gin.TestMode)

	r := gin.New()
	r.Use(DeviceAuthMiddleware())
	r.GET("/test", func(c *gin.Context) {
		apiKey, _ := c.Get("api_key")
		c.JSON(200, gin.H{"api_key": apiKey})
	})

	req := httptest.NewRequest("GET", "/test", nil)
	req.Header.Set("Authorization", "Bearer dk_test_api_key_123")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, 200, w.Code)
	assert.Contains(t, w.Body.String(), "dk_test_api_key_123")
}

func TestDeviceAuthMiddlewareQueryParameter(t *testing.T) {
	gin.SetMode(gin.TestMode)

	r := gin.New()
	r.Use(DeviceAuthMiddleware())
	r.GET("/test", func(c *gin.Context) {
		apiKey, _ := c.Get("api_key")
		c.JSON(200, gin.H{"api_key": apiKey})
	})

	req := httptest.NewRequest("GET", "/test?key=dk_query_key", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, 200, w.Code)
	assert.Contains(t, w.Body.String(), "dk_query_key")
}

func TestDeviceAuthMiddlewareMissingAPIKey(t *testing.T) {
	gin.SetMode(gin.TestMode)

	r := gin.New()
	r.Use(DeviceAuthMiddleware())
	r.GET("/test", func(c *gin.Context) {
		c.JSON(200, gin.H{"status": "ok"})
	})

	req := httptest.NewRequest("GET", "/test", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, 401, w.Code)
	assert.Contains(t, w.Body.String(), "API key required")
}

func TestDeviceAuthMiddlewareInvalidFormat(t *testing.T) {
	gin.SetMode(gin.TestMode)

	r := gin.New()
	r.Use(DeviceAuthMiddleware())
	r.GET("/test", func(c *gin.Context) {
		c.JSON(200, gin.H{"status": "ok"})
	})

	// Missing "sk_live_" prefix
	req := httptest.NewRequest("GET", "/test", nil)
	req.Header.Set("Authorization", "Bearer invalid_key_format")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, 401, w.Code)
	assert.Contains(t, w.Body.String(), "Invalid API key format")
}

func TestDeviceAuthMiddlewareInvalidAuthFormat(t *testing.T) {
	gin.SetMode(gin.TestMode)

	r := gin.New()
	r.Use(DeviceAuthMiddleware())
	r.GET("/test", func(c *gin.Context) {
		c.JSON(200, gin.H{"status": "ok"})
	})

	// Not in "Bearer <key>" format
	req := httptest.NewRequest("GET", "/test", nil)
	req.Header.Set("Authorization", "sk_live_key_without_bearer")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, 401, w.Code)
	assert.Contains(t, w.Body.String(), "Invalid authorization format")
}

func TestGetUserIDSuccess(t *testing.T) {
	gin.SetMode(gin.TestMode)

	c, _ := gin.CreateTestContext(httptest.NewRecorder())
	c.Set("user_id", "user123")

	userID, err := GetUserID(c)
	assert.NoError(t, err)
	assert.Equal(t, "user123", userID)
}

func TestGetUserIDNotFound(t *testing.T) {
	gin.SetMode(gin.TestMode)

	c, _ := gin.CreateTestContext(httptest.NewRecorder())

	userID, err := GetUserID(c)
	assert.Error(t, err)
	assert.Equal(t, "", userID)
	assert.Contains(t, err.Error(), "user_id not found in context")
}

func TestGenerateAPIKey(t *testing.T) {
	apiKey, err := GenerateAPIKey()
	assert.NoError(t, err)
	assert.NotEmpty(t, apiKey)
	assert.True(t, len(apiKey) > 10, "API key should be long enough")
	assert.Contains(t, apiKey, "sk_", "API key should have sk_ prefix")
}

func TestGenerateAPIKeyUnique(t *testing.T) {
	keys := make(map[string]bool)

	// Generate 100 keys and ensure they're all unique
	for i := 0; i < 100; i++ {
		key, err := GenerateAPIKey()
		assert.NoError(t, err)
		assert.False(t, keys[key], "Generated duplicate API key")
		keys[key] = true
	}

	assert.Equal(t, 100, len(keys))
}
