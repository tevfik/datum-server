package handlers

import (
	"crypto/rand"
	"encoding/hex"
	"time"
)

// generateIDString creates a random hex string of specified byte length
func generateIDString(byteLen int) string {
	bytes := make([]byte, byteLen)
	rand.Read(bytes)
	return hex.EncodeToString(bytes)
}

// timeNow returns current time (helper for testability if needed)
func timeNow() time.Time {
	return time.Now()
}
