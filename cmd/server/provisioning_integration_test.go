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

func TestProvisioningEndToEndFlow(t *testing.T) {
	// 1. Setup Server
	gin.SetMode(gin.TestMode)
	tmpDir := t.TempDir()

	// Use explicit variable for store to avoid global state issues if possible,
	// but main handlers use global 'store'. So we overwrite it.
	var err error
	store, err = storage.New(tmpDir+"/meta_prov.db", tmpDir+"/tsdata_prov", 7*24*time.Hour)
	require.NoError(t, err)
	defer store.Close()

	// Create User
	userID := "e2e-user"
	err = store.CreateUser(&storage.User{
		ID:           userID,
		Email:        "e2e@test.com",
		PasswordHash: "pass",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	})
	require.NoError(t, err)

	router := gin.New()
	RegisterProvisioningRoutes(router, func(c *gin.Context) {
		c.Set("user_id", userID)
		c.Next()
	}) // Mock auth middleware for simplicity

	// 2. User Registers Device (Mobile App)
	// POST /devices/register
	regBody := map[string]string{
		"device_uid":  "AA:BB:CC:DD:EE:01",
		"device_name": "E2E Sensor",
		"device_type": "sensor",
		"wifi_ssid":   "MyHomeWiFi",
		"wifi_pass":   "SecretPass",
	}
	regBytes, _ := json.Marshal(regBody)
	reqReg := httptest.NewRequest("POST", "/devices/register", bytes.NewReader(regBytes))
	reqReg.Header.Set("Content-Type", "application/json")
	wReg := httptest.NewRecorder()
	router.ServeHTTP(wReg, reqReg)

	require.Equal(t, http.StatusCreated, wReg.Code)
	var regResp RegisterDeviceResponse
	json.Unmarshal(wReg.Body.Bytes(), &regResp)
	assert.NotEmpty(t, regResp.RequestID)
	assert.Equal(t, "pending", regResp.Status)

	// 3. User Lists Requests
	// GET /devices/provisioning
	reqList := httptest.NewRequest("GET", "/devices/provisioning", nil)
	wList := httptest.NewRecorder()
	router.ServeHTTP(wList, reqList)

	require.Equal(t, http.StatusOK, wList.Code)

	// FIX: Parse wrapper object {"requests": []}
	var listObj struct {
		Requests []storage.ProvisioningRequest `json:"requests"`
	}
	json.Unmarshal(wList.Body.Bytes(), &listObj)
	listResp := listObj.Requests

	assert.Len(t, listResp, 1)
	assert.Equal(t, "pending", listResp[0].Status)

	// 4. Device Activation (simulated Device)

	// Device Check UID (Public Device Endpoint)
	reqCheck := httptest.NewRequest("GET", "/provisioning/check/AA:BB:CC:DD:EE:01", nil)
	wCheck := httptest.NewRecorder()
	router.ServeHTTP(wCheck, reqCheck)
	assert.Equal(t, http.StatusOK, wCheck.Code)

	var checkResp map[string]interface{}
	json.Unmarshal(wCheck.Body.Bytes(), &checkResp)
	assert.Equal(t, "pending", checkResp["status"])

	// Device Activate
	// POST /provisioning/activate with payload
	actBody := map[string]string{
		"request_id": regResp.RequestID,
		"device_uid": "AA:BB:CC:DD:EE:01",
	}
	actBytes, _ := json.Marshal(actBody)
	reqAct := httptest.NewRequest("POST", "/provisioning/activate", bytes.NewReader(actBytes))
	reqAct.Header.Set("Content-Type", "application/json")
	wAct := httptest.NewRecorder()
	router.ServeHTTP(wAct, reqAct)

	require.Equal(t, http.StatusOK, wAct.Code)
	var actResp DeviceActivateResponse
	json.Unmarshal(wAct.Body.Bytes(), &actResp)
	assert.NotEmpty(t, actResp.APIKey)
	assert.NotEmpty(t, actResp.DeviceID)

	// 5. Verify Request Completed
	reqList2 := httptest.NewRequest("GET", "/devices/provisioning", nil)
	wList2 := httptest.NewRecorder()
	router.ServeHTTP(wList2, reqList2)

	var listObj2 struct {
		Requests []storage.ProvisioningRequest `json:"requests"`
	}
	json.Unmarshal(wList2.Body.Bytes(), &listObj2)
	listResp2 := listObj2.Requests

	assert.Len(t, listResp2, 1)
	assert.Equal(t, "completed", listResp2[0].Status)

	// 6. Test Expiration Cleanup Logic
	// Create another request that we will let expire
	regBody2 := map[string]string{
		"device_uid":  "AA:BB:CC:DD:EE:02",
		"device_name": "Expired Sensor",
		"device_type": "sensor",
	}
	regBytes2, _ := json.Marshal(regBody2)
	reqReg2 := httptest.NewRequest("POST", "/devices/register", bytes.NewReader(regBytes2))
	reqReg2.Header.Set("Content-Type", "application/json")
	wReg2 := httptest.NewRecorder()
	router.ServeHTTP(wReg2, reqReg2)

	// 7. Verify Purge (Hard Delete)
	// We can test purge by creating a request and then calling Purge with very small maxAge.
	// But first mark it as 'completed' (which we did for the first device).
	// Complete requests are eligible for purge.

	// Wait a tiny bit to ensure CreatedAt is strictly before purge cutoff
	time.Sleep(10 * time.Millisecond)

	count, err := store.PurgeProvisioningRequests(1 * time.Microsecond) // Purge everything older than 1us
	require.NoError(t, err)
	assert.GreaterOrEqual(t, count, 1) // Should delete the completed request

	// Verify it's gone
	reqList3 := httptest.NewRequest("GET", "/devices/provisioning", nil)
	wList3 := httptest.NewRecorder()
	router.ServeHTTP(wList3, reqList3)

	var listObj3 struct {
		Requests []storage.ProvisioningRequest `json:"requests"`
	}
	json.Unmarshal(wList3.Body.Bytes(), &listObj3)
	listResp3 := listObj3.Requests

	// Should only have the Pending one (from step 6), the Completed one (Step 5) is purged.
	foundCompleted := false
	for _, r := range listResp3 {
		if r.DeviceUID == "AA:BB:CC:DD:EE:01" {
			foundCompleted = true
		}
	}
	assert.False(t, foundCompleted, "Completed request should have been purged")
}
