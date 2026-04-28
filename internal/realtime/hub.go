// Package realtime provides a transport-agnostic pub/sub fabric used by the
// SSE, MQTT and WebSocket layers. It deliberately does NOT implement
// long-lived transports itself — those layers subscribe to topics and stream
// messages out via their own protocol (gin SSE, mochi-mqtt, gorilla/ws).
//
// Use cases:
//
//   - REST handler publishes "user/{id}/notifications" → SSE & WS subscribers
//     fanned out without coupling to the broker.
//   - Bucket handler publishes "bucket/{name}/object/put" — both MQTT broker
//     and any realtime SSE listener pick it up.
//
// The implementation is intentionally minimal: in-memory, single-process,
// non-persistent. Multi-node deployments should bridge through MQTT or NATS.
package realtime

import (
	"context"
	"sync"
	"sync/atomic"
)

// Message is a single envelope dispatched on a topic.
type Message struct {
	Topic   string
	Payload []byte
	// Meta is an optional bag of headers (e.g. "user_id", "device_id").
	Meta map[string]string
}

// Subscription represents an active subscriber. Close it to unsubscribe.
type Subscription struct {
	id    uint64
	topic string
	ch    chan Message
	hub   *Hub
	once  sync.Once
}

// C returns the receive-only channel.
func (s *Subscription) C() <-chan Message { return s.ch }

// Close releases the subscription. Safe to call multiple times.
func (s *Subscription) Close() {
	s.once.Do(func() {
		s.hub.unsubscribe(s.topic, s.id)
		close(s.ch)
	})
}

// Hub is the in-memory broker.
type Hub struct {
	mu   sync.RWMutex
	subs map[string]map[uint64]*Subscription // topic → id → sub
	next atomic.Uint64
	// BufferSize is the per-subscriber channel buffer. Defaults to 16.
	BufferSize int
}

// NewHub constructs an empty Hub.
func NewHub() *Hub {
	return &Hub{
		subs:       make(map[string]map[uint64]*Subscription),
		BufferSize: 16,
	}
}

// Subscribe registers interest in a topic. The returned Subscription must be
// Close()d to free resources.
func (h *Hub) Subscribe(topic string) *Subscription {
	id := h.next.Add(1)
	sub := &Subscription{
		id:    id,
		topic: topic,
		ch:    make(chan Message, h.BufferSize),
		hub:   h,
	}
	h.mu.Lock()
	if _, ok := h.subs[topic]; !ok {
		h.subs[topic] = make(map[uint64]*Subscription)
	}
	h.subs[topic][id] = sub
	h.mu.Unlock()
	return sub
}

// Publish fans out the message to every subscriber of the topic. Slow
// subscribers (whose buffers are full) are skipped — they will lose this
// message but stay subscribed. This keeps publishers non-blocking.
func (h *Hub) Publish(topic string, payload []byte, meta map[string]string) int {
	h.mu.RLock()
	dests := h.subs[topic]
	delivered := 0
	msg := Message{Topic: topic, Payload: payload, Meta: meta}
	for _, s := range dests {
		select {
		case s.ch <- msg:
			delivered++
		default:
			// drop — subscriber too slow
		}
	}
	h.mu.RUnlock()
	return delivered
}

// PublishContext is like Publish but respects ctx cancellation per-subscriber.
func (h *Hub) PublishContext(ctx context.Context, topic string, payload []byte, meta map[string]string) int {
	h.mu.RLock()
	dests := h.subs[topic]
	delivered := 0
	msg := Message{Topic: topic, Payload: payload, Meta: meta}
	for _, s := range dests {
		select {
		case <-ctx.Done():
			h.mu.RUnlock()
			return delivered
		case s.ch <- msg:
			delivered++
		default:
			// drop
		}
	}
	h.mu.RUnlock()
	return delivered
}

// Topics returns a snapshot of currently-subscribed topic names.
func (h *Hub) Topics() []string {
	h.mu.RLock()
	defer h.mu.RUnlock()
	out := make([]string, 0, len(h.subs))
	for t := range h.subs {
		out = append(out, t)
	}
	return out
}

func (h *Hub) unsubscribe(topic string, id uint64) {
	h.mu.Lock()
	if m, ok := h.subs[topic]; ok {
		delete(m, id)
		if len(m) == 0 {
			delete(h.subs, topic)
		}
	}
	h.mu.Unlock()
}
