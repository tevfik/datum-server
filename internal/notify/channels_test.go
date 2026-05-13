package notify

import (
	"context"
	"errors"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"
	"time"

	"datum-go/internal/storage"
)

// ── Storage mock for InAppChannel ─────────────────────────────────────────────

type mockChannelStore struct {
	storage.Provider // embed to satisfy interface (nil for unused methods)
	mu               sync.Mutex
	devices          []storage.Device
	commands         []*storage.Command
	devErr           error
	cmdErr           error
}

func (m *mockChannelStore) GetUserDevices(userID string) ([]storage.Device, error) {
	return m.devices, m.devErr
}

func (m *mockChannelStore) CreateCommand(cmd *storage.Command) error {
	if m.cmdErr != nil {
		return m.cmdErr
	}
	m.mu.Lock()
	defer m.mu.Unlock()
	m.commands = append(m.commands, cmd)
	return nil
}

// ── InAppChannel ─────────────────────────────────────────────────────────────

func TestNewInAppChannel(t *testing.T) {
	store := &mockChannelStore{}
	ch := NewInAppChannel(store)
	if ch == nil {
		t.Fatal("NewInAppChannel returned nil")
	}
	if ch.Name() != "inapp" {
		t.Fatalf("expected name 'inapp', got %q", ch.Name())
	}
}

func TestInAppChannel_Send_NoDevices(t *testing.T) {
	store := &mockChannelStore{devices: nil}
	ch := NewInAppChannel(store)
	if err := ch.Send(context.Background(), Notification{UserID: "u1", Title: "hi", Message: "bye"}); err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(store.commands) != 0 {
		t.Fatal("expected no commands created when no devices")
	}
}

func TestInAppChannel_Send_MobileActiveDevice(t *testing.T) {
	store := &mockChannelStore{
		devices: []storage.Device{
			{ID: "mob-1", Type: "mobile", Status: "active"},
			{ID: "desk-1", Type: "desktop", Status: "active"}, // should be skipped
			{ID: "mob-2", Type: "mobile", Status: "inactive"}, // should be skipped
		},
	}
	ch := NewInAppChannel(store)
	n := Notification{UserID: "u1", Title: "Alert", Message: "Foo", Priority: PriorityHigh, Tags: map[string]string{"tag": "val"}}
	if err := ch.Send(context.Background(), n); err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(store.commands) != 1 {
		t.Fatalf("expected 1 command (only mobile+active), got %d", len(store.commands))
	}
	cmd := store.commands[0]
	if cmd.DeviceID != "mob-1" {
		t.Fatalf("expected command for mob-1, got %s", cmd.DeviceID)
	}
	if cmd.Action != "notify" {
		t.Fatalf("expected action 'notify', got %s", cmd.Action)
	}
	if cmd.Params["title"] != "Alert" {
		t.Fatalf("expected title 'Alert' in params, got %v", cmd.Params["title"])
	}
}

func TestInAppChannel_Send_GetDevicesError(t *testing.T) {
	store := &mockChannelStore{devErr: errors.New("db error")}
	ch := NewInAppChannel(store)
	if err := ch.Send(context.Background(), Notification{UserID: "u1"}); err == nil {
		t.Fatal("expected error from GetUserDevices")
	}
}

func TestInAppChannel_Send_CreateCommandError(t *testing.T) {
	store := &mockChannelStore{
		devices: []storage.Device{{ID: "mob-1", Type: "mobile", Status: "active"}},
		cmdErr:  errors.New("write error"),
	}
	ch := NewInAppChannel(store)
	if err := ch.Send(context.Background(), Notification{UserID: "u1"}); err == nil {
		t.Fatal("expected error from CreateCommand")
	}
}

func TestInAppChannel_Send_ContextCancelled(t *testing.T) {
	store := &mockChannelStore{
		devices: []storage.Device{
			{ID: "mob-1", Type: "mobile", Status: "active"},
			{ID: "mob-2", Type: "mobile", Status: "active"},
		},
	}
	ch := NewInAppChannel(store)

	ctx, cancel := context.WithCancel(context.Background())
	cancel() // cancel immediately

	// Should short-circuit after first successful command
	err := ch.Send(ctx, Notification{UserID: "u1", Title: "t", Message: "m"})
	if !errors.Is(err, context.Canceled) {
		t.Fatalf("expected context.Canceled, got %v", err)
	}
}

// ── NtfyChannel ───────────────────────────────────────────────────────────────

func TestNewNtfyChannel_Name(t *testing.T) {
	ch := NewNtfyChannel(nil)
	if ch.Name() != "ntfy" {
		t.Fatalf("expected 'ntfy', got %q", ch.Name())
	}
}

func TestNtfyChannel_Send_NilClient(t *testing.T) {
	ch := NewNtfyChannel(nil)
	if err := ch.Send(context.Background(), Notification{UserID: "u1", Title: "t", Message: "m"}); err != nil {
		t.Fatalf("nil client Send should be a no-op, got error: %v", err)
	}
}

func TestNtfyChannel_Send_WithClient(t *testing.T) {
	received := make(chan struct{}, 1)
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		received <- struct{}{}
		w.WriteHeader(http.StatusOK)
	}))
	defer srv.Close()

	client := &NtfyClient{baseURL: srv.URL, client: &http.Client{Timeout: 5 * time.Second}}
	ch := NewNtfyChannel(client)

	if err := ch.Send(context.Background(), Notification{UserID: "u1", Title: "t", Message: "m", Priority: PriorityHigh}); err != nil {
		t.Fatalf("unexpected error: %v", err)
	}

	select {
	case <-received:
	case <-time.After(2 * time.Second):
		t.Fatal("ntfy server never received request")
	}
}

func TestNtfyChannel_CustomTopicFor(t *testing.T) {
	received := make(chan string, 1)
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		received <- r.URL.Path
		w.WriteHeader(http.StatusOK)
	}))
	defer srv.Close()

	client := &NtfyClient{baseURL: srv.URL, client: &http.Client{Timeout: 5 * time.Second}}
	ch := NewNtfyChannel(client)
	ch.TopicFor = func(userID string) string { return "custom-" + userID }

	ch.Send(context.Background(), Notification{UserID: "alice", Title: "t", Message: "m"})

	select {
	case path := <-received:
		if !strings.HasSuffix(path, "/custom-alice") {
			t.Fatalf("expected topic 'custom-alice', got path %q", path)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("ntfy server never received request")
	}
}

func TestDefaultNtfyTopic(t *testing.T) {
	topic := defaultNtfyTopic("user123")
	if topic != "datum-user123" {
		t.Fatalf("expected 'datum-user123', got %q", topic)
	}
}

// ── WebPushChannel ────────────────────────────────────────────────────────────

func TestNewWebPushChannel_NilWhenMissingKeys(t *testing.T) {
	if NewWebPushChannel("", "", "") != nil {
		t.Fatal("expected nil when public key missing")
	}
	if NewWebPushChannel("pub", "", "") != nil {
		t.Fatal("expected nil when private key missing")
	}
}

func TestNewWebPushChannel_ValidKeys(t *testing.T) {
	ch := NewWebPushChannel("pub", "priv", "mailto:admin@example.com")
	if ch == nil {
		t.Fatal("expected non-nil channel with valid keys")
	}
	if ch.Name() != "webpush" {
		t.Fatalf("expected 'webpush', got %q", ch.Name())
	}
}

func TestWebPushChannel_Send_Noop(t *testing.T) {
	ch := NewWebPushChannel("pub", "priv", "mailto:admin@example.com")
	if err := ch.Send(context.Background(), Notification{UserID: "u1"}); err != nil {
		t.Fatalf("WebPushChannel.Send should be a no-op, got: %v", err)
	}
}

// ── NtfyClient ────────────────────────────────────────────────────────────────

func TestNtfyClient_Send_Success(t *testing.T) {
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Header.Get("Title") != "MyTitle" {
			t.Errorf("expected Title header 'MyTitle', got %q", r.Header.Get("Title"))
		}
		w.WriteHeader(http.StatusOK)
	}))
	defer srv.Close()

	client := &NtfyClient{baseURL: srv.URL, client: &http.Client{Timeout: 5 * time.Second}}
	if err := client.Send("mytopic", "MyTitle", "body", PriorityDefault); err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
}

func TestNtfyClient_Send_WithPriority(t *testing.T) {
	priorityReceived := make(chan string, 1)
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		priorityReceived <- r.Header.Get("Priority")
		w.WriteHeader(http.StatusOK)
	}))
	defer srv.Close()

	client := &NtfyClient{baseURL: srv.URL, client: &http.Client{Timeout: 5 * time.Second}}
	client.Send("topic", "T", "M", PriorityHigh)

	select {
	case p := <-priorityReceived:
		if p != PriorityHigh {
			t.Fatalf("expected priority %q, got %q", PriorityHigh, p)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("server not called")
	}
}

func TestNtfyClient_Send_ServerError(t *testing.T) {
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusForbidden)
	}))
	defer srv.Close()

	client := &NtfyClient{baseURL: srv.URL, client: &http.Client{Timeout: 5 * time.Second}}
	if err := client.Send("topic", "T", "M", ""); err == nil {
		t.Fatal("expected error for 4xx response")
	}
}

func TestNtfyClient_Send_NilReceiver(t *testing.T) {
	var client *NtfyClient
	if err := client.Send("topic", "T", "M", ""); err != nil {
		t.Fatalf("nil receiver Send should be no-op, got: %v", err)
	}
}

func TestNtfyClient_Send_WithToken(t *testing.T) {
	tokenReceived := make(chan string, 1)
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		tokenReceived <- r.Header.Get("Authorization")
		w.WriteHeader(http.StatusOK)
	}))
	defer srv.Close()

	client := &NtfyClient{baseURL: srv.URL, token: "secret-token", client: &http.Client{Timeout: 5 * time.Second}}
	client.Send("topic", "T", "M", "")

	select {
	case auth := <-tokenReceived:
		expected := "Bearer secret-token"
		if auth != expected {
			t.Fatalf("expected auth header %q, got %q", expected, auth)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("server not called")
	}
}

func TestNtfyClient_SendToUser_NilReceiver(t *testing.T) {
	var client *NtfyClient
	// Should not panic
	client.SendToUser("user1", "T", "M", "")
}

func TestNtfyClient_SendToUser_Success(t *testing.T) {
	pathReceived := make(chan string, 1)
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		pathReceived <- r.URL.Path
		w.WriteHeader(http.StatusOK)
	}))
	defer srv.Close()

	client := &NtfyClient{baseURL: srv.URL, client: &http.Client{Timeout: 5 * time.Second}}
	client.SendToUser("user42", "T", "M", "")

	select {
	case path := <-pathReceived:
		if !strings.HasSuffix(path, "/datum-user42") {
			t.Fatalf("expected topic 'datum-user42', got %q", path)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("server not called")
	}
}

func TestNtfyClient_NewNtfyClient_NoURL(t *testing.T) {
	t.Setenv("NTFY_URL", "")
	client := NewNtfyClient()
	if client != nil {
		t.Fatal("expected nil when NTFY_URL not set")
	}
}

func TestNtfyClient_NewNtfyClient_WithURL(t *testing.T) {
	t.Setenv("NTFY_URL", "https://ntfy.example.com")
	t.Setenv("NTFY_TOKEN", "tok123")
	client := NewNtfyClient()
	if client == nil {
		t.Fatal("expected non-nil NtfyClient when NTFY_URL is set")
	}
	if client.baseURL != "https://ntfy.example.com" {
		t.Fatalf("unexpected baseURL: %s", client.baseURL)
	}
	if client.token != "tok123" {
		t.Fatalf("unexpected token: %s", client.token)
	}
}
