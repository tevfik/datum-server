package mqtt

import (
	"bytes"
	"encoding/json"
	"fmt"
	"log"
	"net"
	"strings"
	"time"

	"datum-go/internal/processing"
	"datum-go/internal/storage"

	mqtt "github.com/mochi-mqtt/server/v2"
	"github.com/mochi-mqtt/server/v2/listeners"
	"github.com/mochi-mqtt/server/v2/packets"
)

// Broker wraps the mochi-mqtt server
type Broker struct {
	server    *mqtt.Server
	store     storage.Provider
	processor *processing.TelemetryProcessor
}

// NewBroker creates a new MQTT broker instance
func NewBroker(store storage.Provider, processor *processing.TelemetryProcessor) *Broker {
	server := mqtt.New(&mqtt.Options{
		InlineClient: true,
	})
	return &Broker{
		server:    server,
		store:     store,
		processor: processor,
	}
}

// Start initializes listeners and starts the broker
func (b *Broker) Start() error {
	// TCP Listener
	tcp := listeners.NewTCP(listeners.Config{
		ID:      "t1",
		Address: ":1883",
	})
	if err := b.server.AddListener(tcp); err != nil {
		return err
	}

	// WebSocket Listener
	ws := listeners.NewWebsocket(listeners.Config{
		ID:      "ws1",
		Address: ":1884",
	})
	if err := b.server.AddListener(ws); err != nil {
		return err
	}

	// Auth Hook
	if err := b.server.AddHook(newAuthHook(b.store), nil); err != nil {
		return err
	}

	// Ingestion Hook (Data Processing)
	if err := b.server.AddHook(newIngestionHook(b.processor, b.store), nil); err != nil {
		return err
	}

	go func() {
		err := b.server.Serve()
		if err != nil {
			log.Printf("Error serving MQTT: %v", err)
		}
	}()

	log.Println("MQTT Broker started on :1883 (TCP) and :1884 (WS)")
	return nil
}

// Stop gracefully shuts down the broker
func (b *Broker) Stop() {
	b.server.Close()
}

// PublishCommand sends a command payload to a specific device
func (b *Broker) PublishCommand(deviceID string, payload []byte) error {
	topic := fmt.Sprintf("dev/%s/cmd", deviceID)
	return b.server.Publish(topic, payload, false, 0)
}

// Publish sends a payload to any topic
func (b *Broker) Publish(topic string, payload []byte, retain bool) error {
	return b.server.Publish(topic, payload, retain, 0)
}

// IsDeviceConnected checks if a device is currently connected
func (b *Broker) IsDeviceConnected(deviceID string) bool {
	// Mochi-MQTT v2 tracks clients by ID.
	// We assume Device.ID is used as ClientID.
	_, ok := b.server.Clients.Get(deviceID)
	return ok
}

// ClientInfo represents connected client details
type ClientInfo struct {
	ID        string `json:"id"`
	IP        string `json:"ip"`
	Connected bool   `json:"connected"`
}

// GetConnectedClients returns a list of currently connected clients
func (b *Broker) GetConnectedClients() []ClientInfo {
	var clients []ClientInfo
	// Iterate over connected clients
	// Mochi-MQTT v2 Clients.GetAll() returns map[string]*Client
	for _, client := range b.server.Clients.GetAll() {
		if !client.Closed() {
			ip, _, _ := net.SplitHostPort(client.Net.Remote)
			clients = append(clients, ClientInfo{
				ID:        client.ID,
				IP:        ip,
				Connected: !client.Closed(),
			})
		}
	}
	return clients
}

// BrokerStats represents broker statistics
type BrokerStats struct {
	BytesRecv     int64 `json:"bytes_recv"`
	BytesSent     int64 `json:"bytes_sent"`
	Clients       int   `json:"clients_connected"`
	Subscriptions int   `json:"subscriptions"`
	Inflight      int   `json:"inflight"`
}

// GetStats returns current broker statistics
func (b *Broker) GetStats() BrokerStats {
	// stats := b.server.Info.Stats
	// Simplified for compatibility with current mochi-mqtt version
	return BrokerStats{
		BytesRecv:     0,
		BytesSent:     0,
		Clients:       b.server.Clients.Len(),
		Subscriptions: 0,
		Inflight:      0,
	}
}

// -----------------------------------------------------------------------------
// Auth Hook
// -----------------------------------------------------------------------------

type AuthHook struct {
	mqtt.HookBase
	store storage.Provider
}

func newAuthHook(store storage.Provider) *AuthHook {
	return &AuthHook{store: store}
}

func (h *AuthHook) ID() string {
	return "datum-auth"
}

func (h *AuthHook) Provides(b byte) bool {
	return bytes.Contains([]byte{
		mqtt.OnConnectAuthenticate,
		mqtt.OnACLCheck,
	}, []byte{b})
}

func (h *AuthHook) OnConnectAuthenticate(cl *mqtt.Client, pk packets.Packet) bool {
	// Username = DeviceID (or "admin_dashboard_..." for admins)
	// Password = Token/API Key

	clientID := string(pk.Connect.Username) // In our logic we treat Username as the identifier
	token := string(pk.Connect.Password)

	// If no credentials, reject
	if clientID == "" {
		return false
	}

	// 1. Admin/User Dashboard Authentication
	if strings.HasPrefix(clientID, "admin_dashboard_") {
		// Expecting User API Key
		if strings.HasPrefix(token, "ak_") {
			user, err := h.store.GetUserByUserAPIKey(token)
			if err != nil {
				return false
			}
			// Allow valid users (Role check moved to ACL)
			if user != nil {
				return true
			}
		}
		return false
	}

	// 2. Device Authentication
	// clientID must be the DeviceID
	deviceID := clientID

	// Validate API Key (sk_ or dk_)
	if strings.HasPrefix(token, "sk_") || strings.HasPrefix(token, "dk_") {
		device, err := h.store.GetDeviceByAPIKey(token)
		if err != nil || device.ID != deviceID {
			return false
		}
		return true
	}

	return false
}

func (h *AuthHook) OnACLCheck(cl *mqtt.Client, topic string, write bool) bool {
	// Enforce Topic Structure:
	// data/{device_id} -> Write Only
	// cmd/{device_id} -> Read Only

	// Dashboard Access (Admin vs User)
	// Format: datum_web_{role}_{userid}_{random}
	if strings.HasPrefix(cl.ID, "datum_web_") {
		parts := strings.Split(cl.ID, "_")
		if len(parts) >= 4 {
			role := parts[2]
			userID := parts[3]

			// Admin: All Access
			if role == "admin" {
				return true
			}

			// User: Own Devices Only
			// Check if topic targets a specific device
			topicParts := strings.Split(topic, "/")
			if len(topicParts) >= 2 {
				// Format: dev/{deviceID}/...
				targetDeviceID := topicParts[1]
				// If old format data/{deviceID} or cmd/{deviceID}, handle that too for safety
				// But we are moving to dev/{deviceID}
				if topicParts[0] != "dev" {
					// Backward compatibility or rejection?
					// Let's assume strict new format for now as requested
					// Check targetID is the second part
					targetDeviceID = topicParts[1]
				}

				// Verify ownership
				// Optimization: GetUserDevices is fast (in-memory BuntDB)
				devices, err := h.store.GetUserDevices(userID)
				if err == nil {
					for _, d := range devices {
						if d.ID == targetDeviceID {
							return true // Allowed
						}
					}
				}
			}
			return false // Block access to other devices
		}
		// Fallback for old clients or malformed IDs -> Block
		return false
	}

	// Device Access
	deviceID := cl.ID // Authenticated as DeviceID

	// Split topic
	parts := strings.Split(topic, "/")
	// Expected format: dev/{deviceID}/{action}
	if len(parts) < 3 {
		return false
	}

	prefix := parts[0]
	targetID := parts[1]
	suffix := parts[2]

	// Must start with dev
	if prefix != "dev" {
		return false
	}

	// Must match own ID
	if targetID != deviceID {
		return false
	}

	if write {
		// Can only write to dev/{id}/data (for device) or dev/{id}/cmd (for admin/user - already covered above)
		// Since we are in "Device Access" section (cl.ID == deviceID), a device should NOT be writing to its own cmd topic usually.
		// However, if we want to support device-to-device via broker later, maybe.
		// For now, Devices only write to DATA.
		return suffix == "data"
	} else {
		// Can only read from dev/{id}/cmd, dev/{id}/fw, or dev/{id}/data (Live Logs)
		return suffix == "cmd" || suffix == "fw" || suffix == "data"
	}
}

// -----------------------------------------------------------------------------
// Ingestion Hook
// -----------------------------------------------------------------------------

type IngestionHook struct {
	mqtt.HookBase
	processor *processing.TelemetryProcessor
	store     storage.Provider
}

func newIngestionHook(processor *processing.TelemetryProcessor, store storage.Provider) *IngestionHook {
	return &IngestionHook{processor: processor, store: store}
}

func (h *IngestionHook) ID() string {
	return "datum-ingestion"
}

func (h *IngestionHook) Provides(b byte) bool {
	return bytes.Contains([]byte{
		mqtt.OnPublish,
	}, []byte{b})
}

func (h *IngestionHook) OnPublish(cl *mqtt.Client, pk packets.Packet) (packets.Packet, error) {
	topic := pk.TopicName
	parts := strings.Split(topic, "/")

	// 1. Intercept Telemetry: dev/+/data
	if len(parts) == 3 && parts[0] == "dev" && parts[2] == "data" {
		deviceID := parts[1]
		var data map[string]interface{}
		if err := json.Unmarshal(pk.Payload, &data); err == nil {
			if cl.Net.Remote != "" {
				host, _, _ := net.SplitHostPort(cl.Net.Remote)
				data["public_ip"] = host
			}
			h.processor.Process(deviceID, data)
		}
		return pk, nil
	}

	// 2. Intercept Commands: dev/+/cmd
	if len(parts) == 3 && parts[0] == "dev" && parts[2] == "cmd" {
		deviceID := parts[1]

		var cmd storage.Command
		if err := json.Unmarshal(pk.Payload, &cmd); err == nil {
			// Deduplication: Check if command ID exists
			if cmd.ID == "" {
				// No ID provided, generate one? Or reject?
				// Let's assume external clients might not provide unique IDs.
				// But to be safe, we require an ID or we generate one.
				// For now, let's treat it as a new command.
				cmd.ID = fmt.Sprintf("cmd_%d", time.Now().UnixNano())
			}

			// Check if exists
			existing, _ := h.store.GetCommand(cmd.ID)
			if existing != nil && existing.ID == cmd.ID {
				// Command already exists (likely sent by API -> MQTT), ignore DB write
				return pk, nil
			}

			// New Command from External Source
			cmd.DeviceID = deviceID
			cmd.Status = "pending"
			cmd.CreatedAt = time.Now()

			// Save to DB so HTTP devices can poll it
			if err := h.store.CreateCommand(&cmd); err != nil {
				log.Printf("Error persisting MQTT command: %v", err)
			}
		}
		return pk, nil
	}

	return pk, nil
}
