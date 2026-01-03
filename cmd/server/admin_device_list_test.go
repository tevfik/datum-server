package main

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
)

// TestAdminListAllDevices verifies that admins can see all devices
func TestAdminListAllDevices(t *testing.T) {
	gin.SetMode(gin.TestMode)
	testStore, err := storage.New(":memory:", "", 7*24*time.Hour)
	assert.NoError(t, err)
	store = testStore

	// Initialize system
	testStore.InitializeSystem("Test", false, 7)

	// Create regular user
	user1 := &storage.User{
		ID:    "user-1",
		Email: "user1@test.com",
		Role:  "user",
	}
	testStore.CreateUser(user1)

	// Create admin user
	admin := &storage.User{
		ID:    "admin-1",
		Email: "admin@test.com",
		Role:  "admin",
	}
	testStore.CreateUser(admin)

	// Create device for user 1
	device1 := &storage.Device{
		ID:        "dev_1_owned_by_user1",
		UserID:    "user-1",
		Name:      "User1 Device",
		Type:      "camera",
		DeviceUID: "UID1",
		APIKey:    "key1",
		CreatedAt: time.Now(),
	}
	err = testStore.CreateDevice(device1)
	assert.NoError(t, err)

	// Create device for admin (just to have another device)
	device2 := &storage.Device{
		ID:        "dev_2_owned_by_admin",
		UserID:    "admin-1",
		Name:      "Admin Device",
		Type:      "sensor",
		DeviceUID: "UID2",
		APIKey:    "key2",
		CreatedAt: time.Now(),
	}
	err = testStore.CreateDevice(device2)
	assert.NoError(t, err)

	// Setup router
	r := gin.New()
	deviceGroup := r.Group("/devices")
	deviceGroup.Use(auth.AuthMiddleware())
	{
		deviceGroup.GET("", listDevicesHandler)
	}

	// 1. Test Regular User access (should see only device1)
	tokenUser, _ := auth.GenerateToken(user1.ID, user1.Email, user1.Role)
	req1, _ := http.NewRequest("GET", "/devices", nil)
	req1.Header.Set("Authorization", "Bearer "+tokenUser)
	w1 := httptest.NewRecorder()
	r.ServeHTTP(w1, req1)

	assert.Equal(t, http.StatusOK, w1.Code)
	// Actually response uses DeviceResponse struct which maps to JSON
	var rawResp1 map[string][]map[string]interface{}
	json.Unmarshal(w1.Body.Bytes(), &rawResp1)

	devices1 := rawResp1["devices"]
	assert.Len(t, devices1, 1, "Regular user should see 1 device")
	assert.Equal(t, "dev_1_owned_by_user1", devices1[0]["id"])

	// 2. Test Admin access (should see BOTH devices)
	tokenAdmin, _ := auth.GenerateToken(admin.ID, admin.Email, admin.Role)
	req2, _ := http.NewRequest("GET", "/devices", nil)
	req2.Header.Set("Authorization", "Bearer "+tokenAdmin)
	w2 := httptest.NewRecorder()
	r.ServeHTTP(w2, req2)

	assert.Equal(t, http.StatusOK, w2.Code)
	var rawResp2 map[string][]map[string]interface{}
	json.Unmarshal(w2.Body.Bytes(), &rawResp2)

	devices2 := rawResp2["devices"]
	assert.Len(t, devices2, 2, "Admin should see ALL 2 devices")

	// Check IDs present
	var ids []string
	for _, d := range devices2 {
		ids = append(ids, d["id"].(string))
	}
	assert.Contains(t, ids, "dev_1_owned_by_user1")
	assert.Contains(t, ids, "dev_2_owned_by_admin")

	testStore.Close()
}
