// Package notify — Channels.
//
// Concrete Channel implementations live in their own files in this package
// so they can be added/removed independently of the dispatcher core.
//
// Currently shipped:
//   - InAppChannel   (SSE/MQTT command for "mobile" devices)
//   - NtfyChannel    (push to an external/internal ntfy-protocol server)
//   - WebPushChannel (VAPID Web Push, browsers — stub until Phase 5)
//
// To add a new channel, implement Channel and register it from main.go:
//
//	dispatcher.Register(myCustomChannel{})

package notify

import (
	"context"
	"time"

	"datum-go/internal/storage"

	"github.com/google/uuid"
)

// InAppChannel delivers a notification by enqueueing a "notify" command for
// every device of type "mobile" the user owns. Mobile clients pick it up via
// their existing SSE/MQTT command stream — no external push service needed.
type InAppChannel struct {
	store storage.Provider
}

// NewInAppChannel constructs an in-app/SSE channel backed by the given store.
func NewInAppChannel(store storage.Provider) *InAppChannel { return &InAppChannel{store: store} }

func (InAppChannel) Name() string { return "inapp" }

func (c *InAppChannel) Send(ctx context.Context, n Notification) error {
	devices, err := c.store.GetUserDevices(n.UserID)
	if err != nil {
		return err
	}
	for _, dev := range devices {
		if dev.Type != "mobile" || dev.Status != "active" {
			continue
		}
		params := map[string]interface{}{
			"title":    n.Title,
			"message":  n.Message,
			"priority": n.Priority,
		}
		for k, v := range n.Tags {
			params[k] = v
		}
		cmd := &storage.Command{
			ID:        uuid.New().String(),
			DeviceID:  dev.ID,
			Action:    "notify",
			Params:    params,
			Status:    "pending",
			CreatedAt: time.Now(),
			ExpiresAt: time.Now().Add(72 * time.Hour),
		}
		if err := c.store.CreateCommand(cmd); err != nil {
			return err
		}
		// Honour cancellation between devices.
		if ctx.Err() != nil {
			return ctx.Err()
		}
	}
	return nil
}

// NtfyChannel wraps an *NtfyClient and exposes it as a Channel.
type NtfyChannel struct {
	client *NtfyClient
	// TopicFor maps a userID -> topic. Override to use stored per-user secrets.
	TopicFor func(userID string) string
}

// NewNtfyChannel wraps client. If client is nil, Send is a no-op.
func NewNtfyChannel(client *NtfyClient) *NtfyChannel {
	return &NtfyChannel{client: client, TopicFor: defaultNtfyTopic}
}

func (NtfyChannel) Name() string { return "ntfy" }

func (c *NtfyChannel) Send(ctx context.Context, n Notification) error {
	if c.client == nil {
		return nil
	}
	topic := c.TopicFor(n.UserID)
	return c.client.Send(topic, n.Title, n.Message, n.Priority)
}

func defaultNtfyTopic(userID string) string { return "datum-" + userID }

// WebPushChannel is a placeholder for VAPID Web Push (browser native push,
// no FCM dependency). Phase 5 will wire it into stored push subscriptions.
type WebPushChannel struct {
	VAPIDPublic  string
	VAPIDPrivate string
	Subject      string
}

// NewWebPushChannel returns a stub channel that just logs successful sends.
// Returns nil when VAPID keys are missing so it is not registered.
func NewWebPushChannel(public, private, subject string) *WebPushChannel {
	if public == "" || private == "" {
		return nil
	}
	return &WebPushChannel{VAPIDPublic: public, VAPIDPrivate: private, Subject: subject}
}

func (WebPushChannel) Name() string { return "webpush" }

func (c *WebPushChannel) Send(ctx context.Context, n Notification) error {
	// Phase 5 will implement actual Web Push delivery using stored
	// subscription endpoints. For now this is intentionally a no-op so the
	// channel can be registered without breaking call sites.
	return nil
}
