package data

import (
	"testing"
)

func TestIsPrivateIP(t *testing.T) {
	tests := []struct {
		name     string
		ip       string
		expected bool
	}{
		// RFC-1918 private ranges
		{"10.x.x.x range start", "10.0.0.1", true},
		{"10.x.x.x range middle", "10.100.200.50", true},
		{"10.x.x.x range end", "10.255.255.255", true},
		{"172.16.x.x range start", "172.16.0.1", true},
		{"172.20.x.x range middle", "172.20.100.5", true},
		{"172.31.x.x range end", "172.31.255.255", true},
		{"172.15.x.x just below range", "172.15.255.255", false},
		{"172.32.x.x just above range", "172.32.0.0", false},
		{"192.168.x.x range start", "192.168.0.1", true},
		{"192.168.x.x range end", "192.168.255.255", true},

		// Loopback
		{"loopback 127.0.0.1", "127.0.0.1", true},
		{"loopback 127.x.x.x", "127.100.0.1", true},

		// Link-local
		{"link-local 169.254.x.x", "169.254.1.1", true},
		{"link-local 169.254.0.0", "169.254.0.0", true},

		// IPv6 loopback
		{"IPv6 loopback ::1", "::1", true},

		// IPv6 unique local (fc00::/7)
		{"IPv6 unique local fc00::", "fc00::1", true},
		{"IPv6 unique local fd00::", "fd00::1", true},

		// Public IPs — should NOT be private
		{"public 8.8.8.8", "8.8.8.8", false},
		{"public 1.1.1.1", "1.1.1.1", false},
		{"public 203.0.113.1", "203.0.113.1", false},
		{"public 185.220.101.1", "185.220.101.1", false},

		// Edge cases
		{"empty string", "", false},
		{"invalid IP", "not_an_ip", false},
		{"partial IP", "192.168", false},
		{"IPv6 public 2001:db8::1", "2001:db8::1", false},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			got := isPrivateIP(tc.ip)
			if got != tc.expected {
				t.Errorf("isPrivateIP(%q) = %v, want %v", tc.ip, got, tc.expected)
			}
		})
	}
}
