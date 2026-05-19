package auth

import (
	"crypto/rand"
	"encoding/base64"
	"fmt"
)

const (
	MasterSecretMinLength = 32
)

// GenerateMasterSecret creates a cryptographically secure master secret
// This should be stored securely on the device and never transmitted after provisioning
func GenerateMasterSecret() (string, error) {
	bytes := make([]byte, 32) // 256 bits
	if _, err := rand.Read(bytes); err != nil {
		return "", fmt.Errorf("failed to generate random bytes: %w", err)
	}
	return base64.URLEncoding.EncodeToString(bytes), nil
}
