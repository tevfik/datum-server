package auth

import (
	"net/http/httptest"
	"path/filepath"
	"testing"
	"time"

	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// Helper to create test storage
func createTestStorageForAuth(t *testing.T) (*storage.Storage, func()) {
	tmpDir := t.TempDir()
	metaPath := filepath.Join(tmpDir, "auth_test.db")
	dataPath := filepath.Join(tmpDir, "auth_data")

	store, err := storage.New(metaPath, dataPath, 7*24*time.Hour)
	require.NoError(t, err)

	cleanup := func() {
		store.Close()
	}

	return store, cleanup
}

func TestAdminMiddlewareSuccess(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store, cleanup := createTestStorageForAuth(t)
	defer cleanup()

	// Create admin user
	adminUser := &storage.User{
		ID:           "admin123",
		Email:        "admin@test.com",
		PasswordHash: "hash",
		Role:         "admin",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := store.CreateUser(adminUser)
	require.NoError(t, err)

	// Create admin token
	token, err := GenerateToken("admin123", "admin@test.com", "admin")
	require.NoError(t, err)

	r := gin.New()
	r.Use(AdminMiddleware(store))
	r.GET("/admin/test", func(c *gin.Context) {
		c.JSON(200, gin.H{"status": "ok"})
	})

	req := httptest.NewRequest("GET", "/admin/test", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, 200, w.Code)
}

func TestAdminMiddlewareNotAdmin(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store, cleanup := createTestStorageForAuth(t)
	defer cleanup()

	user := &storage.User{
		ID:           "user123",
		Email:        "user@test.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(user)

	token, _ := GenerateToken("user123", "user@test.com", "user")

	r := gin.New()
	r.Use(AdminMiddleware(store))
	r.GET("/admin/test", func(c *gin.Context) {
		c.JSON(200, gin.H{"status": "ok"})
	})

	req := httptest.NewRequest("GET", "/admin/test", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, 403, w.Code)
	assert.Contains(t, w.Body.String(), "Admin access required")
}

func TestAdminMiddlewareSuspendedUser(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store, cleanup := createTestStorageForAuth(t)
	defer cleanup()

	adminUser := &storage.User{
		ID:           "admin123",
		Email:        "admin@test.com",
		PasswordHash: "hash",
		Role:         "admin",
		Status:       "suspended",
		CreatedAt:    time.Now(),
	}
	store.CreateUser(adminUser)

	token, _ := GenerateToken("admin123", "admin@test.com", "admin")

	r := gin.New()
	r.Use(AdminMiddleware(store))
	r.GET("/admin/test", func(c *gin.Context) {
		c.JSON(200, gin.H{"status": "ok"})
	})

	req := httptest.NewRequest("GET", "/admin/test", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, 403, w.Code)
	assert.Contains(t, w.Body.String(), "suspended")
}

func TestAdminMiddlewareUserNotFound(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store, cleanup := createTestStorageForAuth(t)
	defer cleanup()

	token, _ := GenerateToken("nonexistent", "nonexistent@test.com", "user")

	r := gin.New()
	r.Use(AdminMiddleware(store))
	r.GET("/admin/test", func(c *gin.Context) {
		c.JSON(200, gin.H{"status": "ok"})
	})

	req := httptest.NewRequest("GET", "/admin/test", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, 401, w.Code)
	assert.Contains(t, w.Body.String(), "User not found")
}
