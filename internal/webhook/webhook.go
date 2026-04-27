package webhook

import (
	"bytes"
	"encoding/json"
	"net/http"
	"sync"
	"time"

	"datum-go/internal/logger"
)

// EventType defines the kind of event.
type EventType string

const (
	EventDeviceCreated     EventType = "device.created"
	EventDeviceDeleted     EventType = "device.deleted"
	EventDeviceOnline      EventType = "device.online"
	EventDeviceOffline     EventType = "device.offline"
	EventDataReceived      EventType = "data.received"
	EventCommandSent       EventType = "command.sent"
	EventCommandAcked      EventType = "command.acknowledged"
	EventAlertTriggered    EventType = "alert.triggered"
	EventProvisionComplete EventType = "provisioning.complete"
	EventRuleTriggered     EventType = "rule.triggered"
)

// Event is sent to all registered webhook endpoints.
type Event struct {
	ID        string                 `json:"id"`
	Type      EventType              `json:"type"`
	Timestamp time.Time              `json:"timestamp"`
	DeviceID  string                 `json:"device_id,omitempty"`
	UserID    string                 `json:"user_id,omitempty"`
	Data      map[string]interface{} `json:"data,omitempty"`
}

// Subscription represents a registered webhook endpoint.
type Subscription struct {
	ID        string      `json:"id"`
	URL       string      `json:"url"`
	Secret    string      `json:"secret,omitempty"`
	Events    []EventType `json:"events"`
	Active    bool        `json:"active"`
	CreatedAt time.Time   `json:"created_at"`
}

// Dispatcher manages webhook subscriptions and event delivery.
type Dispatcher struct {
	mu            sync.RWMutex
	subscriptions map[string]*Subscription
	client        *http.Client
	eventCh       chan Event
	quit          chan struct{}
}

// NewDispatcher creates a new webhook dispatcher and starts the delivery worker.
func NewDispatcher() *Dispatcher {
	d := &Dispatcher{
		subscriptions: make(map[string]*Subscription),
		client: &http.Client{
			Timeout: 10 * time.Second,
		},
		eventCh: make(chan Event, 1000),
		quit:    make(chan struct{}),
	}
	go d.worker()
	return d
}

// Subscribe registers a new webhook endpoint.
func (d *Dispatcher) Subscribe(sub *Subscription) {
	d.mu.Lock()
	defer d.mu.Unlock()
	sub.Active = true
	sub.CreatedAt = time.Now()
	d.subscriptions[sub.ID] = sub
}

// Unsubscribe removes a webhook endpoint.
func (d *Dispatcher) Unsubscribe(id string) {
	d.mu.Lock()
	defer d.mu.Unlock()
	delete(d.subscriptions, id)
}

// List returns all active subscriptions.
func (d *Dispatcher) List() []*Subscription {
	d.mu.RLock()
	defer d.mu.RUnlock()
	out := make([]*Subscription, 0, len(d.subscriptions))
	for _, s := range d.subscriptions {
		out = append(out, s)
	}
	return out
}

// Emit queues an event for delivery to matching subscriptions.
func (d *Dispatcher) Emit(evt Event) {
	evt.Timestamp = time.Now()
	select {
	case d.eventCh <- evt:
	default:
		logger.GetLogger().Warn().Str("event_type", string(evt.Type)).Msg("webhook event dropped (queue full)")
	}
}

// Close shuts down the dispatcher.
func (d *Dispatcher) Close() {
	close(d.quit)
}

func (d *Dispatcher) worker() {
	for {
		select {
		case evt, ok := <-d.eventCh:
			if !ok {
				return
			}
			d.deliver(evt)
		case <-d.quit:
			return
		}
	}
}

func (d *Dispatcher) deliver(evt Event) {
	d.mu.RLock()
	targets := make([]*Subscription, 0)
	for _, sub := range d.subscriptions {
		if !sub.Active {
			continue
		}
		for _, t := range sub.Events {
			if t == evt.Type || t == "*" {
				targets = append(targets, sub)
				break
			}
		}
	}
	d.mu.RUnlock()

	body, err := json.Marshal(evt)
	if err != nil {
		return
	}

	for _, sub := range targets {
		go d.send(sub, body)
	}
}

func (d *Dispatcher) send(sub *Subscription, body []byte) {
	log := logger.GetLogger()

	req, err := http.NewRequest("POST", sub.URL, bytes.NewReader(body))
	if err != nil {
		log.Error().Err(err).Str("webhook_id", sub.ID).Msg("webhook: failed to create request")
		return
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-Datum-Event", "webhook")
	if sub.Secret != "" {
		req.Header.Set("X-Webhook-Secret", sub.Secret)
	}

	resp, err := d.client.Do(req)
	if err != nil {
		log.Warn().Err(err).Str("webhook_id", sub.ID).Str("url", sub.URL).Msg("webhook: delivery failed")
		return
	}
	resp.Body.Close()

	if resp.StatusCode >= 400 {
		log.Warn().Int("status", resp.StatusCode).Str("webhook_id", sub.ID).Msg("webhook: endpoint returned error")
	}
}
