package mqtt

import (
	"bytes"
	"encoding/json"
	"fmt"
	"log"
	"net"
	"strings"

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
	if err := b.server.AddHook(newIngestionHook(b.processor), nil); err != nil {
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
	topic := fmt.Sprintf("cmd/%s", deviceID)
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
	// Skip auth for internal/admin if needed, but for now enforce device auth.
	// Username = DeviceID, Password = Token (or "key" if legacy API keys)

	deviceID := string(pk.Connect.Username)
	token := string(pk.Connect.Password)

	// If no credentials, reject
	if deviceID == "" {
		return false
	}

	// Validate against DB
	// We use GetDeviceByToken (assuming token flow) OR GetDeviceByAPIKey
	// To simplify, let's try token first, then API key.
	// Actually, `GetDeviceByToken` logic might need to be implemented or we reuse existing flow.
	// For now, let's assume the user passes the API Key (sk_...) as password.

	// If the password starts with "dk_", it's a dynamic token.
	// If "sk_", it's an API Key.
	// Implementation detail: `handlers_auth` handles these. We should reuse logic if possible.
	// But `storage` interface has `GetDeviceByAPIKey`.

	// Let's support API Key for now as it's simplest.
	// TODO: Support Dynamic Tokens (dk_) by verifying JWT signature.

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

	deviceID := cl.ID // Authenticated as DeviceID

	// Split topic
	parts := strings.Split(topic, "/")
	if len(parts) < 2 {
		return false
	}

	prefix := parts[0]
	targetID := parts[1]

	// Must match own ID
	if targetID != deviceID {
		return false
	}

	if write {
		// Can only write to data/{id}
		return prefix == "data"
	} else {
		// Can only read from cmd/{id}
		return prefix == "cmd"
	}
}

// -----------------------------------------------------------------------------
// Ingestion Hook
// -----------------------------------------------------------------------------

type IngestionHook struct {
	mqtt.HookBase
	processor *processing.TelemetryProcessor
}

func newIngestionHook(processor *processing.TelemetryProcessor) *IngestionHook {
	return &IngestionHook{processor: processor}
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
	// Intercept "data/+" messages
	topic := pk.TopicName
	if strings.HasPrefix(topic, "data/") {
		parts := strings.Split(topic, "/")
		if len(parts) >= 2 {
			deviceID := parts[1]

			// Parse JSON payload
			var data map[string]interface{}
			if err := json.Unmarshal(pk.Payload, &data); err == nil {
				// Process data
				// Use Client's IP for enrichment
				ip, _, _ := net.SplitHostPort(cl.Net.Remote)

				// --- ENRICHMENT START ---
				// Inject public_ip into the payload so subscribers (Mobile App) see it
				if ip != "" {
					// Only override if client didn't send it (or sent empty)
					if existing, ok := data["public_ip"]; !ok || existing == "" {
						data["public_ip"] = ip
					}
				}

				// Re-marshal to update the packet payload
				if enrichedPayload, err := json.Marshal(data); err == nil {
					pk.Payload = enrichedPayload
				}
				// --- ENRICHMENT END ---

				// Send to processor (storage)
				// Processor will also see the "public_ip" now since it's in data
				h.processor.Process(deviceID, data, ip)
			}
		}
	}

	// Allow the message to continue (e.g. to subscribers)
	// Now subscribers get the enriched JSON with public_ip!
	return pk, nil
}
