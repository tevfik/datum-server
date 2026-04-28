package realtime

import (
	"sync"
	"testing"
	"time"
)

func TestHubPublishSubscribe(t *testing.T) {
	h := NewHub()
	sub := h.Subscribe("user/42/notifications")
	defer sub.Close()

	if n := h.Publish("user/42/notifications", []byte("hello"), nil); n != 1 {
		t.Fatalf("expected 1 delivery, got %d", n)
	}
	select {
	case msg := <-sub.C():
		if string(msg.Payload) != "hello" || msg.Topic != "user/42/notifications" {
			t.Fatalf("unexpected message: %+v", msg)
		}
	case <-time.After(time.Second):
		t.Fatal("timed out waiting for message")
	}
}

func TestHubMultipleSubscribers(t *testing.T) {
	h := NewHub()
	a := h.Subscribe("topic")
	b := h.Subscribe("topic")
	defer a.Close()
	defer b.Close()

	h.Publish("topic", []byte("x"), nil)
	for i, s := range []*Subscription{a, b} {
		select {
		case <-s.C():
		case <-time.After(time.Second):
			t.Fatalf("subscriber %d missed message", i)
		}
	}
}

func TestHubUnsubscribeOnClose(t *testing.T) {
	h := NewHub()
	sub := h.Subscribe("t")
	sub.Close()
	if got := h.Publish("t", []byte("ignored"), nil); got != 0 {
		t.Fatalf("expected zero deliveries after close, got %d", got)
	}
	if topics := h.Topics(); len(topics) != 0 {
		t.Fatalf("expected no topics, got %v", topics)
	}
}

func TestHubSlowSubscriberDoesNotBlock(t *testing.T) {
	h := NewHub()
	h.BufferSize = 1
	// Re-create with new buffer size by re-subscribing
	sub := h.Subscribe("t")
	defer sub.Close()

	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		defer wg.Done()
		for i := 0; i < 100; i++ {
			h.Publish("t", []byte{byte(i)}, nil)
		}
	}()
	done := make(chan struct{})
	go func() {
		wg.Wait()
		close(done)
	}()
	select {
	case <-done:
	case <-time.After(2 * time.Second):
		t.Fatal("publishes blocked on slow subscriber")
	}
}
