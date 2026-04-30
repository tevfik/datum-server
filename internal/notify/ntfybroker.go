// Package notify — embedded ntfy-protocol broker.
//
// This implements a tiny subset of the ntfy.sh HTTP wire protocol so existing
// ntfy mobile/desktop clients can subscribe to topics served directly by
// datum-server, without running a separate ntfy daemon.
//
// Supported endpoints (mounted under a configurable prefix, e.g. /notify):
//
//	POST /{topic}            — publish (text/plain body, optional Title/Priority headers)
//	GET  /{topic}/json       — long-lived stream of newline-delimited JSON messages
//	GET  /{topic}/sse        — Server-Sent Events stream of the same messages
//	GET  /{topic}/raw        — newline-delimited message bodies (no metadata)
//
// What's intentionally NOT supported (yet):
//
//   - Persistent message cache (?since= / ?poll=). Subscribers only see
//     messages published after they connect.
//   - Authentication / ACLs. Bind to localhost or front with a reverse proxy
//     until the policy engine arrives in Phase 5.
//   - Attachments, actions, click URLs.
//
// Design notes:
//
//   - Each topic owns a fan-out hub (slice of subscriber channels). Publish
//     iterates subscribers under a read lock; slow subscribers are dropped
//     after a 1-second send timeout to keep the broker non-blocking.
package notify

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"strings"
	"sync"
	"sync/atomic"
	"time"
)

// NtfyMessage is the JSON shape ntfy clients expect on the /json stream.
type NtfyMessage struct {
	ID       string   `json:"id"`
	Time     int64    `json:"time"`
	Event    string   `json:"event"` // "message" | "open" | "keepalive"
	Topic    string   `json:"topic"`
	Title    string   `json:"title,omitempty"`
	Message  string   `json:"message,omitempty"`
	Priority int      `json:"priority,omitempty"`
	Tags     []string `json:"tags,omitempty"`
}

// NtfyBroker is an in-process ntfy-protocol pub/sub server.
type NtfyBroker struct {
	mu     sync.RWMutex
	topics map[string]*ntfyTopic
	idGen  uint64
}

type ntfyTopic struct {
	mu   sync.Mutex
	subs []chan NtfyMessage
}

// NewNtfyBroker returns an empty broker ready to be mounted on a router.
func NewNtfyBroker() *NtfyBroker {
	return &NtfyBroker{topics: map[string]*ntfyTopic{}}
}

func (b *NtfyBroker) topic(name string) *ntfyTopic {
	b.mu.RLock()
	t, ok := b.topics[name]
	b.mu.RUnlock()
	if ok {
		return t
	}
	b.mu.Lock()
	defer b.mu.Unlock()
	if t, ok := b.topics[name]; ok {
		return t
	}
	t = &ntfyTopic{}
	b.topics[name] = t
	return t
}

// Publish fans out a message to every current subscriber of topic. It never
// blocks: subscribers whose buffers are full simply miss the message.
func (b *NtfyBroker) Publish(topic, title, message, priority string, tags []string) NtfyMessage {
	id := atomic.AddUint64(&b.idGen, 1)
	msg := NtfyMessage{
		ID:       fmt.Sprintf("ntfy_%d", id),
		Time:     time.Now().Unix(),
		Event:    "message",
		Topic:    topic,
		Title:    title,
		Message:  message,
		Priority: priorityToInt(priority),
		Tags:     tags,
	}
	t := b.topic(topic)
	t.mu.Lock()
	subs := append([]chan NtfyMessage(nil), t.subs...)
	t.mu.Unlock()
	for _, ch := range subs {
		select {
		case ch <- msg:
		case <-time.After(1 * time.Second):
			// drop slow subscriber for this message
		}
	}
	return msg
}

// Subscribe returns a channel of messages and a cancel func. The caller MUST
// invoke cancel when finished to free the slot in the topic's sub list.
func (b *NtfyBroker) Subscribe(topic string, buffer int) (<-chan NtfyMessage, func()) {
	if buffer <= 0 {
		buffer = 16
	}
	ch := make(chan NtfyMessage, buffer)
	t := b.topic(topic)
	t.mu.Lock()
	t.subs = append(t.subs, ch)
	t.mu.Unlock()
	cancel := func() {
		t.mu.Lock()
		for i, c := range t.subs {
			if c == ch {
				t.subs = append(t.subs[:i], t.subs[i+1:]...)
				break
			}
		}
		t.mu.Unlock()
		close(ch)
	}
	return ch, cancel
}

// ServeHTTP routes a request matching one of the documented endpoints.
//
// `prefix` is the URL prefix the broker is mounted under (e.g. "/notify").
// Anything after the prefix is interpreted as <topic>[/<format>].
func (b *NtfyBroker) ServeHTTP(prefix string, w http.ResponseWriter, r *http.Request) {
	path := strings.TrimPrefix(r.URL.Path, prefix)
	path = strings.TrimPrefix(path, "/")
	if path == "" {
		http.Error(w, "topic required", http.StatusBadRequest)
		return
	}
	parts := strings.SplitN(path, "/", 2)
	topic := parts[0]
	format := "json"
	if len(parts) == 2 {
		format = parts[1]
	}

	switch r.Method {
	case http.MethodPost, http.MethodPut:
		b.handlePublish(w, r, topic)
	case http.MethodGet:
		switch format {
		case "json", "":
			b.streamJSON(w, r, topic)
		case "sse":
			b.streamSSE(w, r, topic)
		case "raw":
			b.streamRaw(w, r, topic)
		default:
			http.Error(w, "unknown format: "+format, http.StatusBadRequest)
		}
	default:
		w.Header().Set("Allow", "GET, POST, PUT")
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
	}
}

func (b *NtfyBroker) handlePublish(w http.ResponseWriter, r *http.Request, topic string) {
	body := make([]byte, 0, 1024)
	buf := make([]byte, 1024)
	for {
		n, err := r.Body.Read(buf)
		if n > 0 {
			body = append(body, buf[:n]...)
		}
		if err != nil {
			break
		}
		if len(body) > 4096 {
			http.Error(w, "message too large (max 4 KiB)", http.StatusRequestEntityTooLarge)
			return
		}
	}
	title := r.Header.Get("Title")
	if title == "" {
		title = r.Header.Get("X-Title")
	}
	priority := r.Header.Get("Priority")
	if priority == "" {
		priority = r.Header.Get("X-Priority")
	}
	tagsHeader := r.Header.Get("Tags")
	if tagsHeader == "" {
		tagsHeader = r.Header.Get("X-Tags")
	}
	var tags []string
	if tagsHeader != "" {
		for _, t := range strings.Split(tagsHeader, ",") {
			if v := strings.TrimSpace(t); v != "" {
				tags = append(tags, v)
			}
		}
	}
	msg := b.Publish(topic, title, string(body), priority, tags)
	w.Header().Set("Content-Type", "application/json")
	_ = json.NewEncoder(w).Encode(msg)
}

func (b *NtfyBroker) streamJSON(w http.ResponseWriter, r *http.Request, topic string) {
	flusher, ok := w.(http.Flusher)
	if !ok {
		http.Error(w, "streaming unsupported", http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "application/x-ndjson")
	w.Header().Set("X-Accel-Buffering", "no")

	ch, cancel := b.Subscribe(topic, 32)
	defer cancel()

	enc := json.NewEncoder(w)
	_ = enc.Encode(NtfyMessage{ID: "open", Event: "open", Time: time.Now().Unix(), Topic: topic})
	flusher.Flush()

	keepalive := time.NewTicker(30 * time.Second)
	defer keepalive.Stop()

	for {
		select {
		case <-r.Context().Done():
			return
		case <-keepalive.C:
			_ = enc.Encode(NtfyMessage{ID: "keepalive", Event: "keepalive", Time: time.Now().Unix(), Topic: topic})
			flusher.Flush()
		case msg, ok := <-ch:
			if !ok {
				return
			}
			if err := enc.Encode(msg); err != nil {
				return
			}
			flusher.Flush()
		}
	}
}

func (b *NtfyBroker) streamSSE(w http.ResponseWriter, r *http.Request, topic string) {
	flusher, ok := w.(http.Flusher)
	if !ok {
		http.Error(w, "streaming unsupported", http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "text/event-stream")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")
	w.Header().Set("X-Accel-Buffering", "no")

	ch, cancel := b.Subscribe(topic, 32)
	defer cancel()

	keepalive := time.NewTicker(30 * time.Second)
	defer keepalive.Stop()

	for {
		select {
		case <-r.Context().Done():
			return
		case <-keepalive.C:
			fmt.Fprintf(w, ": keepalive\n\n")
			flusher.Flush()
		case msg, ok := <-ch:
			if !ok {
				return
			}
			data, _ := json.Marshal(msg)
			fmt.Fprintf(w, "event: message\nid: %s\ndata: %s\n\n", msg.ID, data)
			flusher.Flush()
		}
	}
}

func (b *NtfyBroker) streamRaw(w http.ResponseWriter, r *http.Request, topic string) {
	flusher, ok := w.(http.Flusher)
	if !ok {
		http.Error(w, "streaming unsupported", http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "text/plain; charset=utf-8")
	w.Header().Set("X-Accel-Buffering", "no")

	ch, cancel := b.Subscribe(topic, 32)
	defer cancel()

	for {
		select {
		case <-r.Context().Done():
			return
		case msg, ok := <-ch:
			if !ok {
				return
			}
			fmt.Fprintln(w, msg.Message)
			flusher.Flush()
		}
	}
}

// EmbeddedNtfyChannel is a Channel that publishes into an in-process
// NtfyBroker (rather than an external ntfy server). Useful when you don't
// want to run a separate daemon.
type EmbeddedNtfyChannel struct {
	Broker *NtfyBroker
	// TopicFor maps userID -> broker topic.
	TopicFor func(userID string) string
}

// NewEmbeddedNtfyChannel wires a broker into the dispatcher.
func NewEmbeddedNtfyChannel(b *NtfyBroker) *EmbeddedNtfyChannel {
	return &EmbeddedNtfyChannel{Broker: b, TopicFor: defaultNtfyTopic}
}

// Name returns the channel name. Two channels can co-exist (external "ntfy"
// and embedded "ntfy-embedded"); main.go decides which one(s) to register.
func (EmbeddedNtfyChannel) Name() string { return "ntfy-embedded" }

func (c *EmbeddedNtfyChannel) Send(_ context.Context, n Notification) error {
	if c.Broker == nil {
		return nil
	}
	c.Broker.Publish(c.TopicFor(n.UserID), n.Title, n.Message, n.Priority, nil)
	return nil
}

func priorityToInt(s string) int {
	switch strings.ToLower(strings.TrimSpace(s)) {
	case "min", "1":
		return 1
	case "low", "2":
		return 2
	case "default", "3", "":
		return 3
	case "high", "4":
		return 4
	case "max", "urgent", "5":
		return 5
	default:
		return 3
	}
}
