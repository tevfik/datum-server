package main

import (
	"bytes"
	"datum-go/internal/storage"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// setupKeyTestServer creates a test server with storage initialized for key tests
func setupKeyTestServer(t *testing.T) (*gin.Engine, func()) {
	gin.SetMode(gin.TestMode)

	// Create temporary storage
	tmpDir := t.TempDir()
	var err error
	store, err = storage.New(tmpDir+"/meta.db", tmpDir+"/tsdata", 7*24*time.Hour)
	require.NoError(t, err)

	// Create test user
	testUser := &storage.User{
		ID:           "test-user-id",
		Email:        "keytest@example.com",
		PasswordHash: "hashed_password",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err = store.CreateUser(testUser)
	require.NoError(t, err)

	router := gin.New()

	cleanup := func() {
		store.Close()
	}

	return router, cleanup
}

func TestCreateKeyHandler_Success(t *testing.T) {
	router, cleanup := setupKeyTestServer(t)
	defer cleanup()

	router.POST("/auth/keys", func(c *gin.Context) {
		c.Set("user_id", "test-user-id")
		createKeyHandler(c)
	})

	body := map[string]string{
		"name": "My Test Key",
	}
	bodyBytes, _ := json.Marshal(body)

	req := httptest.NewRequest(http.MethodPost, "/auth/keys", bytes.NewReader(bodyBytes))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var response KeyResponse
	err := json.Unmarshal(w.Body.Bytes(), &response)
	require.NoError(t, err)

	assert.NotEmpty(t, response.ID)
	assert.Equal(t, "My Test Key", response.Name)
	assert.NotEmpty(t, response.Key)
	assert.Contains(t, response.Key, "ak_")
}

func TestListKeysHandler_Success(t *testing.T) {
	router, cleanup := setupKeyTestServer(t)
	defer cleanup()

	// Create some keys first
	apiKey1 := &storage.APIKey{
		ID:        "key-1",
		UserID:    "test-user-id",
		Name:      "Key 1",
		Key:       "ak_1234567890abcdef1234567890abcdef",
		CreatedAt: time.Now(),
	}
	store.CreateUserAPIKey(apiKey1)

	router.GET("/auth/keys", func(c *gin.Context) {
		c.Set("user_id", "test-user-id")
		listKeysHandler(c)
	})

	req := httptest.NewRequest(http.MethodGet, "/auth/keys", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string][]KeyResponse
	err := json.Unmarshal(w.Body.Bytes(), &response)
	require.NoError(t, err)

	keys := response["keys"]
	assert.Len(t, keys, 1)
	assert.Equal(t, "Key 1", keys[0].Name)
	assert.Equal(t, "key-1", keys[0].ID)
	assert.NotEqual(t, "ak_1234567890abcdef1234567890abcdef", keys[0].Key) // Should be masked
	assert.Contains(t, keys[0].Key, "******")
}

func TestDeleteKeyHandler_Success(t *testing.T) {
	router, cleanup := setupKeyTestServer(t)
	defer cleanup()

	// Create a key
	apiKey := &storage.APIKey{
		ID:        "key-to-delete",
		UserID:    "test-user-id",
		Name:      "Delete Me",
		Key:       "ak_delete",
		CreatedAt: time.Now(),
	}
	store.CreateUserAPIKey(apiKey)

	router.DELETE("/auth/keys/:id", func(c *gin.Context) {
		c.Set("user_id", "test-user-id")
		deleteKeyHandler(c)
	})

	req := httptest.NewRequest(http.MethodDelete, "/auth/keys/key-to-delete", nil)
	w := httptest.NewRecorder()
	router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify deletion
	keys, _ := store.GetUserAPIKeys("test-user-id")
	assert.Len(t, keys, 0)
}
