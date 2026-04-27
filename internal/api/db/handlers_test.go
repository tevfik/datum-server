package db

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

func userMiddleware(userID string) gin.HandlerFunc {
	return func(c *gin.Context) {
		c.Set("userID", userID)
		c.Set("user_id", userID)
		c.Next()
	}
}

// ---------- CreateDoc ----------

func TestCreateDoc_Success(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.Use(userMiddleware("user1"))
	r.POST("/db/:collection", handler.CreateDoc)

	body, _ := json.Marshal(map[string]interface{}{
		"title":   "Hello",
		"content": "World",
	})
	req, _ := http.NewRequest("POST", "/db/notes", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	assert.NotEmpty(t, resp["id"])
	assert.Equal(t, "created", resp["status"])
}

func TestCreateDoc_InvalidJSON(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.Use(userMiddleware("user1"))
	r.POST("/db/:collection", handler.CreateDoc)

	req, _ := http.NewRequest("POST", "/db/notes", bytes.NewBufferString("not-json"))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

// ---------- ListDocs ----------

func TestListDocs_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	// Create some documents
	store.CreateDocument("user1", "notes", map[string]interface{}{"title": "Note 1"})
	store.CreateDocument("user1", "notes", map[string]interface{}{"title": "Note 2"})

	r := gin.New()
	r.Use(userMiddleware("user1"))
	r.GET("/db/:collection", handler.ListDocs)

	req, _ := http.NewRequest("GET", "/db/notes", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var docs []map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &docs))
	assert.Len(t, docs, 2)
}

func TestListDocs_Empty(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.Use(userMiddleware("user1"))
	r.GET("/db/:collection", handler.ListDocs)

	req, _ := http.NewRequest("GET", "/db/empty_collection", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var docs []map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &docs))
	assert.Empty(t, docs)
}

// ---------- GetDoc ----------

func TestGetDoc_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	docID, err := store.CreateDocument("user1", "notes", map[string]interface{}{"title": "Found Me"})
	require.NoError(t, err)

	r := gin.New()
	r.Use(userMiddleware("user1"))
	r.GET("/db/:collection/:id", handler.GetDoc)

	req, _ := http.NewRequest("GET", "/db/notes/"+docID, nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var doc map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &doc))
	assert.Equal(t, "Found Me", doc["title"])
}

func TestGetDoc_NotFound(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.Use(userMiddleware("user1"))
	r.GET("/db/:collection/:id", handler.GetDoc)

	req, _ := http.NewRequest("GET", "/db/notes/nonexistent", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

// ---------- UpdateDoc ----------

func TestUpdateDoc_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	docID, err := store.CreateDocument("user1", "notes", map[string]interface{}{"title": "Original"})
	require.NoError(t, err)

	r := gin.New()
	r.Use(userMiddleware("user1"))
	r.PUT("/db/:collection/:id", handler.UpdateDoc)

	body, _ := json.Marshal(map[string]interface{}{"title": "Updated"})
	req, _ := http.NewRequest("PUT", "/db/notes/"+docID, bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify update
	doc, _ := store.GetDocument("user1", "notes", docID)
	assert.Equal(t, "Updated", doc["title"])
}

// ---------- DeleteDoc ----------

func TestDeleteDoc_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	docID, err := store.CreateDocument("user1", "notes", map[string]interface{}{"title": "Delete Me"})
	require.NoError(t, err)

	r := gin.New()
	r.Use(userMiddleware("user1"))
	r.DELETE("/db/:collection/:id", handler.DeleteDoc)

	req, _ := http.NewRequest("DELETE", "/db/notes/"+docID, nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify deleted
	_, err = store.GetDocument("user1", "notes", docID)
	assert.Error(t, err)
}
