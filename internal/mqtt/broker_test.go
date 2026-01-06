package mqtt

import (
	"datum-go/internal/processing"
	"datum-go/internal/storage"
	"encoding/json"
	"testing"
	"time"

	mqtt "github.com/mochi-mqtt/server/v2"
	"github.com/mochi-mqtt/server/v2/packets"
	"github.com/stretchr/testify/assert"
)

func TestAuthHook_OnConnectAuthenticate(t *testing.T) {
	// Setup
	store, _ := storage.New(":memory:", "", 0)
	store.InitializeSystem("Test", false, 7)

	// Create device
	device := &storage.Device{
		ID:        "dev-1",
		UserID:    "u1",
		APIKey:    "sk_valid_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	store.CreateDevice(device)

	hook := newAuthHook(store)

	tests := []struct {
		name     string
		clientID string
		password string
		want     bool
	}{
		{"Valid Auth", "dev-1", "sk_valid_key", true},
		{"Invalid Key", "dev-1", "sk_wrong", false},
		{"Wrong DeviceID", "dev-2", "sk_valid_key", false}, // Key belongs to dev-1
		{"Empty Creds", "", "", false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			cl := &mqtt.Client{}
			pk := packets.Packet{
				Connect: packets.ConnectParams{
					Username: []byte(tt.clientID),
					Password: []byte(tt.password),
				},
			}
			got := hook.OnConnectAuthenticate(cl, pk)
			assert.Equal(t, tt.want, got)
		})
	}
}

func TestAuthHook_OnACLCheck(t *testing.T) {
	hook := newAuthHook(nil) // DB not needed for ACL check logic as it uses ID

	tests := []struct {
		name     string
		clientID string
		topic    string
		write    bool
		want     bool
	}{
		{"Write Own Data", "dev-1", "data/dev-1", true, true},
		{"Write Other Data", "dev-1", "data/dev-2", true, false},
		{"Read Own Cmd", "dev-1", "cmd/dev-1", false, true},
		{"Read Other Cmd", "dev-1", "cmd/dev-2", false, false},
		{"Write Cmd (Denied)", "dev-1", "cmd/dev-1", true, false},   // Devices can't write to cmd
		{"Read Data (Denied)", "dev-1", "data/dev-1", false, false}, // Devices can't read data (privacy?) or just not supported yet
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			cl := &mqtt.Client{ID: tt.clientID}
			got := hook.OnACLCheck(cl, tt.topic, tt.write)
			assert.Equal(t, tt.want, got)
		})
	}
}

func TestIngestionHook_OnPublish(t *testing.T) {
	// Setup
	store, _ := storage.New(":memory:", "", 0)
	store.InitializeSystem("Test", false, 7)
	tp := processing.NewTelemetryProcessor(store)
	hook := newIngestionHook(tp)

	// Create device so storage works
	store.CreateDevice(&storage.Device{ID: "dev-1", Status: "active"})

	// Prepare packet
	payload := map[string]interface{}{"temp": 42}
	payloadBytes, _ := json.Marshal(payload)

	cl := &mqtt.Client{
		Net: mqtt.ClientConnection{Remote: "192.168.1.50:12345"},
	}
	pk := packets.Packet{
		TopicName: "data/dev-1",
		Payload:   payloadBytes,
	}

	// Execute
	_, err := hook.OnPublish(cl, pk)
	assert.NoError(t, err)

	// Flush async processor
	tp.Close()

	// Verify Data Stored
	latest, err := store.GetLatestData("dev-1")
	assert.NoError(t, err)
	assert.Equal(t, float64(42), latest.Data["temp"])
	assert.Equal(t, "192.168.1.50", latest.Data["public_ip"])
}
