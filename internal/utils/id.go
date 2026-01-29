package utils

import (
	"crypto/rand"
	"encoding/hex"
	"fmt"
)

// GenerateID generates a prefixed random ID (e.g., usr_123456)
func GenerateID(prefix string) string {
	bytes := make([]byte, 6)
	rand.Read(bytes)
	return fmt.Sprintf("%s_%s", prefix, hex.EncodeToString(bytes))
}
