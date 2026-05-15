package mqtt_api

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"datum-go/internal/mqtt"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func setupTestMQTT(t *testing.T) (*Handler, *mqtt.Broker, func()) {
	gin.SetMode(gin.TestMode)
	
	// Broker needs a store and processor, but we can pass nil for basic API testing
	// if the methods we use don't crash on nil. 
	// GetStats and GetConnectedClients use b.server.Clients which is initialized in NewBroker.
	broker := mqtt.NewBroker(nil, nil)
	handler := NewHandler(broker)
	
	return handler, broker, func() {
		broker.Stop()
	}
}

func TestGetMQTTStats(t *testing.T) {
	handler, _, cleanup := setupTestMQTT(t)
	defer cleanup()

	r := gin.New()
	r.GET("/admin/mqtt/stats", handler.GetMQTTStatsHandler)

	req, _ := http.NewRequest("GET", "/admin/mqtt/stats", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	
	var resp map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &resp)
	require.NoError(t, err)
	assert.Contains(t, resp, "clients_connected")
}

func TestGetMQTTClients(t *testing.T) {
	handler, _, cleanup := setupTestMQTT(t)
	defer cleanup()

	r := gin.New()
	r.GET("/admin/mqtt/clients", handler.GetMQTTClientsHandler)

	req, _ := http.NewRequest("GET", "/admin/mqtt/clients", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	
	var resp map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &resp)
	require.NoError(t, err)
	assert.Contains(t, resp, "clients")
}

func TestPostMQTTPublish(t *testing.T) {
	handler, _, cleanup := setupTestMQTT(t)
	defer cleanup()

	r := gin.New()
	r.POST("/admin/mqtt/publish", handler.PostMQTTPublishHandler)

	pubReq := PublishRequest{
		Topic:   "test/topic",
		Message: "hello",
		Retain:  false,
	}
	body, _ := json.Marshal(pubReq)
	req, _ := http.NewRequest("POST", "/admin/mqtt/publish", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	assert.Contains(t, w.Body.String(), "published")
}

func TestMQTTBroker_Nil(t *testing.T) {
	handler := NewHandler(nil)
	r := gin.New()
	r.GET("/admin/mqtt/stats", handler.GetMQTTStatsHandler)

	req, _ := http.NewRequest("GET", "/admin/mqtt/stats", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusServiceUnavailable, w.Code)
}
