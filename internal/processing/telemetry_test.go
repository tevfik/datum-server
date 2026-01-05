package processing

import (
	"datum-go/internal/storage"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
)

func TestTelemetryProcessor_Process(t *testing.T) {
	// Setup memory store
	store, err := storage.New(":memory:", "", 0)
	assert.NoError(t, err)

	tp := NewTelemetryProcessor(store)

	// Create test device
	deviceID := "test-device-1"
	// We might need to create the device in store if Process checks it?
	// Process() blindly stores data using store.StoreData which validates device existence usually?
	// Let's create it to be safe.
	store.InitializeSystem("Test", false, 7)
	device := &storage.Device{
		ID:        deviceID,
		UserID:    "user-1",
		APIKey:    "sk_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	payload := map[string]interface{}{
		"temp": 25.5,
	}

	// EXECUTE
	_, err = tp.Process(deviceID, payload, "127.0.0.1")

	// VERIFY
	assert.NoError(t, err)

	// Check data stored
	latest, err := store.GetLatestData(deviceID)
	assert.NoError(t, err)
	assert.Equal(t, 25.5, latest.Data["temp"])
	assert.Equal(t, "127.0.0.1", latest.Data["public_ip"]) // enriched
	assert.Contains(t, latest.Data, "server_time")         // enriched
}

func TestTelemetryProcessor_CommandCheck(t *testing.T) {
	store, _ := storage.New(":memory:", "", 0)
	tp := NewTelemetryProcessor(store)
	store.InitializeSystem("Test", false, 7)

	deviceID := "cmd-device-1"
	device := &storage.Device{ID: deviceID, Status: "active", UserID: "u1"}
	store.CreateDevice(device)

	// Create a pending command
	cmd := &storage.Command{
		ID:        "cmd-1",
		DeviceID:  deviceID,
		Action:    "reboot",
		Status:    "pending",
		CreatedAt: time.Now(),
	}
	store.CreateCommand(cmd)

	// Process data
	res, err := tp.Process(deviceID, map[string]interface{}{"a": 1}, "1.1.1.1")
	assert.NoError(t, err)

	// Check pending count
	assert.Equal(t, 1, res.CommandsPending)
}
