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

// TestProvisioningFlowIntegration tests the provisioning flow
func TestProvisioningFlowIntegration(t *testing.T) {
	// Setup storage using t.TempDir()
	gin.SetMode(gin.TestMode)
	tmpDir := t.TempDir()
	var err error
	// Use slightly different path to avoid collision if running parallel
	store, err = storage.New(tmpDir+"/meta_prov.db", tmpDir+"/tsdata_prov", 0)
	require.NoError(t, err)
	defer store.Close()

	r := gin.New()

	// Create a user for the mobile app
	userID := "user_test_prov_1"
	user := &storage.User{
		ID:        userID,
		Email:     "mobile_test@example.com",
		Role:      "user",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	err = store.CreateUser(user)
	require.NoError(t, err)

	// Auth Middleware Mock
	authMiddleware := func(c *gin.Context) {
		c.Set("user_id", userID)
		c.Next()
	}

	// Register Routes
	RegisterProvisioningRoutes(r, authMiddleware)

	// ==========================================
	// Step 1: Mobile App Registers Device
	// ==========================================
	deviceUID := "AA:BB:CC:DD:EE:FF"
	regReq := RegisterDeviceRequest{
		DeviceUID:  deviceUID,
		DeviceName: "Living Room Camera",
		DeviceType: "camera",
		WiFiSSID:   "HomeWiFi",
		WiFiPass:   "secret123",
	}
	body, _ := json.Marshal(regReq)

	w := httptest.NewRecorder()
	req, _ := http.NewRequest("POST", "/devices/register", bytes.NewBuffer(body))
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var regResp RegisterDeviceResponse
	err = json.Unmarshal(w.Body.Bytes(), &regResp)
	assert.NoError(t, err)
	assert.NotEmpty(t, regResp.RequestID)
	// Current implementation auto-activates
	assert.Equal(t, "active", regResp.Status)
	assert.NotEmpty(t, regResp.DeviceID)
	assert.NotEmpty(t, regResp.APIKey)

	// ==========================================
	// Step 2: Device Checks Status (Simulated)
	// ==========================================
	// Since status is "active", /check-uid should return generic info or status?
	// The handler `deviceCheckHandler` returns 200 with status=active if registered.

	w2 := httptest.NewRecorder()
	req2, _ := http.NewRequest("GET", "/provisioning/check/"+deviceUID, nil)
	r.ServeHTTP(w2, req2)

	assert.Equal(t, http.StatusOK, w2.Code)
	var checkMap map[string]interface{}
	err = json.Unmarshal(w2.Body.Bytes(), &checkMap)
	assert.NoError(t, err)
	// Expect status to handle "registered but checking" scenario?
	// Wait, CheckUIDResponse structure:
	/*
		type CheckUIDResponse struct {
			Registered bool   `json:"registered"`
			DeviceID   string `json:"device_id,omitempty"`
			HasPending bool   `json:"has_pending"`
			RequestID  string `json:"request_id,omitempty"`
		}
	*/
	// BUT `deviceCheckHandler` uses `gin.H` for response:
	/*
			if registered {
		        // ...
				c.JSON(http.StatusConflict, gin.H{"error":...})
			}
	*/
	// Wait, `deviceCheckHandler` BLOCKS registered devices?
	// Lines 361-372 in `provisioning_handlers.go` (in deviceActivateHandler) blocks registered.
	// But `deviceCheckHandler`:
	/*
			// Check if there's a pending request
			provReq, err := store.GetProvisioningRequestByUID(deviceUID)
		    // ...
	*/
	// It relies on `GetProvisioningRequestByUID`.
	// Does `CreateDevice` in Step 1 create a persistent ProvisioningRequest?
	// Yes, lines 169-183 creates `provReq` with status "active".
	// So `GetProvisioningRequestByUID` should find it.

	// Then logic:
	/*
		if provReq.Status != "pending" {
			c.JSON(http.StatusOK, gin.H{
				"status":  provReq.Status,
				"message": "Provisioning request is " + provReq.Status,
			})
			return
		}
	*/

	assert.Equal(t, "active", checkMap["status"])
}
