package auth

import (
	"strings"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestGenerateMasterSecret(t *testing.T) {
	secret1, err := GenerateMasterSecret()
	require.NoError(t, err)
	assert.NotEmpty(t, secret1)
	assert.GreaterOrEqual(t, len(secret1), MasterSecretMinLength)

	// Generate another to verify uniqueness
	secret2, err := GenerateMasterSecret()
	require.NoError(t, err)
	assert.NotEqual(t, secret1, secret2, "secrets should be unique")
}

func TestGenerateDeviceToken(t *testing.T) {
	masterSecret, _ := GenerateMasterSecret()
	deviceID := "device_test_123"

	t.Run("valid token generation", func(t *testing.T) {
		token, expiresAt, err := GenerateDeviceToken(deviceID, masterSecret, 90)
		require.NoError(t, err)
		assert.NotEmpty(t, token)
		assert.True(t, strings.HasPrefix(token, TokenPrefix))
		assert.Contains(t, token, ".") // New format uses dots
		assert.True(t, expiresAt.After(time.Now()))
		assert.True(t, expiresAt.Before(time.Now().Add(91*24*time.Hour)))
	})

	t.Run("default validity when zero", func(t *testing.T) {
		token, expiresAt, err := GenerateDeviceToken(deviceID, masterSecret, 0)
		require.NoError(t, err)
		assert.NotEmpty(t, token)
		// Should use DefaultTokenValidityDays (90)
		expectedExpiry := time.Now().Add(time.Duration(DefaultTokenValidityDays) * 24 * time.Hour)
		assert.WithinDuration(t, expectedExpiry, expiresAt, time.Hour)
	})

	t.Run("weak master secret rejected", func(t *testing.T) {
		_, _, err := GenerateDeviceToken(deviceID, "short", 90)
		assert.ErrorIs(t, err, ErrMasterSecretWeak)
	})

	t.Run("custom validity days", func(t *testing.T) {
		_, expiresAt, err := GenerateDeviceToken(deviceID, masterSecret, 30)
		require.NoError(t, err)
		expectedExpiry := time.Now().Add(30 * 24 * time.Hour)
		assert.WithinDuration(t, expectedExpiry, expiresAt, time.Hour)
	})

	t.Run("device ID with underscores works", func(t *testing.T) {
		// This was a bug in the original format
		token, _, err := GenerateDeviceToken("device_with_many_underscores", masterSecret, 90)
		require.NoError(t, err)
		assert.True(t, strings.HasPrefix(token, TokenPrefix))

		// Token should be parseable
		parsed, err := ParseDeviceToken(token)
		require.NoError(t, err)
		assert.NotNil(t, parsed)
	})
}

func TestValidateDeviceToken(t *testing.T) {
	masterSecret, _ := GenerateMasterSecret()
	deviceID := "device_validate_test"

	t.Run("valid token", func(t *testing.T) {
		token, _, err := GenerateDeviceToken(deviceID, masterSecret, 90)
		require.NoError(t, err)

		result := ValidateDeviceToken(token, deviceID, masterSecret)
		assert.True(t, result.Valid, "token should be valid")
		assert.False(t, result.Expired)
		assert.NoError(t, result.Error)
	})

	t.Run("wrong device ID", func(t *testing.T) {
		token, _, err := GenerateDeviceToken(deviceID, masterSecret, 90)
		require.NoError(t, err)

		result := ValidateDeviceToken(token, "wrong_device_id", masterSecret)
		assert.False(t, result.Valid)
		assert.ErrorIs(t, result.Error, ErrTokenSignature)
	})

	t.Run("wrong master secret", func(t *testing.T) {
		token, _, err := GenerateDeviceToken(deviceID, masterSecret, 90)
		require.NoError(t, err)

		wrongSecret, _ := GenerateMasterSecret()
		result := ValidateDeviceToken(token, deviceID, wrongSecret)
		assert.False(t, result.Valid)
		assert.ErrorIs(t, result.Error, ErrTokenSignature)
	})

	t.Run("expired token", func(t *testing.T) {
		// Create a token that represents an already expired time
		// We can't use negative validity, so we'll manually construct a scenario
		// by generating a short-lived token and checking behavior

		// For proper testing, we'd need time mocking
		// Instead, let's verify the expiration check works by creating
		// a token with 0 validity (which gives default 90 days)
		// and manually test ParseDeviceToken with a past timestamp

		// Test manually constructed expired token
		expiredToken := "dk_1609459200.abcdef1234567890abcdef12" // Jan 1, 2021
		result := ValidateDeviceToken(expiredToken, "any_device", "any_secret_that_is_long_enough_32chars")
		assert.False(t, result.Valid)
		assert.True(t, result.Expired)
		assert.ErrorIs(t, result.Error, ErrTokenExpired)
	})

	t.Run("needs refresh detection", func(t *testing.T) {
		// Generate token expiring in 5 days (less than threshold of 7)
		token, _, err := GenerateDeviceToken(deviceID, masterSecret, 5)
		require.NoError(t, err)

		result := ValidateDeviceToken(token, deviceID, masterSecret)
		assert.True(t, result.Valid, "token should still be valid")
		assert.True(t, result.NeedsRefresh, "should indicate token needs refresh")
	})

	t.Run("token valid for long time - no refresh needed", func(t *testing.T) {
		token, _, err := GenerateDeviceToken(deviceID, masterSecret, 60)
		require.NoError(t, err)

		result := ValidateDeviceToken(token, deviceID, masterSecret)
		assert.True(t, result.Valid)
		assert.False(t, result.NeedsRefresh, "should not indicate refresh needed")
	})
}

func TestParseDeviceToken(t *testing.T) {
	t.Run("valid token format", func(t *testing.T) {
		masterSecret, _ := GenerateMasterSecret()
		token, expiresAt, _ := GenerateDeviceToken("device123", masterSecret, 90)

		parsed, err := ParseDeviceToken(token)
		require.NoError(t, err)
		assert.Equal(t, expiresAt.Unix(), parsed.ExpiresAt.Unix())
		assert.NotEmpty(t, parsed.Signature)
		assert.Equal(t, token, parsed.Raw)
	})

	t.Run("invalid prefix", func(t *testing.T) {
		_, err := ParseDeviceToken("invalid_token_format")
		assert.ErrorIs(t, err, ErrTokenInvalid)
	})

	t.Run("invalid format - no dot separator", func(t *testing.T) {
		_, err := ParseDeviceToken("dk_nodothere")
		assert.ErrorIs(t, err, ErrTokenInvalid)
	})

	t.Run("invalid expiry timestamp", func(t *testing.T) {
		_, err := ParseDeviceToken("dk_notanumber.signaturehere1234567890")
		assert.Error(t, err)
	})

	t.Run("signature too short", func(t *testing.T) {
		_, err := ParseDeviceToken("dk_1234567890.short")
		assert.Error(t, err)
	})
}

func TestIsTokenExpiringSoon(t *testing.T) {
	t.Run("token expiring soon", func(t *testing.T) {
		expiresAt := time.Now().Add(3 * 24 * time.Hour) // 3 days
		assert.True(t, IsTokenExpiringSoon(expiresAt, 7))
	})

	t.Run("token not expiring soon", func(t *testing.T) {
		expiresAt := time.Now().Add(30 * 24 * time.Hour) // 30 days
		assert.False(t, IsTokenExpiringSoon(expiresAt, 7))
	})

	t.Run("default threshold when zero", func(t *testing.T) {
		expiresAt := time.Now().Add(3 * 24 * time.Hour)
		assert.True(t, IsTokenExpiringSoon(expiresAt, 0)) // Uses default 7 days
	})
}

func TestCalculateGracePeriodEnd(t *testing.T) {
	t.Run("custom grace period", func(t *testing.T) {
		end := CalculateGracePeriodEnd(14)
		expected := time.Now().Add(14 * 24 * time.Hour)
		assert.WithinDuration(t, expected, end, time.Minute)
	})

	t.Run("default when zero", func(t *testing.T) {
		end := CalculateGracePeriodEnd(0)
		expected := time.Now().Add(time.Duration(DefaultGracePeriodDays) * 24 * time.Hour)
		assert.WithinDuration(t, expected, end, time.Minute)
	})
}

func TestIsInGracePeriod(t *testing.T) {
	t.Run("within grace period", func(t *testing.T) {
		gracePeriodEnd := time.Now().Add(24 * time.Hour)
		assert.True(t, IsInGracePeriod(gracePeriodEnd))
	})

	t.Run("after grace period", func(t *testing.T) {
		gracePeriodEnd := time.Now().Add(-24 * time.Hour)
		assert.False(t, IsInGracePeriod(gracePeriodEnd))
	})
}

func TestTokenConfig(t *testing.T) {
	config := DefaultTokenConfig()
	assert.Equal(t, DefaultTokenValidityDays, config.ValidityDays)
	assert.Equal(t, DefaultTokenRefreshThreshold, config.RefreshThresholdDays)
	assert.Equal(t, DefaultGracePeriodDays, config.GracePeriodDays)
}

func TestTokenRoundTrip(t *testing.T) {
	// Test that a token can be generated, parsed, and validated in sequence
	masterSecret, _ := GenerateMasterSecret()
	deviceID := "roundtrip_device_123"

	token, expiresAt, err := GenerateDeviceToken(deviceID, masterSecret, 90)
	require.NoError(t, err)

	parsed, err := ParseDeviceToken(token)
	require.NoError(t, err)
	assert.Equal(t, expiresAt.Unix(), parsed.ExpiresAt.Unix())

	result := ValidateDeviceToken(token, deviceID, masterSecret)
	assert.True(t, result.Valid)
	assert.Equal(t, deviceID, result.DeviceID)
}

// Benchmark tests
func BenchmarkGenerateDeviceToken(b *testing.B) {
	masterSecret, _ := GenerateMasterSecret()
	deviceID := "bench_device_123"

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		GenerateDeviceToken(deviceID, masterSecret, 90)
	}
}

func BenchmarkValidateDeviceToken(b *testing.B) {
	masterSecret, _ := GenerateMasterSecret()
	deviceID := "bench_device_123"
	token, _, _ := GenerateDeviceToken(deviceID, masterSecret, 90)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		ValidateDeviceToken(token, deviceID, masterSecret)
	}
}

func BenchmarkParseDeviceToken(b *testing.B) {
	masterSecret, _ := GenerateMasterSecret()
	token, _, _ := GenerateDeviceToken("device123", masterSecret, 90)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		ParseDeviceToken(token)
	}
}
