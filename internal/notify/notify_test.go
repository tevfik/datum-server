package notify

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

type recordingChannel struct {
	name  string
	calls int32
	last  Notification
	mu    sync.Mutex
	err   error
}

func (r *recordingChannel) Name() string { return r.name }
func (r *recordingChannel) Send(_ context.Context, n Notification) error {
	atomic.AddInt32(&r.calls, 1)
	r.mu.Lock()
	r.last = n
	r.mu.Unlock()
	return r.err
}

func TestDispatcherFanOut(t *testing.T) {
	d := NewDispatcher()
	a := &recordingChannel{name: "a"}
	b := &recordingChannel{name: "b"}
	d.Register(a)
	d.Register(b)
	d.SetDefaultChannels([]string{"a", "b"})

	d.Dispatch(Notification{UserID: "u1", Title: "hi", Message: "m"})

	deadline := time.Now().Add(time.Second)
	for time.Now().Before(deadline) {
		if atomic.LoadInt32(&a.calls) == 1 && atomic.LoadInt32(&b.calls) == 1 {
			return
		}
		time.Sleep(5 * time.Millisecond)
	}
	t.Fatalf("expected both channels to be called, got a=%d b=%d",
		atomic.LoadInt32(&a.calls), atomic.LoadInt32(&b.calls))
}

func TestDispatcherExplicitChannelWinsOverDefault(t *testing.T) {
	d := NewDispatcher()
	a := &recordingChannel{name: "a"}
	b := &recordingChannel{name: "b"}
	d.Register(a)
	d.Register(b)
	d.SetDefaultChannels([]string{"a"})

	d.Dispatch(Notification{UserID: "u1", Channels: []string{"b"}})

	time.Sleep(100 * time.Millisecond)
	if got := atomic.LoadInt32(&a.calls); got != 0 {
		t.Fatalf("expected a not called, got %d", got)
	}
	if got := atomic.LoadInt32(&b.calls); got != 1 {
		t.Fatalf("expected b called once, got %d", got)
	}
}

func TestNtfyBrokerPublishSubscribe(t *testing.T) {
	b := NewNtfyBroker()
	ch, cancel := b.Subscribe("topicA", 4)
	defer cancel()

	go b.Publish("topicA", "Hello", "World", "high", []string{"alarm"})

	select {
	case msg := <-ch:
		if msg.Title != "Hello" || msg.Message != "World" || msg.Topic != "topicA" {
			t.Fatalf("unexpected msg: %+v", msg)
		}
		if msg.Priority != 4 {
			t.Fatalf("expected priority 4, got %d", msg.Priority)
		}
		if len(msg.Tags) != 1 || msg.Tags[0] != "alarm" {
			t.Fatalf("unexpected tags: %v", msg.Tags)
		}
	case <-time.After(time.Second):
		t.Fatal("did not receive message")
	}
}

func TestNtfyBrokerHTTP_PublishAndSubscribeJSON(t *testing.T) {
	b := NewNtfyBroker()
	mux := http.NewServeMux()
	mux.HandleFunc("/ntfy/", func(w http.ResponseWriter, r *http.Request) {
		b.ServeHTTP("/ntfy", w, r)
	})
	srv := httptest.NewServer(mux)
	defer srv.Close()

	// Subscriber: open the JSON stream
	subReady := make(chan struct{})
	subDone := make(chan NtfyMessage, 1)
	go func() {
		req, _ := http.NewRequest("GET", srv.URL+"/ntfy/test/json", nil)
		resp, err := http.DefaultClient.Do(req)
		if err != nil {
			t.Errorf("subscribe: %v", err)
			return
		}
		defer resp.Body.Close()
		dec := json.NewDecoder(resp.Body)
		first := true
		for {
			var msg NtfyMessage
			if err := dec.Decode(&msg); err != nil {
				return
			}
			if first {
				// the open frame
				first = false
				close(subReady)
				continue
			}
			if msg.Event == "message" {
				subDone <- msg
				return
			}
		}
	}()

	// Wait for subscriber's "open" frame
	select {
	case <-subReady:
	case <-time.After(2 * time.Second):
		t.Fatal("subscriber never opened")
	}

	// Publish
	req, _ := http.NewRequest("POST", srv.URL+"/ntfy/test", strings.NewReader("payload"))
	req.Header.Set("Title", "hello")
	req.Header.Set("Priority", "high")
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		t.Fatalf("publish: %v", err)
	}
	resp.Body.Close()
	if resp.StatusCode != 200 {
		t.Fatalf("publish status=%d", resp.StatusCode)
	}

	select {
	case msg := <-subDone:
		if msg.Title != "hello" || msg.Message != "payload" || msg.Topic != "test" {
			t.Fatalf("unexpected msg: %+v", msg)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("subscriber never received message")
	}
}

func TestPriorityToInt(t *testing.T) {
	tests := map[string]int{
		"":        3,
		"min":     1,
		"low":     2,
		"default": 3,
		"high":    4,
		"max":     5,
		"urgent":  5,
		"unknown": 3,
	}
	for in, want := range tests {
		if got := priorityToInt(in); got != want {
			t.Errorf("priorityToInt(%q) = %d, want %d", in, got, want)
		}
	}
}
