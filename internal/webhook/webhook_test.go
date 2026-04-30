package webhook

import (
	"testing"
	"time"
)

func TestDispatcherSubscribeUnsubscribe(t *testing.T) {
	d := NewDispatcher()
	defer d.Close()

	sub := &Subscription{
		ID:     "test-1",
		URL:    "http://example.com/hook",
		Events: []EventType{EventDeviceCreated},
	}
	d.Subscribe(sub)

	list := d.List()
	if len(list) != 1 {
		t.Fatalf("expected 1 subscription, got %d", len(list))
	}
	if list[0].ID != "test-1" {
		t.Fatalf("expected id test-1, got %s", list[0].ID)
	}

	d.Unsubscribe("test-1")
	if len(d.List()) != 0 {
		t.Fatal("expected 0 subscriptions after unsubscribe")
	}
}

func TestDispatcherEmitDoesNotBlock(t *testing.T) {
	d := NewDispatcher()
	defer d.Close()

	// Emit without subscribers should not panic or block.
	done := make(chan struct{})
	go func() {
		d.Emit(Event{
			ID:   "evt-1",
			Type: EventDataReceived,
		})
		close(done)
	}()

	select {
	case <-done:
	case <-time.After(time.Second):
		t.Fatal("Emit blocked unexpectedly")
	}
}

func TestDispatcherEventFiltering(t *testing.T) {
	d := NewDispatcher()
	defer d.Close()

	d.Subscribe(&Subscription{
		ID:     "only-create",
		URL:    "http://localhost:9999/noop",
		Events: []EventType{EventDeviceCreated},
	})

	// This should not crash - the endpoint doesn't exist but delivery is async.
	d.Emit(Event{ID: "e1", Type: EventDeviceDeleted})
	d.Emit(Event{ID: "e2", Type: EventDeviceCreated})

	// Give worker time to process.
	time.Sleep(50 * time.Millisecond)
}
