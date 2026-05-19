package storage

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// ============ Session Tests ============

func TestCreateSession(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	session := &Session{
		JTI:       "jti_test_001",
		UserID:    "usr_ses_001",
		CreatedAt: time.Now(),
		ExpiresAt: time.Now().Add(7 * 24 * time.Hour),
		UserAgent: "TestAgent/1.0",
		IP:        "127.0.0.1",
	}

	err := s.CreateSession(session)
	assert.NoError(t, err)
}

func TestGetSession(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	session := &Session{
		JTI:       "jti_get_001",
		UserID:    "usr_ses_002",
		CreatedAt: time.Now(),
		ExpiresAt: time.Now().Add(7 * 24 * time.Hour),
		UserAgent: "TestAgent/2.0",
		IP:        "192.168.1.1",
	}

	require.NoError(t, s.CreateSession(session))

	retrieved, err := s.GetSession("jti_get_001")
	require.NoError(t, err)
	assert.Equal(t, session.JTI, retrieved.JTI)
	assert.Equal(t, session.UserID, retrieved.UserID)
	assert.Equal(t, session.UserAgent, retrieved.UserAgent)
	assert.Equal(t, session.IP, retrieved.IP)
}

func TestGetSession_NotFound(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	_, err := s.GetSession("nonexistent_jti")
	assert.Error(t, err)
}

func TestDeleteSession(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	session := &Session{
		JTI:       "jti_del_001",
		UserID:    "usr_ses_003",
		CreatedAt: time.Now(),
		ExpiresAt: time.Now().Add(7 * 24 * time.Hour),
	}

	require.NoError(t, s.CreateSession(session))
	require.NoError(t, s.DeleteSession("jti_del_001"))

	_, err := s.GetSession("jti_del_001")
	assert.Error(t, err)
}

func TestDeleteSession_NonExistent(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	// Deleting non-existent session should not error (idempotent)
	err := s.DeleteSession("nonexistent_jti")
	assert.NoError(t, err)
}

func TestGetUserSessions(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	userID := "usr_ses_004"

	// Create 3 sessions for this user, 1 for another
	for i := 1; i <= 3; i++ {
		ses := &Session{
			JTI:       "jti_gus_" + string(rune('0'+i)),
			UserID:    userID,
			CreatedAt: time.Now(),
			ExpiresAt: time.Now().Add(7 * 24 * time.Hour),
		}
		require.NoError(t, s.CreateSession(ses))
	}

	// Different user
	require.NoError(t, s.CreateSession(&Session{
		JTI:       "jti_other_001",
		UserID:    "usr_other_001",
		CreatedAt: time.Now(),
		ExpiresAt: time.Now().Add(7 * 24 * time.Hour),
	}))

	sessions, err := s.GetUserSessions(userID)
	require.NoError(t, err)
	assert.Len(t, sessions, 3)
}

func TestGetUserSessions_ExcludesExpired(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	userID := "usr_ses_005"

	// Active session
	require.NoError(t, s.CreateSession(&Session{
		JTI:       "jti_active_001",
		UserID:    userID,
		CreatedAt: time.Now(),
		ExpiresAt: time.Now().Add(7 * 24 * time.Hour),
	}))

	// Expired session
	require.NoError(t, s.CreateSession(&Session{
		JTI:       "jti_expired_001",
		UserID:    userID,
		CreatedAt: time.Now().Add(-14 * 24 * time.Hour),
		ExpiresAt: time.Now().Add(-7 * 24 * time.Hour),
	}))

	sessions, err := s.GetUserSessions(userID)
	require.NoError(t, err)
	assert.Len(t, sessions, 1) // Only non-expired
	assert.Equal(t, "jti_active_001", sessions[0].JTI)
}

func TestDeleteAllUserSessions(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	userID := "usr_ses_006"

	for i := 1; i <= 4; i++ {
		ses := &Session{
			JTI:       "jti_daus_" + string(rune('0'+i)),
			UserID:    userID,
			CreatedAt: time.Now(),
			ExpiresAt: time.Now().Add(7 * 24 * time.Hour),
		}
		require.NoError(t, s.CreateSession(ses))
	}

	require.NoError(t, s.DeleteAllUserSessions(userID))

	sessions, err := s.GetUserSessions(userID)
	require.NoError(t, err)
	assert.Empty(t, sessions)
}

func TestDeleteAllUserSessions_DoesNotAffectOtherUsers(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	// User A: 2 sessions
	for i := 1; i <= 2; i++ {
		ses := &Session{
			JTI:       "jti_ua_" + string(rune('0'+i)),
			UserID:    "usr_ua_100",
			CreatedAt: time.Now(),
			ExpiresAt: time.Now().Add(7 * 24 * time.Hour),
		}
		require.NoError(t, s.CreateSession(ses))
	}

	// User B: 2 sessions
	for i := 1; i <= 2; i++ {
		ses := &Session{
			JTI:       "jti_ub_" + string(rune('0'+i)),
			UserID:    "usr_ub_100",
			CreatedAt: time.Now(),
			ExpiresAt: time.Now().Add(7 * 24 * time.Hour),
		}
		require.NoError(t, s.CreateSession(ses))
	}

	// Delete only User A's sessions
	require.NoError(t, s.DeleteAllUserSessions("usr_ua_100"))

	uaSessions, err := s.GetUserSessions("usr_ua_100")
	require.NoError(t, err)
	assert.Empty(t, uaSessions)

	ubSessions, err := s.GetUserSessions("usr_ub_100")
	require.NoError(t, err)
	assert.Len(t, ubSessions, 2)
}

// ============ Push Token Tests ============

func TestSavePushToken(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	pt := &PushToken{
		ID:        "pt_test_001",
		UserID:    "usr_pt_001",
		Platform:  "fcm",
		Token:     "fcm_token_abc123",
		CreatedAt: time.Now(),
	}

	err := s.SavePushToken(pt)
	assert.NoError(t, err)
}

func TestGetUserPushTokens(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	userID := "usr_pt_002"

	for i := 1; i <= 3; i++ {
		pt := &PushToken{
			ID:        "pt_" + string(rune('0'+i)),
			UserID:    userID,
			Platform:  "fcm",
			Token:     "token_" + string(rune('0'+i)),
			CreatedAt: time.Now(),
		}
		require.NoError(t, s.SavePushToken(pt))
	}

	// Different user
	other := &PushToken{ID: "pt_other", UserID: "usr_pt_other", Platform: "apns", Token: "other_token", CreatedAt: time.Now()}
	require.NoError(t, s.SavePushToken(other))

	tokens, err := s.GetUserPushTokens(userID)
	require.NoError(t, err)
	assert.Len(t, tokens, 3)
}

func TestGetUserPushTokens_Empty(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	tokens, err := s.GetUserPushTokens("usr_no_tokens")
	require.NoError(t, err)
	assert.Empty(t, tokens)
}

func TestDeletePushToken(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	userID := "usr_pt_003"
	pt := &PushToken{ID: "pt_del_001", UserID: userID, Platform: "fcm", Token: "del_token_001", CreatedAt: time.Now()}
	require.NoError(t, s.SavePushToken(pt))

	require.NoError(t, s.DeletePushToken(userID, "pt_del_001"))

	tokens, err := s.GetUserPushTokens(userID)
	require.NoError(t, err)
	assert.Empty(t, tokens)
}

func TestDeletePushToken_Unauthorized(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	pt := &PushToken{ID: "pt_auth_001", UserID: "usr_pt_owner", Platform: "fcm", Token: "auth_token_001", CreatedAt: time.Now()}
	require.NoError(t, s.SavePushToken(pt))

	// Different user tries to delete — should fail
	err := s.DeletePushToken("usr_pt_attacker", "pt_auth_001")
	assert.Error(t, err)
}

func TestDeletePushToken_NonExistent(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	// Non-existent token should not error (idempotent)
	err := s.DeletePushToken("usr_pt_004", "nonexistent_pt")
	assert.NoError(t, err)
}

// ============ UpdateUserProfile Tests ============

func TestUpdateUserProfile(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_prof_001", Email: "prof@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	err := s.UpdateUserProfile(user.ID, "John Doe")
	require.NoError(t, err)

	retrieved, err := s.GetUserByID(user.ID)
	require.NoError(t, err)
	assert.Equal(t, "John Doe", retrieved.DisplayName)
}

func TestUpdateUserProfile_UserNotFound(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	err := s.UpdateUserProfile("usr_nonexistent", "John Doe")
	assert.Error(t, err)
}

func TestUpdateUserProfile_EmptyName(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_prof_002", Email: "prof2@example.com", Role: "user", Status: "active", DisplayName: "Old Name", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	// Empty string should clear display name
	err := s.UpdateUserProfile(user.ID, "")
	require.NoError(t, err)

	retrieved, err := s.GetUserByID(user.ID)
	require.NoError(t, err)
	assert.Equal(t, "", retrieved.DisplayName)
}
