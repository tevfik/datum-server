package main

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
)

func TestSetProvisioningServerURL(t *testing.T) {
	// Save original config to restore later
	originalURL := provisioningConfig.ServerURL
	defer func() {
		provisioningConfig.ServerURL = originalURL
	}()

	t.Run("Update URL", func(t *testing.T) {
		newURL := "https://api.datum.example.com"
		SetProvisioningServerURL(newURL)

		assert.Equal(t, newURL, provisioningConfig.ServerURL)
	})

	t.Run("Config Defaults", func(t *testing.T) {
		// Verify defaults exist
		assert.Equal(t, 15*time.Minute, provisioningConfig.DefaultExpiration)
	})
}
