// Package notify provides a multi-channel notification dispatcher.
//
// A Notification is fanned out to one or more Channels. Each Channel
// implements a single delivery mechanism — in-app SSE/MQTT command, ntfy
// push, web push (VAPID), email, etc. Channels are pluggable so tests and
// alternate transports can be injected without touching call sites.
//
// The dispatcher itself owns no networking; concrete channels do. The
// public Dispatcher.NotifyUser() helper preserves the historical API used
// by api/auth and rules so existing call sites keep working.
package notify

import (
	"context"
	"sync"
	"time"

	"github.com/rs/zerolog/log"
)

// Priority levels (mirror ntfy values for convenience).
const (
	PriorityMin     = "min"
	PriorityLow     = "low"
	PriorityDefault = "default"
	PriorityHigh    = "high"
	PriorityMax     = "max"
)

// Notification is the channel-agnostic payload delivered to a single user.
type Notification struct {
	UserID   string
	Title    string
	Message  string
	Priority string
	// Tags is an optional bag of channel-specific hints (e.g. "click_url",
	// "icon"). Channels are free to ignore unknown keys.
	Tags map[string]string
	// Channels lists explicit channel names to use. If empty, the dispatcher
	// uses its configured default set.
	Channels []string
}

// Channel is a single notification transport (in-app, ntfy, webpush, ...).
//
// Send must be safe for concurrent use. It should respect ctx for timeouts
// and return quickly; the dispatcher already runs Send in a goroutine.
type Channel interface {
	Name() string
	Send(ctx context.Context, n Notification) error
}

// Dispatcher fans out a Notification to a configured set of Channels.
type Dispatcher struct {
	mu              sync.RWMutex
	channels        map[string]Channel
	defaultChannels []string
}

// NewDispatcher creates an empty dispatcher. Use Register to add channels
// and SetDefaultChannels to choose which fire when Notification.Channels
// is empty.
func NewDispatcher() *Dispatcher {
	return &Dispatcher{channels: map[string]Channel{}}
}

// Register adds (or replaces) a channel by its Name().
func (d *Dispatcher) Register(c Channel) {
	if c == nil {
		return
	}
	d.mu.Lock()
	d.channels[c.Name()] = c
	d.mu.Unlock()
}

// SetDefaultChannels controls which channels fire when a Notification does
// not specify any explicitly. Unknown channel names are silently ignored.
func (d *Dispatcher) SetDefaultChannels(names []string) {
	d.mu.Lock()
	d.defaultChannels = append([]string(nil), names...)
	d.mu.Unlock()
}

// Channels returns the names of all registered channels (sorted-stable order
// is not guaranteed; callers expecting a stable view should sort).
func (d *Dispatcher) Channels() []string {
	d.mu.RLock()
	defer d.mu.RUnlock()
	out := make([]string, 0, len(d.channels))
	for name := range d.channels {
		out = append(out, name)
	}
	return out
}

// Dispatch delivers n to the requested channels asynchronously. Each channel
// runs in its own goroutine with a 10-second context deadline. Errors are
// logged but never block other channels.
func (d *Dispatcher) Dispatch(n Notification) {
	d.mu.RLock()
	names := n.Channels
	if len(names) == 0 {
		names = d.defaultChannels
	}
	targets := make([]Channel, 0, len(names))
	for _, name := range names {
		if ch, ok := d.channels[name]; ok {
			targets = append(targets, ch)
		}
	}
	d.mu.RUnlock()

	if len(targets) == 0 {
		return
	}
	for _, ch := range targets {
		ch := ch
		go func() {
			ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
			defer cancel()
			if err := ch.Send(ctx, n); err != nil {
				log.Warn().
					Err(err).
					Str("channel", ch.Name()).
					Str("user_id", n.UserID).
					Msg("notification channel send failed")
			}
		}()
	}
}

// NotifyUser is a convenience wrapper that mirrors the historical API.
func (d *Dispatcher) NotifyUser(userID, title, message, priority string) {
	d.Dispatch(Notification{
		UserID:   userID,
		Title:    title,
		Message:  message,
		Priority: priority,
	})
}
