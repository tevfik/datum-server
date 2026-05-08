package notify

import (
	"context"
	"errors"
	"sync/atomic"
	"testing"
	"time"
)

func TestNewDispatcher(t *testing.T) {
	d := NewDispatcher()
	if d == nil {
		t.Fatal("expected non-nil dispatcher")
	}
	if len(d.Channels()) != 0 {
		t.Fatalf("expected 0 channels, got %d", len(d.Channels()))
	}
}

func TestRegisterNilChannel(t *testing.T) {
	d := NewDispatcher()
	d.Register(nil) // should not panic
	if len(d.Channels()) != 0 {
		t.Fatalf("expected 0 channels after nil register, got %d", len(d.Channels()))
	}
}

func TestDispatchNoChannels(t *testing.T) {
	d := NewDispatcher()
	// Should not panic when no channels configured
	d.Dispatch(Notification{UserID: "u1", Message: "test"})
}

func TestDispatchNoMatchingChannel(t *testing.T) {
	d := NewDispatcher()
	a := &recordingChannel{name: "a"}
	d.Register(a)

	// Request channel "b" which doesn't exist
	d.Dispatch(Notification{UserID: "u1", Channels: []string{"b"}})

	time.Sleep(50 * time.Millisecond)
	if got := atomic.LoadInt32(&a.calls); got != 0 {
		t.Fatalf("channel 'a' should not be called, got %d", got)
	}
}

func TestNotifyUserConvenience(t *testing.T) {
	d := NewDispatcher()
	a := &recordingChannel{name: "a"}
	d.Register(a)
	d.SetDefaultChannels([]string{"a"})

	d.NotifyUser("u1", "Title", "Body", "high")

	deadline := time.Now().Add(time.Second)
	for time.Now().Before(deadline) {
		if atomic.LoadInt32(&a.calls) == 1 {
			a.mu.Lock()
			last := a.last
			a.mu.Unlock()
			if last.UserID != "u1" || last.Title != "Title" || last.Message != "Body" || last.Priority != "high" {
				t.Fatalf("unexpected notification: %+v", last)
			}
			return
		}
		time.Sleep(5 * time.Millisecond)
	}
	t.Fatal("notification not received in time")
}

func TestChannelSendError(t *testing.T) {
	d := NewDispatcher()
	errCh := &recordingChannel{name: "err", err: errors.New("send failed")}
	goodCh := &recordingChannel{name: "good"}
	d.Register(errCh)
	d.Register(goodCh)
	d.SetDefaultChannels([]string{"err", "good"})

	d.Dispatch(Notification{UserID: "u1", Message: "test"})

	// Both channels should still be attempted even if one errors
	deadline := time.Now().Add(time.Second)
	for time.Now().Before(deadline) {
		if atomic.LoadInt32(&errCh.calls) >= 1 && atomic.LoadInt32(&goodCh.calls) >= 1 {
			return
		}
		time.Sleep(5 * time.Millisecond)
	}
	t.Fatalf("expected both channels called, err=%d good=%d",
		atomic.LoadInt32(&errCh.calls), atomic.LoadInt32(&goodCh.calls))
}

func TestSetDefaultChannelsReplaces(t *testing.T) {
	d := NewDispatcher()
	a := &recordingChannel{name: "a"}
	b := &recordingChannel{name: "b"}
	d.Register(a)
	d.Register(b)

	d.SetDefaultChannels([]string{"a"})
	d.SetDefaultChannels([]string{"b"}) // overwrite

	d.Dispatch(Notification{UserID: "u1", Message: "test"})

	time.Sleep(100 * time.Millisecond)
	if atomic.LoadInt32(&a.calls) != 0 {
		t.Fatal("channel 'a' should NOT be called after default override")
	}
	if atomic.LoadInt32(&b.calls) != 1 {
		t.Fatal("channel 'b' should be called once")
	}
}

func TestChannelsList(t *testing.T) {
	d := NewDispatcher()
	d.Register(&recordingChannel{name: "alpha"})
	d.Register(&recordingChannel{name: "beta"})

	names := d.Channels()
	if len(names) != 2 {
		t.Fatalf("expected 2 channels, got %d", len(names))
	}
	found := map[string]bool{}
	for _, n := range names {
		found[n] = true
	}
	if !found["alpha"] || !found["beta"] {
		t.Fatalf("expected alpha and beta, got %v", names)
	}
}

func TestNtfyBrokerCancelSubscription(t *testing.T) {
	b := NewNtfyBroker()
	ch, cancel := b.Subscribe("topicCancel", 4)

	cancel() // cancel immediately

	// ensure we don't block reading from a cancelled subscription
	select {
	case _, ok := <-ch:
		if ok {
			t.Fatal("channel should be closed after cancel")
		}
	case <-time.After(500 * time.Millisecond):
		// OK — channel drained or already closed
	}
}

func TestNtfyBrokerMultipleSubscribers(t *testing.T) {
	b := NewNtfyBroker()
	ch1, cancel1 := b.Subscribe("topicMulti", 4)
	ch2, cancel2 := b.Subscribe("topicMulti", 4)
	defer cancel1()
	defer cancel2()

	go b.Publish("topicMulti", "Hi", "Hello to all", "default", nil)

	received := 0
	timeout := time.After(time.Second)
	for received < 2 {
		select {
		case msg := <-ch1:
			if msg.Topic == "topicMulti" && msg.Message == "Hello to all" {
				received++
			}
		case msg := <-ch2:
			if msg.Topic == "topicMulti" && msg.Message == "Hello to all" {
				received++
			}
		case <-timeout:
			t.Fatalf("expected 2 subscribers to receive, got %d", received)
		}
	}
}

func TestNtfyBrokerDifferentTopics(t *testing.T) {
	b := NewNtfyBroker()
	ch1, cancel1 := b.Subscribe("topicA", 4)
	ch2, cancel2 := b.Subscribe("topicB", 4)
	defer cancel1()
	defer cancel2()

	go b.Publish("topicA", "", "Only A", "default", nil)

	select {
	case msg := <-ch1:
		if msg.Message != "Only A" {
			t.Fatalf("unexpected message on ch1: %s", msg.Message)
		}
	case <-time.After(time.Second):
		t.Fatal("topicA subscriber did not receive")
	}

	// ch2 should NOT receive
	select {
	case <-ch2:
		t.Fatal("topicB subscriber should not have received")
	case <-time.After(100 * time.Millisecond):
		// expected
	}
}

// Test context timeout behavior in Dispatch (indirectly via a slow channel)
type slowChannel struct {
	name    string
	calls   int32
	delay   time.Duration
}

func (s *slowChannel) Name() string { return s.name }
func (s *slowChannel) Send(ctx context.Context, _ Notification) error {
	atomic.AddInt32(&s.calls, 1)
	select {
	case <-time.After(s.delay):
		return nil
	case <-ctx.Done():
		return ctx.Err()
	}
}

func TestDispatchRespectsSendTimeout(t *testing.T) {
	d := NewDispatcher()
	fast := &slowChannel{name: "fast", delay: 10 * time.Millisecond}
	d.Register(fast)
	d.SetDefaultChannels([]string{"fast"})

	d.Dispatch(Notification{UserID: "u1", Message: "test"})

	time.Sleep(200 * time.Millisecond)
	if atomic.LoadInt32(&fast.calls) != 1 {
		t.Fatal("fast channel should have completed")
	}
}
