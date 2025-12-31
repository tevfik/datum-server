package auth

import (
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"encoding/base64"
	"encoding/hex"
	"errors"
	"fmt"
	"strconv"
	"strings"
	"time"
)

// Token-related errors
var (
	ErrTokenExpired     = errors.New("token has expired")
	ErrTokenInvalid     = errors.New("invalid token format")
	ErrTokenSignature   = errors.New("invalid token signature")
	ErrTokenRevoked     = errors.New("token has been revoked")
	ErrMasterSecretWeak = errors.New("master secret too weak")
)

// Token configuration defaults
const (
	DefaultTokenValidityDays     = 90
	DefaultGracePeriodDays       = 7
	DefaultTokenRefreshThreshold = 7 // days before expiry to suggest refresh
	MasterSecretMinLength        = 32
	TokenPrefix                  = "dk_" // Datum Key prefix
)

// DeviceToken represents the parsed token structure
type DeviceToken struct {
	DeviceID  string
	ExpiresAt time.Time
	Signature string
	Raw       string
}

// TokenValidationResult contains the result of token validation
type TokenValidationResult struct {
	Valid         bool
	Expired       bool
	NeedsRefresh  bool // Token is valid but approaching expiry
	InGracePeriod bool // Using previous token during grace period
	DeviceID      string
	ExpiresAt     time.Time
	Error         error
}

// GenerateMasterSecret creates a cryptographically secure master secret
// This should be stored securely on the device and never transmitted after provisioning
func GenerateMasterSecret() (string, error) {
	bytes := make([]byte, 32) // 256 bits
	if _, err := rand.Read(bytes); err != nil {
		return "", fmt.Errorf("failed to generate random bytes: %w", err)
	}
	return base64.URLEncoding.EncodeToString(bytes), nil
}

// GenerateDeviceToken creates a new token for a device
// Format: dk_{expiry_unix}.{signature}
// The signature is computed from deviceID:expiry using HMAC-SHA256
func GenerateDeviceToken(deviceID, masterSecret string, validityDays int) (string, time.Time, error) {
	if len(masterSecret) < MasterSecretMinLength {
		return "", time.Time{}, ErrMasterSecretWeak
	}

	if validityDays <= 0 {
		validityDays = DefaultTokenValidityDays
	}

	expiresAt := time.Now().Add(time.Duration(validityDays) * 24 * time.Hour)
	expiryUnix := expiresAt.Unix()

	// Create the token payload (full device ID in signature computation)
	payload := fmt.Sprintf("%s:%d", deviceID, expiryUnix)

	// Create HMAC-SHA256 signature
	signature := createHMAC(payload, masterSecret)

	// Build the token: dk_{expiry}.{signature_truncated}
	// Note: Device ID is NOT in token - server looks up by token signature
	token := fmt.Sprintf("%s%d.%s", TokenPrefix, expiryUnix, signature[:24])

	return token, expiresAt, nil
}

// ValidateDeviceToken validates a token against a master secret
// Returns detailed validation result
func ValidateDeviceToken(token, deviceID, masterSecret string) TokenValidationResult {
	result := TokenValidationResult{
		Valid:    false,
		DeviceID: deviceID,
	}

	// Parse the token
	parsed, err := ParseDeviceToken(token)
	if err != nil {
		result.Error = err
		return result
	}

	// Check if expired
	if time.Now().After(parsed.ExpiresAt) {
		result.Expired = true
		result.ExpiresAt = parsed.ExpiresAt
		result.Error = ErrTokenExpired
		return result
	}

	// Verify signature - compute expected signature using full device ID
	payload := fmt.Sprintf("%s:%d", deviceID, parsed.ExpiresAt.Unix())
	expectedSignature := createHMAC(payload, masterSecret)

	// Compare truncated signatures
	if len(expectedSignature) < 24 || !hmac.Equal([]byte(expectedSignature[:24]), []byte(parsed.Signature)) {
		result.Error = ErrTokenSignature
		return result
	}

	// Token is valid
	result.Valid = true
	result.ExpiresAt = parsed.ExpiresAt

	// Check if approaching expiry (needs refresh)
	refreshThreshold := time.Duration(DefaultTokenRefreshThreshold) * 24 * time.Hour
	if time.Until(parsed.ExpiresAt) < refreshThreshold {
		result.NeedsRefresh = true
	}

	return result
}

// ParseDeviceToken extracts components from a token string
// Token format: dk_{expiry}.{signature}
func ParseDeviceToken(token string) (*DeviceToken, error) {
	if !strings.HasPrefix(token, TokenPrefix) {
		return nil, ErrTokenInvalid
	}

	// Remove prefix
	tokenBody := strings.TrimPrefix(token, TokenPrefix)

	// Split by dot: {expiry}.{signature}
	parts := strings.SplitN(tokenBody, ".", 2)
	if len(parts) != 2 {
		return nil, ErrTokenInvalid
	}

	// Parse expiry timestamp
	expiryUnix, err := strconv.ParseInt(parts[0], 10, 64)
	if err != nil {
		return nil, fmt.Errorf("%w: invalid expiry timestamp", ErrTokenInvalid)
	}

	if len(parts[1]) < 16 {
		return nil, fmt.Errorf("%w: signature too short", ErrTokenInvalid)
	}

	return &DeviceToken{
		ExpiresAt: time.Unix(expiryUnix, 0),
		Signature: parts[1],
		Raw:       token,
	}, nil
}

// createHMAC creates an HMAC-SHA256 signature
func createHMAC(message, secret string) string {
	h := hmac.New(sha256.New, []byte(secret))
	h.Write([]byte(message))
	return hex.EncodeToString(h.Sum(nil))
}

// ValidateDualModeAuth validates either the new token system or legacy API key
// This enables backward compatibility during migration
type DualModeAuthResult struct {
	Authenticated bool
	DeviceID      string
	UsedLegacyKey bool      // True if authenticated via legacy APIKey
	UsedToken     bool      // True if authenticated via new token system
	NeedsRefresh  bool      // Token approaching expiry
	InGracePeriod bool      // Using previous token
	ExpiresAt     time.Time // Token expiry (if using token)
	Error         error
}

// TokenConfig holds token system configuration
type TokenConfig struct {
	ValidityDays         int // How long tokens are valid (default: 90)
	RefreshThresholdDays int // When to suggest refresh (default: 7)
	GracePeriodDays      int // How long old tokens remain valid after rotation (default: 7)
}

// DefaultTokenConfig returns the default configuration
func DefaultTokenConfig() TokenConfig {
	return TokenConfig{
		ValidityDays:         DefaultTokenValidityDays,
		RefreshThresholdDays: DefaultTokenRefreshThreshold,
		GracePeriodDays:      DefaultGracePeriodDays,
	}
}

// IsTokenExpiringSoon checks if a token will expire within the threshold
func IsTokenExpiringSoon(expiresAt time.Time, thresholdDays int) bool {
	if thresholdDays <= 0 {
		thresholdDays = DefaultTokenRefreshThreshold
	}
	threshold := time.Duration(thresholdDays) * 24 * time.Hour
	return time.Until(expiresAt) < threshold
}

// CalculateGracePeriodEnd calculates when the grace period ends after rotation
func CalculateGracePeriodEnd(gracePeriodDays int) time.Time {
	if gracePeriodDays <= 0 {
		gracePeriodDays = DefaultGracePeriodDays
	}
	return time.Now().Add(time.Duration(gracePeriodDays) * 24 * time.Hour)
}

// IsInGracePeriod checks if we're still within a grace period
func IsInGracePeriod(gracePeriodEnd time.Time) bool {
	return time.Now().Before(gracePeriodEnd)
}

// TokenInfo holds information about a device's tokens for API responses
type TokenInfo struct {
	CurrentToken   string    `json:"current_token,omitempty"`
	TokenExpiresAt time.Time `json:"token_expires_at,omitempty"`
	NeedsRefresh   bool      `json:"needs_refresh,omitempty"`
	GracePeriodEnd time.Time `json:"grace_period_end,omitempty"`
	InGracePeriod  bool      `json:"in_grace_period,omitempty"`
}
