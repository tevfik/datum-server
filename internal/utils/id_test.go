package utils

import (
	"strings"
	"testing"
)

func TestGenerateID(t *testing.T) {
	id := GenerateID("usr")
	if !strings.HasPrefix(id, "usr_") {
		t.Fatalf("expected prefix usr_, got %q", id)
	}
	// usr_ + 12 hex chars = 16
	if len(id) != 16 {
		t.Fatalf("expected len 16, got %d (%q)", len(id), id)
	}
}

func TestGenerateIDWithBytes(t *testing.T) {
	id := GenerateIDWithBytes("dev", 8)
	if !strings.HasPrefix(id, "dev_") {
		t.Fatalf("expected prefix dev_, got %q", id)
	}
	// dev_ + 16 hex chars = 20
	if len(id) != 20 {
		t.Fatalf("expected len 20, got %d (%q)", len(id), id)
	}

	// Default fallback for non-positive byteLen
	id2 := GenerateIDWithBytes("x", 0)
	if len(id2) != 14 { // x_ + 12 hex
		t.Fatalf("expected len 14, got %d (%q)", len(id2), id2)
	}
}

func TestRandomHex(t *testing.T) {
	h := RandomHex(8)
	if len(h) != 16 {
		t.Fatalf("expected len 16, got %d", len(h))
	}
	h2 := RandomHex(0) // default 8
	if len(h2) != 16 {
		t.Fatalf("expected default len 16, got %d", len(h2))
	}

	// Two consecutive calls should not collide
	a, b := RandomHex(16), RandomHex(16)
	if a == b {
		t.Fatal("two random hex strings collided")
	}
}

func TestUniqueness(t *testing.T) {
	seen := make(map[string]struct{}, 1000)
	for i := 0; i < 1000; i++ {
		id := GenerateID("t")
		if _, ok := seen[id]; ok {
			t.Fatalf("duplicate ID generated: %s", id)
		}
		seen[id] = struct{}{}
	}
}
