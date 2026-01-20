package auth

import (
	"errors"
	"net/http"
	"net/http/httptest"
	"testing"

	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
)

// MockStore implements storage.Provider partially for testing
type MockStore struct {
	storage.Provider // Embed interface to satisfy it (panic on unused)
}

func (m *MockStore) GetUserByUserAPIKey(apiKey string) (*storage.User, error) {
	if apiKey == "ak_valid_key" {
		return &storage.User{
			ID:    "user_123",
			Email: "mock@example.com",
			Role:  "admin",
		}, nil
	}
	return nil, errors.New("key not found")
}

// Implement cleanup to avoid panic if middleware calls it? Start calls close? No.
func (m *MockStore) Close() error { return nil }

func TestUserAuthMiddleware_APIKey_Success(t *testing.T) {
	gin.SetMode(gin.TestMode)
	mockStore := &MockStore{}

	r := gin.New()
	r.Use(UserAuthMiddleware(mockStore))
	r.GET("/protected", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{
			"user_id": c.GetString("user_id"),
			"role":    c.GetString("role"),
			"method":  c.GetString("auth_method"),
		})
	})

	req := httptest.NewRequest(http.MethodGet, "/protected", nil)
	req.Header.Set("Authorization", "Bearer ak_valid_key")
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	assert.Contains(t, w.Body.String(), "user_123")
	assert.Contains(t, w.Body.String(), "api_key")
}

func TestUserAuthMiddleware_APIKey_Invalid(t *testing.T) {
	gin.SetMode(gin.TestMode)
	mockStore := &MockStore{}

	r := gin.New()
	r.Use(UserAuthMiddleware(mockStore))
	r.GET("/protected", func(c *gin.Context) {
		c.Status(http.StatusOK)
	})

	req := httptest.NewRequest(http.MethodGet, "/protected", nil)
	req.Header.Set("Authorization", "Bearer ak_INVALID")
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func TestUserAuthMiddleware_JWT_Success(t *testing.T) {
	// Re-verify JWT path still works
	gin.SetMode(gin.TestMode)
	mockStore := &MockStore{}

	r := gin.New()
	r.Use(UserAuthMiddleware(mockStore))
	r.GET("/protected", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{"user_id": c.GetString("user_id")})
	})

	token, _ := GenerateToken("uJWT", "jwt@test.com", "user")
	req := httptest.NewRequest(http.MethodGet, "/protected", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	assert.Contains(t, w.Body.String(), "uJWT")
}
