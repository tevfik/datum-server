package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/mock"
)

// MockStore for DBHandler testing
type MockDBStore struct {
	mock.Mock
}

func (m *MockDBStore) CreateDocument(userID, collection string, doc map[string]interface{}) (string, error) {
	args := m.Called(userID, collection, doc)
	return args.String(0), args.Error(1)
}

func (m *MockDBStore) ListDocuments(userID, collection string) ([]map[string]interface{}, error) {
	args := m.Called(userID, collection)
	return args.Get(0).([]map[string]interface{}), args.Error(1)
}

func (m *MockDBStore) GetDocument(userID, collection, docID string) (map[string]interface{}, error) {
	args := m.Called(userID, collection, docID)
	return args.Get(0).(map[string]interface{}), args.Error(1)
}

func (m *MockDBStore) UpdateDocument(userID, collection, docID string, doc map[string]interface{}) error {
	args := m.Called(userID, collection, docID, doc)
	return args.Error(0)
}

func (m *MockDBStore) DeleteDocument(userID, collection, docID string) error {
	args := m.Called(userID, collection, docID)
	return args.Error(0)
}

// Setup router for testing
func setupDBRouter(store *MockDBStore) *gin.Engine {
	gin.SetMode(gin.TestMode)
	r := gin.New()

	// Mock auth middleware that sets userID
	r.Use(func(c *gin.Context) {
		c.Set("userID", "test-user")
		c.Next()
	})

	RegisterDBRoutes(r.Group(""), store)
	return r
}

func TestCreateDocument(t *testing.T) {
	store := new(MockDBStore)
	r := setupDBRouter(store)

	doc := map[string]interface{}{"title": "Test Task"}
	store.On("CreateDocument", "test-user", "todos", mock.Anything).Return("doc-123", nil)

	body, _ := json.Marshal(doc)
	req, _ := http.NewRequest("POST", "/db/todos", bytes.NewBuffer(body))
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)
	assert.JSONEq(t, `{"id":"doc-123", "status":"created"}`, w.Body.String())
}

func TestListDocuments(t *testing.T) {
	store := new(MockDBStore)
	r := setupDBRouter(store)

	docs := []map[string]interface{}{
		{"id": "1", "title": "Doc 1"},
		{"id": "2", "title": "Doc 2"},
	}
	store.On("ListDocuments", "test-user", "todos").Return(docs, nil)

	req, _ := http.NewRequest("GET", "/db/todos", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response []map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Len(t, response, 2)
}

func TestGetDocument(t *testing.T) {
	store := new(MockDBStore)
	r := setupDBRouter(store)

	doc := map[string]interface{}{"id": "doc-123", "title": "My Doc"}
	store.On("GetDocument", "test-user", "todos", "doc-123").Return(doc, nil)

	req, _ := http.NewRequest("GET", "/db/todos/doc-123", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	assert.Contains(t, w.Body.String(), "My Doc")
}

func TestGetDocumentNotFound(t *testing.T) {
	store := new(MockDBStore)
	r := setupDBRouter(store)

	store.On("GetDocument", "test-user", "todos", "missing").Return(map[string]interface{}{}, errors.New("not found"))

	req, _ := http.NewRequest("GET", "/db/todos/missing", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func TestUpdateDocument(t *testing.T) {
	store := new(MockDBStore)
	r := setupDBRouter(store)

	update := map[string]interface{}{"completed": true}
	store.On("UpdateDocument", "test-user", "todos", "doc-123", mock.Anything).Return(nil)

	body, _ := json.Marshal(update)
	req, _ := http.NewRequest("PUT", "/db/todos/doc-123", bytes.NewBuffer(body))
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestDeleteDocument(t *testing.T) {
	store := new(MockDBStore)
	r := setupDBRouter(store)

	store.On("DeleteDocument", "test-user", "todos", "doc-123").Return(nil)

	req, _ := http.NewRequest("DELETE", "/db/todos/doc-123", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}
