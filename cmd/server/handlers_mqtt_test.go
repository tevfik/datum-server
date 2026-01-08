package main

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
)

func TestMQTTHandlers(t *testing.T) {
	gin.SetMode(gin.TestMode)

	// Since we cannot easily mock the global mqttBroker in this package structure without refactoring,
	// and mqttBroker is nil by default in tests, these handlers will return 503.
	// This confirms the handlers are wired and handle the nil case gracefully.
	// For full testing, we would need to mock the broker which requires interface decoupling.

	t.Run("GetStats_NoBroker", func(t *testing.T) {
		w := httptest.NewRecorder()
		c, _ := gin.CreateTestContext(w)

		getMQTTStatsHandler(c)

		assert.Equal(t, http.StatusServiceUnavailable, w.Code)
	})

	t.Run("GetClients_NoBroker", func(t *testing.T) {
		w := httptest.NewRecorder()
		c, _ := gin.CreateTestContext(w)

		getMQTTClientsHandler(c)

		assert.Equal(t, http.StatusServiceUnavailable, w.Code)
	})

	t.Run("Publish_NoBroker", func(t *testing.T) {
		w := httptest.NewRecorder()
		c, _ := gin.CreateTestContext(w)

		reqBody := PublishRequest{
			Topic:   "test/topic",
			Message: "hello",
		}
		jsonBytes, _ := json.Marshal(reqBody)
		c.Request = httptest.NewRequest("POST", "/admin/mqtt/publish", bytes.NewBuffer(jsonBytes))

		postMQTTPublishHandler(c)

		assert.Equal(t, http.StatusServiceUnavailable, w.Code)
	})
}
