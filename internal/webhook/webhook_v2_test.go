package webhook

import (
	"io"
	"net/http"
	"net/http/httptest"
	"strconv"
	"sync/atomic"
	"testing"
	"time"
)

func TestSignAndVerify(t *testing.T) {
	body := []byte(`{"hello":"world"}`)
	ts := int64(1700000000)
	sig := SignBody("topsecret", ts, body)
	if sig == "" {
		t.Fatal("empty signature for non-empty secret")
	}
	if !VerifySignature("topsecret", ts, body, sig) {
		t.Fatal("VerifySignature returned false for matching signature")
	}
	if VerifySignature("wrongsecret", ts, body, sig) {
		t.Fatal("VerifySignature accepted wrong secret")
	}
}

func TestSendRetriesAndSignsBody(t *testing.T) {
	var calls atomic.Int32
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		n := calls.Add(1)
		body, _ := io.ReadAll(r.Body)
		ts, _ := strconv.ParseInt(r.Header.Get("X-Datum-Timestamp"), 10, 64)
		sig := r.Header.Get("X-Datum-Signature")
		if !VerifySignature("s3cret", ts, body, sig) {
			t.Errorf("attempt %d: signature did not verify", n)
		}
		if n < 3 {
			http.Error(w, "boom", http.StatusInternalServerError)
			return
		}
		w.WriteHeader(http.StatusOK)
	}))
	defer srv.Close()

	d := NewDispatcher()
	defer d.Close()
	d.Subscribe(&Subscription{
		ID:     "test-sub",
		URL:    srv.URL,
		Secret: "s3cret",
		Events: []EventType{"*"},
	})
	d.Emit(Event{Type: EventDataReceived, ID: "evt-1", Data: map[string]interface{}{"x": 1}})

	deadline := time.Now().Add(15 * time.Second)
	for time.Now().Before(deadline) {
		if calls.Load() >= 3 {
			return
		}
		time.Sleep(50 * time.Millisecond)
	}
	t.Fatalf("expected at least 3 calls after retries, got %d", calls.Load())
}

func TestSendDoesNotRetryOn4xx(t *testing.T) {
	var calls atomic.Int32
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		calls.Add(1)
		http.Error(w, "bad", http.StatusBadRequest)
	}))
	defer srv.Close()

	d := NewDispatcher()
	defer d.Close()
	d.Subscribe(&Subscription{ID: "x", URL: srv.URL, Events: []EventType{"*"}})
	d.Emit(Event{Type: EventDataReceived})

	time.Sleep(500 * time.Millisecond)
	if got := calls.Load(); got != 1 {
		t.Fatalf("expected single attempt on 4xx, got %d", got)
	}
}
