package webhook

import (
	"bytes"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"math/rand"
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

	// Webhook v2 signing scheme:
	//   X-Datum-Timestamp: <unix-seconds>
	//   X-Datum-Signature: sha256=<hex(hmac_sha256(secret, "<ts>.<body>"))>
	//   X-Datum-Event: webhook
	//
	// Replay protection: receivers should reject timestamps older than ~5
	// minutes. The legacy X-Webhook-Secret header is still emitted for
	// back-compat with v1 receivers.
	ts := time.Now().Unix()
	signature := ""
	if sub.Secret != "" {
		h := hmac.New(sha256.New, []byte(sub.Secret))
		fmt.Fprintf(h, "%d.", ts)
		h.Write(body)
		signature = "sha256=" + hex.EncodeToString(h.Sum(nil))
	}

	const maxAttempts = 5
	// Exponential backoff with jitter: 0s, 1s, 2s, 4s, 8s (+/- 25%).
	delay := func(attempt int) time.Duration {
		if attempt == 0 {
			return 0
		}
		base := time.Duration(1<<uint(attempt-1)) * time.Second
		jitter := time.Duration(rand.Int63n(int64(base) / 2)) //nolint:gosec // jitter only
		return base - base/4 + jitter
	}

	for attempt := 0; attempt < maxAttempts; attempt++ {
		if d := delay(attempt); d > 0 {
			time.Sleep(d)
		}
		req, err := http.NewRequest("POST", sub.URL, bytes.NewReader(body))
		if err != nil {
			log.Error().Err(err).Str("webhook_id", sub.ID).Msg("webhook: failed to create request")
			return
		}
		req.Header.Set("Content-Type", "application/json")
		req.Header.Set("X-Datum-Event", "webhook")
		req.Header.Set("X-Datum-Timestamp", fmt.Sprintf("%d", ts))
		if signature != "" {
			req.Header.Set("X-Datum-Signature", signature)
			req.Header.Set("X-Webhook-Secret", sub.Secret) // back-compat
		}
		resp, err := d.client.Do(req)
		if err != nil {
			log.Warn().Err(err).
				Str("webhook_id", sub.ID).
				Str("url", sub.URL).
				Int("attempt", attempt+1).
				Msg("webhook: delivery failed")
			continue
		}
		resp.Body.Close()
		if resp.StatusCode < 300 {
			return // success
		}
		// 4xx (except 408/429) is non-retryable.
		if resp.StatusCode >= 400 && resp.StatusCode < 500 &&
			resp.StatusCode != http.StatusRequestTimeout &&
			resp.StatusCode != http.StatusTooManyRequests {
			log.Warn().Int("status", resp.StatusCode).Str("webhook_id", sub.ID).Msg("webhook: non-retryable error")
			return
		}
		log.Warn().Int("status", resp.StatusCode).Int("attempt", attempt+1).Str("webhook_id", sub.ID).Msg("webhook: retryable error")
	}
	log.Error().Str("webhook_id", sub.ID).Int("attempts", maxAttempts).Msg("webhook: gave up after retries")
}


