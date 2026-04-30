package ratelimit

import (
	"testing"
	"time"
)

func TestLimiterAllow(t *testing.T) {
	lim := New(3, time.Second)

	for i := 0; i < 3; i++ {
		if !lim.Allow("a") {
			t.Fatalf("request %d should be allowed", i+1)
		}
	}
	if lim.Allow("a") {
		t.Fatal("4th request should be denied")
	}

	// Different key should be independent.
	if !lim.Allow("b") {
		t.Fatal("different key should be allowed")
	}
}

func TestLimiterWindowReset(t *testing.T) {
	lim := New(1, 50*time.Millisecond)

	if !lim.Allow("x") {
		t.Fatal("first request should be allowed")
	}
	if lim.Allow("x") {
		t.Fatal("second request should be denied")
	}

	time.Sleep(60 * time.Millisecond)

	if !lim.Allow("x") {
		t.Fatal("request after window reset should be allowed")
	}
}

func TestLimiterGC(t *testing.T) {
	lim := New(10, 10*time.Millisecond)
	lim.gcEvery = 10 * time.Millisecond // speed up GC for test

	lim.Allow("gc-test")
	time.Sleep(20 * time.Millisecond)
	lim.Allow("trigger-gc") // triggers GC

	lim.mu.Lock()
	_, exists := lim.clients["gc-test"]
	lim.mu.Unlock()

	if exists {
		t.Fatal("expired entry should have been GC'd")
	}
}
