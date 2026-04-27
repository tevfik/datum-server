package keys

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"testing"
	"time"

	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func setupTestEnv(t *testing.T) (*Handler, storage.Provider, func()) {
	gin.SetMode(gin.TestMode)
	tmpDir := t.TempDir()
	store, err := storage.New(
		filepath.Join(tmpDir, "meta.db"),
		filepath.Join(tmpDir, "tsdata"),
		24*time.Hour,
	)
	require.NoError(t, err)

	handler := NewHandler(store)
	return handler, store, func() { store.Close() }
}

// ---------- CreateKeyHandler ----------

func TestCreateKey_Success(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "user1")
		c.Next()
	})
	r.POST("/auth/keys", handler.CreateKeyHandler)

	body, _ := json.Marshal(CreateKeyRequest{Name: "My API Key"})
	req, _ := http.NewRequest("POST", "/auth/keys", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var resp KeyResponse
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	assert.Equal(t, "My API Key", resp.Name)
	assert.NotEmpty(t, resp.Key)
	assert.Contains(t, resp.Key, "ak_")
}

func TestCreateKey_MissingName(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "user1")
		c.Next()
	})
	r.POST("/auth/keys", handler.CreateKeyHandler)

	body, _ := json.Marshal(map[string]string{})
	req, _ := http.NewRequest("POST", "/auth/keys", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestCreateKey_Unauthorized(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	// No user_id set
	r.POST("/auth/keys", handler.CreateKeyHandler)

	body, _ := json.Marshal(CreateKeyRequest{Name: "test"})
	req, _ := http.NewRequest("POST", "/auth/keys", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

// ---------- ListKeysHandler ----------

func TestListKeys_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	// Create some keys directly
	require.NoError(t, store.CreateUserAPIKey(&storage.APIKey{
		ID:        "key1",
		UserID:    "user1",
		Name:      "Key 1",
		Key:       "ak_testkey1234567890abcdef",
		CreatedAt: time.Now(),
	}))
	require.NoError(t, store.CreateUserAPIKey(&storage.APIKey{
		ID:        "key2",
		UserID:    "user1",
		Name:      "Key 2",
		Key:       "ak_testkey2234567890abcdef",
		CreatedAt: time.Now(),
	}))

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "user1")
		c.Next()
	})
	r.GET("/auth/keys", handler.ListKeysHandler)

	req, _ := http.NewRequest("GET", "/auth/keys", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	keys := resp["keys"].([]interface{})
	assert.Len(t, keys, 2)
	// Keys should be masked
	for _, k := range keys {
		km := k.(map[string]interface{})
		assert.Contains(t, km["key"].(string), "ak_****")
	}
}

func TestListKeys_Empty(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "user_no_keys")
		c.Next()
	})
	r.GET("/auth/keys", handler.ListKeysHandler)

	req, _ := http.NewRequest("GET", "/auth/keys", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

// ---------- DeleteKeyHandler ----------

func TestDeleteKey_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	require.NoError(t, store.CreateUserAPIKey(&storage.APIKey{
		ID:        "key_del",
		UserID:    "user1",
		Name:      "To Delete",
		Key:       "ak_deletekey567890abcdefgh",
		CreatedAt: time.Now(),
	}))

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "user1")
		c.Next()
	})
	r.DELETE("/auth/keys/:id", handler.DeleteKeyHandler)

	req, _ := http.NewRequest("DELETE", "/auth/keys/key_del", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify key is gone
	keys, _ := store.GetUserAPIKeys("user1")
	assert.Empty(t, keys)
}
