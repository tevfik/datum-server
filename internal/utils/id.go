package utils

import (
	"crypto/rand"
	"encoding/hex"
	"fmt"
)

// GenerateID generates a prefixed random ID with 6 random bytes (12 hex chars).
// Example: GenerateID("usr") -> "usr_a1b2c3d4e5f6"
func GenerateID(prefix string) string {
	return GenerateIDWithBytes(prefix, 6)
}

// GenerateIDWithBytes generates a prefixed random ID with a custom number of
// random bytes. Use 8 bytes (16 hex chars) for resources requiring stronger
// uniqueness guarantees (devices, sessions, API keys, etc.).
func GenerateIDWithBytes(prefix string, byteLen int) string {
	if byteLen <= 0 {
		byteLen = 6
	}
	b := make([]byte, byteLen)
	_, _ = rand.Read(b)
	return fmt.Sprintf("%s_%s", prefix, hex.EncodeToString(b))
}

