package main

import (
	"datum-go/internal/auth"
	"datum-go/internal/storage"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// setupMiddlewareTestServer creates a simple server to test middleware
func setupMiddlewareTestServer(t *testing.T) (*gin.Engine, *storage.Storage, func()) {
	gin.SetMode(gin.TestMode)

	tmpDir := t.TempDir()
	store, err := storage.New(tmpDir+"/meta.db", tmpDir+"/tsdata", 7*24*time.Hour)
	require.NoError(t, err)

	router := gin.New()
	router.Use(gin.Recovery())

	cleanup := func() {
		store.Close()
	}

	return router, store, cleanup
}

func TestUserAuthMiddleware_Success_JWT(t *testing.T) {
	router, _, cleanup := setupMiddlewareTestServer(t)
	defer cleanup()

	// Setup protected route
	router.GET("/protected", auth.UserAuthMiddleware(store), func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{
			"user_id": c.GetString("user_id"),
			"role":    c.GetString("role"),
		})
	})

	// Generate valid token
	token, _ := auth.GenerateToken("user-123", "user@test.com", "user")

	req := httptest.NewRequest(http.MethodGet, "/protected", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// json decode
	// ... (simplified for test)
	assert.Contains(t, w.Body.String(), "user-123")
	assert.Contains(t, w.Body.String(), "user")
}

func TestUserAuthMiddleware_Success_APIKey(t *testing.T) {
	router, store, cleanup := setupMiddlewareTestServer(t)
	defer cleanup()

	// Create user and key
	user := &storage.User{ID: "user-api", Email: "api@test.com", Role: "user", Status: "active"}
	store.CreateUser(user)

	apiKey := &storage.APIKey{
		ID:     "key-1",
		UserID: "user-api",
		Key:    "ak_valid_api_key_12345",
	}
	store.CreateUserAPIKey(apiKey)

	// Setup protected route
	router.GET("/protected-key", auth.UserAuthMiddleware(store), func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{
			"user_id":     c.GetString("user_id"),
			"auth_method": c.GetString("auth_method"),
		})
	})

	req := httptest.NewRequest(http.MethodGet, "/protected-key", nil)
	req.Header.Set("Authorization", "Bearer ak_valid_api_key_12345")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	assert.Contains(t, w.Body.String(), "user-api")
	assert.Contains(t, w.Body.String(), "api_key")
}

func TestUserAuthMiddleware_MissingToken(t *testing.T) {
	router, _, cleanup := setupMiddlewareTestServer(t)
	defer cleanup()

	router.GET("/protected", auth.UserAuthMiddleware(store), func(c *gin.Context) {})

	req := httptest.NewRequest(http.MethodGet, "/protected", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func TestUserAuthMiddleware_InvalidJWT(t *testing.T) {
	router, _, cleanup := setupMiddlewareTestServer(t)
	defer cleanup()

	router.GET("/protected", auth.UserAuthMiddleware(store), func(c *gin.Context) {})

	req := httptest.NewRequest(http.MethodGet, "/protected", nil)
	req.Header.Set("Authorization", "Bearer invalid.jwt.token")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func TestUserAuthMiddleware_InvalidAPIKey(t *testing.T) {
	router, _, cleanup := setupMiddlewareTestServer(t)
	defer cleanup()

	router.GET("/protected", auth.UserAuthMiddleware(store), func(c *gin.Context) {})

	req := httptest.NewRequest(http.MethodGet, "/protected", nil)
	req.Header.Set("Authorization", "Bearer ak_invalid_key")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}
