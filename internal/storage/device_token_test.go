package storage

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// ============ RotateDeviceKey Tests ============

func TestRotateDeviceKey(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_rdk_001", Email: "rdk@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	device := &Device{ID: "dev_rdk_001", UserID: user.ID, Name: "Token Device", APIKey: "sk_rdk_001", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(device))

	// Initialize with first token
	expiry := time.Now().Add(30 * 24 * time.Hour)
	d, err := s.InitializeDeviceToken(device.ID, "master_secret_abc", "token_v1", expiry)
	require.NoError(t, err)
	assert.Equal(t, "token_v1", d.CurrentToken)

	// Rotate to new token
	newExpiry := time.Now().Add(60 * 24 * time.Hour)
	rotated, err := s.RotateDeviceKey(device.ID, "token_v2", newExpiry, 7)
	require.NoError(t, err)
	assert.Equal(t, "token_v2", rotated.CurrentToken)
	assert.Equal(t, "token_v1", rotated.PreviousToken)
	assert.False(t, rotated.GracePeriodEnd.IsZero())
}

func TestRotateDeviceKey_DeviceNotFound(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	_, err := s.RotateDeviceKey("dev_nonexistent", "token_x", time.Now().Add(24*time.Hour), 7)
	assert.Error(t, err)
}

func TestRotateDeviceKey_RevokedDevice(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_rdk_rev", Email: "rdk_rev@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	device := &Device{ID: "dev_rdk_rev", UserID: user.ID, Name: "Revoked Device", APIKey: "sk_rdk_rev", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(device))

	// Revoke it first
	_, err := s.RevokeDeviceKey(device.ID)
	require.NoError(t, err)

	// Rotating a revoked device should fail
	_, err = s.RotateDeviceKey(device.ID, "token_x", time.Now().Add(24*time.Hour), 7)
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "revoked")
}

// ============ RevokeDeviceKey Tests ============

func TestRevokeDeviceKey(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_revoke_001", Email: "revoke@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	device := &Device{ID: "dev_revoke_001", UserID: user.ID, Name: "Revoke Device", APIKey: "sk_revoke_001", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(device))

	// Initialize token first
	expiry := time.Now().Add(30 * 24 * time.Hour)
	_, err := s.InitializeDeviceToken(device.ID, "master_secret", "token_to_revoke", expiry)
	require.NoError(t, err)

	// Revoke
	revoked, err := s.RevokeDeviceKey(device.ID)
	require.NoError(t, err)
	assert.Equal(t, "revoked", revoked.Status)
	assert.Empty(t, revoked.CurrentToken)
	assert.Empty(t, revoked.PreviousToken)
	assert.False(t, revoked.KeyRevokedAt.IsZero())
}

func TestRevokeDeviceKey_DeviceNotFound(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	_, err := s.RevokeDeviceKey("dev_nonexistent")
	assert.Error(t, err)
}

func TestRevokeDeviceKey_TokenIndexCleanup(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_tic_001", Email: "tic@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	device := &Device{ID: "dev_tic_001", UserID: user.ID, Name: "TIC Device", APIKey: "sk_tic_001", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(device))

	_, err := s.InitializeDeviceToken(device.ID, "master_tic", "active_token_tic", time.Now().Add(30*24*time.Hour))
	require.NoError(t, err)

	// Rotate to create previous token
	_, err = s.RotateDeviceKey(device.ID, "new_active_tic", time.Now().Add(60*24*time.Hour), 7)
	require.NoError(t, err)

	// Now revoke - both tokens should be cleared
	_, err = s.RevokeDeviceKey(device.ID)
	require.NoError(t, err)

	// Looking up by old token should fail
	_, _, err = s.GetDeviceByToken("active_token_tic")
	assert.Error(t, err)
	_, _, err = s.GetDeviceByToken("new_active_tic")
	assert.Error(t, err)
}

// ============ GetDeviceByToken Tests ============

func TestGetDeviceByToken_CurrentToken(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_gbt_001", Email: "gbt@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	device := &Device{ID: "dev_gbt_001", UserID: user.ID, Name: "GBT Device", APIKey: "sk_gbt_001", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(device))

	_, err := s.InitializeDeviceToken(device.ID, "master_gbt", "current_token_gbt", time.Now().Add(30*24*time.Hour))
	require.NoError(t, err)

	retrieved, isGrace, err := s.GetDeviceByToken("current_token_gbt")
	require.NoError(t, err)
	assert.Equal(t, device.ID, retrieved.ID)
	assert.False(t, isGrace)
}

func TestGetDeviceByToken_PreviousTokenInGrace(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_gbt_002", Email: "gbt2@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	device := &Device{ID: "dev_gbt_002", UserID: user.ID, Name: "GBT Device 2", APIKey: "sk_gbt_002", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(device))

	_, err := s.InitializeDeviceToken(device.ID, "master_gbt2", "old_token_gbt", time.Now().Add(30*24*time.Hour))
	require.NoError(t, err)

	// Rotate to push old_token_gbt to previous with 7-day grace
	_, err = s.RotateDeviceKey(device.ID, "new_token_gbt", time.Now().Add(60*24*time.Hour), 7)
	require.NoError(t, err)

	// Previous token should still work (within grace period)
	retrieved, isGrace, err := s.GetDeviceByToken("old_token_gbt")
	require.NoError(t, err)
	assert.Equal(t, device.ID, retrieved.ID)
	assert.True(t, isGrace)
}

func TestGetDeviceByToken_TokenNotFound(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	_, _, err := s.GetDeviceByToken("completely_invalid_token")
	assert.Error(t, err)
}

// ============ GetDeviceTokenInfo Tests ============

func TestGetDeviceTokenInfo_NoToken(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_tki_001", Email: "tki@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	device := &Device{ID: "dev_tki_001", UserID: user.ID, Name: "TKI Device", APIKey: "sk_tki_001", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(device))

	info, err := s.GetDeviceTokenInfo(device.ID)
	require.NoError(t, err)
	assert.Equal(t, false, info["has_token"])
}

func TestGetDeviceTokenInfo_WithToken(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_tki_002", Email: "tki2@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	device := &Device{ID: "dev_tki_002", UserID: user.ID, Name: "TKI Device 2", APIKey: "sk_tki_002", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(device))

	expiry := time.Now().Add(30 * 24 * time.Hour)
	_, err := s.InitializeDeviceToken(device.ID, "master_tki", "tki_token", expiry)
	require.NoError(t, err)

	info, err := s.GetDeviceTokenInfo(device.ID)
	require.NoError(t, err)
	assert.Equal(t, true, info["has_token"])
	assert.Equal(t, device.ID, info["device_id"])
}

func TestGetDeviceTokenInfo_RevokedDevice(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_tki_003", Email: "tki3@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	device := &Device{ID: "dev_tki_003", UserID: user.ID, Name: "TKI Device 3", APIKey: "sk_tki_003", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(device))

	_, err := s.InitializeDeviceToken(device.ID, "master_tki3", "tki3_token", time.Now().Add(30*24*time.Hour))
	require.NoError(t, err)

	_, err = s.RevokeDeviceKey(device.ID)
	require.NoError(t, err)

	info, err := s.GetDeviceTokenInfo(device.ID)
	require.NoError(t, err)
	assert.Equal(t, true, info["revoked"])
}

func TestGetDeviceTokenInfo_DeviceNotFound(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	_, err := s.GetDeviceTokenInfo("dev_nonexistent")
	assert.Error(t, err)
}

// ============ InitializeDeviceToken Tests ============

func TestInitializeDeviceToken(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_idt_001", Email: "idt@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	device := &Device{ID: "dev_idt_001", UserID: user.ID, Name: "IDT Device", APIKey: "sk_idt_001", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(device))

	expiry := time.Now().Add(30 * 24 * time.Hour)
	d, err := s.InitializeDeviceToken(device.ID, "secret_abc", "initial_token_idt", expiry)
	require.NoError(t, err)
	assert.Equal(t, "initial_token_idt", d.CurrentToken)
	assert.Equal(t, "secret_abc", d.MasterSecret)

	// Verify via token lookup
	found, isGrace, err := s.GetDeviceByToken("initial_token_idt")
	require.NoError(t, err)
	assert.Equal(t, device.ID, found.ID)
	assert.False(t, isGrace)
}

func TestInitializeDeviceToken_DeviceNotFound(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	_, err := s.InitializeDeviceToken("dev_nonexistent", "secret", "token", time.Now().Add(24*time.Hour))
	assert.Error(t, err)
}

// ============ CleanupExpiredGracePeriods Tests ============

func TestCleanupExpiredGracePeriods_NoExpired(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	count, err := s.CleanupExpiredGracePeriods()
	require.NoError(t, err)
	assert.Equal(t, 0, count)
}

func TestCleanupExpiredGracePeriods_WithExpired(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_cgp_001", Email: "cgp@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	device := &Device{ID: "dev_cgp_001", UserID: user.ID, Name: "CGP Device", APIKey: "sk_cgp_001", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(device))

	// Initialize and rotate with 0-day grace (immediately expired)
	_, err := s.InitializeDeviceToken(device.ID, "master_cgp", "old_token_cgp", time.Now().Add(30*24*time.Hour))
	require.NoError(t, err)

	// Rotate with grace period ending in the past (0 days = immediate)
	_, err = s.RotateDeviceKey(device.ID, "new_token_cgp", time.Now().Add(60*24*time.Hour), 0)
	require.NoError(t, err)

	// Cleanup — the grace period should already be in the past (0 days)
	count, err := s.CleanupExpiredGracePeriods()
	require.NoError(t, err)
	assert.GreaterOrEqual(t, count, 1)

	// Verify previous token is cleared
	retrieved, err := s.GetDevice(device.ID)
	require.NoError(t, err)
	assert.Empty(t, retrieved.PreviousToken)
}
