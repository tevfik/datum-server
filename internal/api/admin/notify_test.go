package admin

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"datum-go/internal/notify"

	"github.com/stretchr/testify/assert"
)

func TestTestNotificationHandler(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	r := setupRouter(handler)
	r.POST("/notify/test", handler.TestNotificationHandler)

	t.Run("No Dispatcher", func(t *testing.T) {
		// handler.Dispatcher is nil by default in setupTestEnv
		reqBody := map[string]string{
			"user_id": "user_123",
			"message": "Hello",
		}
		body, _ := json.Marshal(reqBody)

		req, _ := http.NewRequest("POST", "/notify/test", bytes.NewBuffer(body))
		req.Header.Set("Content-Type", "application/json")
		w := httptest.NewRecorder()

		r.ServeHTTP(w, req)
		assert.Equal(t, http.StatusServiceUnavailable, w.Code)
	})

	t.Run("With Dispatcher Success", func(t *testing.T) {
		handler.Dispatcher = notify.NewDispatcher()

		reqBody := map[string]string{
			"user_id": "user_456",
			"message": "Test message",
		}
		body, _ := json.Marshal(reqBody)

		req, _ := http.NewRequest("POST", "/notify/test", bytes.NewBuffer(body))
		req.Header.Set("Content-Type", "application/json")
		w := httptest.NewRecorder()

		r.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)

		var resp map[string]interface{}
		err := json.Unmarshal(w.Body.Bytes(), &resp)
		assert.NoError(t, err)
		assert.Equal(t, "Notification dispatched", resp["message"])
		assert.Equal(t, "user_456", resp["user_id"])
		assert.Equal(t, "datum-user_456", resp["topic"])
	})

	t.Run("With Title and Priority", func(t *testing.T) {
		handler.Dispatcher = notify.NewDispatcher()

		reqBody := map[string]string{
			"user_id":  "user_789",
			"message":  "Custom title test",
			"title":    "My Title",
			"priority": "max",
		}
		body, _ := json.Marshal(reqBody)

		req, _ := http.NewRequest("POST", "/notify/test", bytes.NewBuffer(body))
		req.Header.Set("Content-Type", "application/json")
		w := httptest.NewRecorder()

		r.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)
	})

	t.Run("Missing Required Fields", func(t *testing.T) {
		handler.Dispatcher = notify.NewDispatcher()

		reqBody := map[string]string{
			"title": "No user_id or message",
		}
		body, _ := json.Marshal(reqBody)

		req, _ := http.NewRequest("POST", "/notify/test", bytes.NewBuffer(body))
		req.Header.Set("Content-Type", "application/json")
		w := httptest.NewRecorder()

		r.ServeHTTP(w, req)

		assert.Equal(t, http.StatusBadRequest, w.Code)
	})

	t.Run("Invalid JSON", func(t *testing.T) {
		handler.Dispatcher = notify.NewDispatcher()

		req, _ := http.NewRequest("POST", "/notify/test", bytes.NewBuffer([]byte("not-json")))
		req.Header.Set("Content-Type", "application/json")
		w := httptest.NewRecorder()

		r.ServeHTTP(w, req)

		assert.Equal(t, http.StatusBadRequest, w.Code)
	})

	t.Run("Default Title and Priority", func(t *testing.T) {
		handler.Dispatcher = notify.NewDispatcher()

		reqBody := map[string]string{
			"user_id": "user_default",
			"message": "Defaults test",
		}
		body, _ := json.Marshal(reqBody)

		req, _ := http.NewRequest("POST", "/notify/test", bytes.NewBuffer(body))
		req.Header.Set("Content-Type", "application/json")
		w := httptest.NewRecorder()

		r.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)
	})
}
