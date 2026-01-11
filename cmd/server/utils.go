package main

import (
	"crypto/rand"
	"encoding/hex"
	"os"
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

// pathExists checks if a file or directory exists
func pathExists(path string) bool {
	_, err := os.Stat(path)
	if err == nil {
		return true
	}
	if os.IsNotExist(err) {
		return false
	}
	return false
}
