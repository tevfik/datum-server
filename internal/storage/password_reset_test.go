package storage

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// ============ Password Reset Tests ============

func TestSavePasswordResetToken(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_pw_001", Email: "pw@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	err := s.SavePasswordResetToken(user.ID, "reset_token_abc123", 1*time.Hour)
	assert.NoError(t, err)
}

func TestGetUserByResetToken(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_pw_002", Email: "pw2@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	token := "reset_token_xyz789"
	require.NoError(t, s.SavePasswordResetToken(user.ID, token, 1*time.Hour))

	retrieved, err := s.GetUserByResetToken(token)
	require.NoError(t, err)
	assert.Equal(t, user.ID, retrieved.ID)
	assert.Equal(t, user.Email, retrieved.Email)
}

func TestGetUserByResetToken_InvalidToken(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	_, err := s.GetUserByResetToken("nonexistent_token")
	assert.Error(t, err)
}

func TestGetUserByResetToken_ExpiredToken(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_pw_003", Email: "pw3@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	// Save with very short TTL — BuntDB expiry is lazy (checked on access)
	// TTL of 1 nanosecond should expire immediately
	token := "expiring_token_001"
	require.NoError(t, s.SavePasswordResetToken(user.ID, token, 1*time.Nanosecond))

	// Small delay to let the TTL expire
	time.Sleep(10 * time.Millisecond)

	_, err := s.GetUserByResetToken(token)
	assert.Error(t, err)
}

func TestDeleteResetToken(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_pw_004", Email: "pw4@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	token := "delete_token_abc"
	require.NoError(t, s.SavePasswordResetToken(user.ID, token, 1*time.Hour))

	// Delete the token
	err := s.DeleteResetToken(token)
	assert.NoError(t, err)

	// Should no longer be retrievable
	_, err = s.GetUserByResetToken(token)
	assert.Error(t, err)
}

func TestDeleteResetToken_NonExistent(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	// Deleting a non-existent token should return an error (buntdb returns ErrNotFound)
	err := s.DeleteResetToken("nonexistent_token_xyz")
	assert.Error(t, err)
}

func TestPasswordResetFlow(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_prf_001", Email: "prf@example.com", Role: "user", Status: "active", PasswordHash: "oldhash", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	// 1. Save reset token
	token := "flow_reset_token_001"
	require.NoError(t, s.SavePasswordResetToken(user.ID, token, 15*time.Minute))

	// 2. Get user by token
	u, err := s.GetUserByResetToken(token)
	require.NoError(t, err)
	assert.Equal(t, user.ID, u.ID)

	// 3. Update password
	require.NoError(t, s.UpdateUserPassword(u.ID, "newhash"))

	// 4. Delete reset token (prevent reuse)
	require.NoError(t, s.DeleteResetToken(token))

	// 5. Token should be gone
	_, err = s.GetUserByResetToken(token)
	assert.Error(t, err)

	// 6. Verify password was updated
	updated, err := s.GetUserByID(user.ID)
	require.NoError(t, err)
	assert.Equal(t, "newhash", updated.PasswordHash)
}
