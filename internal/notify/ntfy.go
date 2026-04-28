// Package notify provides push notification via ntfy (https://ntfy.sh).
// ntfy is a self-hostable, open-source HTTP-based pub-sub notification service.
// datum-server publishes events to per-user topics; mobile apps subscribe via the ntfy app or SDK.
//
// Configuration env vars:
//
//	NTFY_URL   - ntfy server URL (e.g. https://ntfy.sh or https://ntfy.yourdomain.com)
//	NTFY_TOKEN - optional bearer token for authenticated ntfy servers
package notify

import (
	"bytes"
	"fmt"
	"net/http"
	"os"
	"time"

	"github.com/rs/zerolog/log"
)

// Priority levels correspond to ntfy priority values.
const (
	PriorityMin     = "min"
	PriorityLow     = "low"
	PriorityDefault = "default"
	PriorityHigh    = "high"
	PriorityMax     = "max"
)

// NtfyClient sends push notifications to an ntfy server.
type NtfyClient struct {
	baseURL string
	token   string
	client  *http.Client
}

// NewNtfyClient creates a new ntfy client from environment variables.
// Returns nil if NTFY_URL is not set (push notifications disabled).
func NewNtfyClient() *NtfyClient {
	baseURL := os.Getenv("NTFY_URL")
	if baseURL == "" {
		log.Info().Msg("Push notifications disabled (NTFY_URL not set). Set NTFY_URL to enable.")
		return nil
	}
	token := os.Getenv("NTFY_TOKEN")
	log.Info().Str("url", baseURL).Msg("ntfy push notifications enabled")
	return &NtfyClient{
		baseURL: baseURL,
		token:   token,
		client:  &http.Client{Timeout: 10 * time.Second},
	}
}

// Send publishes a notification to a specific ntfy topic.
// topic is the channel identifier (e.g. "datum-usr_abc123").
// Returns an error only for logging purposes — callers should fire-and-forget via goroutine.
func (n *NtfyClient) Send(topic, title, message, priority string) error {
	if n == nil {
		return nil
	}

	url := fmt.Sprintf("%s/%s", n.baseURL, topic)
	req, err := http.NewRequest("POST", url, bytes.NewBufferString(message))
	if err != nil {
		return fmt.Errorf("ntfy: failed to create request: %w", err)
	}

	req.Header.Set("Title", title)
	if priority != "" && priority != PriorityDefault {
		req.Header.Set("Priority", priority)
	}
	req.Header.Set("Content-Type", "text/plain")
	if n.token != "" {
		req.Header.Set("Authorization", "Bearer "+n.token)
	}

	resp, err := n.client.Do(req)
	if err != nil {
		return fmt.Errorf("ntfy: request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode >= 400 {
		return fmt.Errorf("ntfy: server returned %d", resp.StatusCode)
	}
	return nil
}

// SendToUser publishes a notification to a user's default datum topic.
// The topic is derived from the user ID — stable and consistent.
func (n *NtfyClient) SendToUser(userID, title, message, priority string) {
	if n == nil {
		return
	}
	topic := "datum-" + userID
	go func() {
		if err := n.Send(topic, title, message, priority); err != nil {
			log.Warn().Err(err).Str("user_id", userID).Msg("Failed to send ntfy notification")
		}
	}()
}
