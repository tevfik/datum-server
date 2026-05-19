package system

import (
	"net"
	"testing"
)

func TestIsPrivateIP_System(t *testing.T) {
	tests := []struct {
		name     string
		ip       net.IP
		expected bool
	}{
		{"loopback IPv4", net.ParseIP("127.0.0.1"), true},
		{"loopback IPv6 ::1", net.ParseIP("::1"), true},
		{"RFC-1918 10.x.x.x", net.ParseIP("10.0.0.1"), true},
		{"RFC-1918 192.168.x.x", net.ParseIP("192.168.1.1"), true},
		{"RFC-1918 172.16.x.x", net.ParseIP("172.16.5.10"), true},
		{"link-local 169.254.x.x", net.ParseIP("169.254.1.1"), true},
		{"IPv6 unique local fc00::", net.ParseIP("fc00::1"), true},
		{"IPv6 unique local fd00::", net.ParseIP("fd00::1"), true},
		{"public 8.8.8.8", net.ParseIP("8.8.8.8"), false},
		{"public 1.1.1.1", net.ParseIP("1.1.1.1"), false},
		{"public 203.0.113.1", net.ParseIP("203.0.113.1"), false},
		{"public IPv6 2001:db8::1", net.ParseIP("2001:db8::1"), false},
		{"unspecified 0.0.0.0", net.ParseIP("0.0.0.0"), true},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			got := isPrivateIP(tc.ip)
			if got != tc.expected {
				t.Errorf("isPrivateIP(%v) = %v, want %v", tc.ip, got, tc.expected)
			}
		})
	}
}
