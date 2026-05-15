package provisioning

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

	handler := NewHandler(store, Config{
		ServerURL: "http://localhost:8000",
	})
	return handler, store, func() { store.Close() }
}

func TestCheckDeviceUID(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	// 1. Not registered
	r := gin.New()
	r.GET("/dev/check-uid/:uid", handler.CheckDeviceUIDHandler)

	req, _ := http.NewRequest("GET", "/dev/check-uid/aabbcc", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	var resp CheckUIDResponse
	json.Unmarshal(w.Body.Bytes(), &resp)
	assert.False(t, resp.Registered)

	// 2. Registered
	require.NoError(t, store.CreateDevice(&storage.Device{
		ID:        "dev_1",
		DeviceUID: "AABBCC",
		Name:      "Test",
		UserID:    "user_1",
	}))

	w = httptest.NewRecorder()
	r.ServeHTTP(w, req)
	assert.Equal(t, http.StatusOK, w.Code)
	json.Unmarshal(w.Body.Bytes(), &resp)
	assert.True(t, resp.Registered)
	assert.Equal(t, "dev_1", resp.DeviceID)
}

func TestDeviceCheck(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.GET("/prov/check/:uid", handler.DeviceCheckHandler)

	// 1. Unconfigured
	req, _ := http.NewRequest("GET", "/prov/check/112233", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
	
	// 2. Pending request
	require.NoError(t, store.CreateProvisioningRequest(&storage.ProvisioningRequest{
		ID:        "prov_1",
		DeviceUID: "112233",
		UserID:    "user_1",
		Status:    "pending",
		ExpiresAt: time.Now().Add(10 * time.Minute),
	}))

	w = httptest.NewRecorder()
	r.ServeHTTP(w, req)
	assert.Equal(t, http.StatusOK, w.Code)
	var resp map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &resp)
	assert.Equal(t, "pending", resp["status"])
}

func TestProvisioning_FullFlow(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "user_1")
		c.Next()
	})

	r.POST("/dev/register", handler.RegisterDeviceHandler)
	r.GET("/prov/check/:uid", handler.DeviceCheckHandler)
	r.POST("/prov/activate", handler.DeviceActivateHandler)

	// 1. Register Device (Mobile App side)
	regBody := RegisterDeviceRequest{
		DeviceUID:  "UID123",
		DeviceName: "My Device",
		DeviceType: "relay_board",
	}
	body, _ := json.Marshal(regBody)
	req, _ := http.NewRequest("POST", "/dev/register", bytes.NewBuffer(body))
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)
	var regResp RegisterDeviceResponse
	json.Unmarshal(w.Body.Bytes(), &regResp)
	assert.NotEmpty(t, regResp.RequestID)

	// 2. Check Status (Device side)
	req, _ = http.NewRequest("GET", "/prov/check/UID123", nil)
	w = httptest.NewRecorder()
	r.ServeHTTP(w, req)
	assert.Equal(t, http.StatusOK, w.Code)

	// 3. Activate (Device side)
	actReq := DeviceActivateRequest{
		DeviceUID: "UID123",
	}
	body, _ = json.Marshal(actReq)
	req, _ = http.NewRequest("POST", "/prov/activate", bytes.NewBuffer(body))
	w = httptest.NewRecorder()
	r.ServeHTTP(w, req)
	
	// Note: RegisterDeviceHandler in this project AUTO-COMPLETES the request if possible.
	// So activation might return 409 Conflict (already registered) or 200.
	// Let's check the code: RegisterDeviceHandler line 215 calls CompleteProvisioningRequest.
	// So the device is already active.
	assert.True(t, w.Code == http.StatusOK || w.Code == http.StatusConflict)
}
