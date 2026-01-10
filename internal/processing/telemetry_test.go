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
	_, err = tp.Process(deviceID, payload)
	assert.NoError(t, err)

	// Force flush by closing
	tp.Close()

	// Check data stored
	latest, err := store.GetLatestData(deviceID)
	assert.NoError(t, err)
	assert.Equal(t, 25.5, latest.Data["temp"])
	// Public IP should not be enriched automatically anymore
	// assert.Equal(t, "127.0.0.1", latest.Data["public_ip"])
	assert.Contains(t, latest.Data, "server_time") // enriched
}

func TestTelemetryProcessor_Batching(t *testing.T) {
	// Setup memory store for BuntDB, temp dir for TStorage
	tmp := t.TempDir()
	store, err := storage.New(":memory:", tmp, 0)
	assert.NoError(t, err)

	tp := NewTelemetryProcessor(store)
	// Override flush interval for faster test if needed, but Close() handles it

	deviceID := "batch-device-1"
	store.InitializeSystem("Test", false, 7)
	device := &storage.Device{ID: deviceID, Status: "active", UserID: "u1"}
	store.CreateDevice(device)

	// Send 10 points
	for i := 0; i < 10; i++ {
		payload := map[string]interface{}{"value": float64(i)}
		_, err := tp.Process(deviceID, payload)
		assert.NoError(t, err)
	}

	// Allow time for async workers to pick up data from channel
	time.Sleep(200 * time.Millisecond)

	// Force flush
	tp.Close()

	// 1. Check updated shadow (BuntDB)
	latest, err := store.GetLatestData(deviceID)
	assert.NoError(t, err)
	assert.Equal(t, float64(9), latest.Data["value"]) // Should be lighter

	// 2. Check history (TStorage)
	history, err := store.GetDataHistory(deviceID, 100)
	assert.NoError(t, err)
	assert.Len(t, history, 10)

	// Verify order or content (optional)
	// History is sorted descending by default
	assert.Equal(t, float64(9), history[0].Data["value"])
}

func TestTelemetryProcessor_CommandCheck(t *testing.T) {
	store, _ := storage.New(":memory:", "", 0)
	tp := NewTelemetryProcessor(store)
	defer tp.Close() // Good practice
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
	res, err := tp.Process(deviceID, map[string]interface{}{"a": 1})
	assert.NoError(t, err)

	// Check pending count
	assert.Equal(t, 1, res.CommandsPending)
}
