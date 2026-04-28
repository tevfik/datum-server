package notify

import (
	"time"

	"datum-go/internal/storage"

	"github.com/google/uuid"
	"github.com/rs/zerolog/log"
)

// Dispatcher delivers notifications to a user through two channels:
//
//  1. SSE / MQTT command stream — phones registered as devices with type "mobile"
//     receive a "notify" command on their /dev/:device_id/cmd/stream endpoint.
//     This works whenever the mobile app is running in the foreground or background.
//
//  2. ntfy push (fallback) — used when the app is closed or the SSE connection
//     has dropped. The user subscribes to their personal ntfy topic (datum-<userID>)
//     in the ntfy app to receive these even when the app is completely stopped.
//
// By treating each phone as a regular device, datum-server needs no third-party
// push service (APNs/FCM) for when the app is open. ntfy handles the rest.
type Dispatcher struct {
	store storage.Provider
	ntfy  *NtfyClient // may be nil
}

// NewDispatcher creates a Dispatcher.
// ntfy may be nil — in that case only the SSE/MQTT channel is used.
func NewDispatcher(store storage.Provider, ntfy *NtfyClient) *Dispatcher {
	return &Dispatcher{store: store, ntfy: ntfy}
}

// NotifyUser sends a notification to all of a user's mobile devices via SSE
// command dispatch, and also fires an ntfy push for when the app is closed.
//
// title    — short notification title
// message  — notification body
// priority — one of PriorityMin / PriorityLow / PriorityDefault / PriorityHigh / PriorityMax
func (d *Dispatcher) NotifyUser(userID, title, message, priority string) {
	go d.dispatch(userID, title, message, priority)
}

func (d *Dispatcher) dispatch(userID, title, message, priority string) {
	// Channel 1: SSE command to every registered mobile device
	devices, err := d.store.GetUserDevices(userID)
	if err == nil {
		for _, dev := range devices {
			if dev.Type != "mobile" || dev.Status != "active" {
				continue
			}
			cmd := &storage.Command{
				ID:       uuid.New().String(),
				DeviceID: dev.ID,
				Action:   "notify",
				Params: map[string]interface{}{
					"title":    title,
					"message":  message,
					"priority": priority,
				},
				Status:    "pending",
				CreatedAt: time.Now(),
				ExpiresAt: time.Now().Add(72 * time.Hour), // survive app restarts
			}
			if err := d.store.CreateCommand(cmd); err != nil {
				log.Warn().Err(err).Str("device_id", dev.ID).Msg("Failed to queue notify command for mobile device")
			}
		}
	} else {
		log.Warn().Err(err).Str("user_id", userID).Msg("Failed to fetch user devices for notification")
	}

	// Channel 2: ntfy push (works when app is closed)
	if d.ntfy != nil {
		if err := d.ntfy.Send("datum-"+userID, title, message, priority); err != nil {
			log.Warn().Err(err).Str("user_id", userID).Msg("Failed to send ntfy notification")
		}
	}
}
