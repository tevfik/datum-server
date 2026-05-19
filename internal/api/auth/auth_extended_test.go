package auth

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// ---------- Logout ----------

func TestLogout_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	user := createTestUser(t, store, "logout@example.com", "password123", "user")

	// Create a session
	session := &storage.Session{
		JTI:       "jti_logout_001",
		UserID:    user.ID,
		CreatedAt: time.Now(),
		ExpiresAt: time.Now().Add(7 * 24 * time.Hour),
	}
	require.NoError(t, store.CreateSession(session))

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", user.ID)
		c.Set("session_jti", "jti_logout_001")
		c.Next()
	})
	r.POST("/auth/logout", handler.Logout)

	req, _ := http.NewRequest("POST", "/auth/logout", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Session should be deleted
	_, err := store.GetSession("jti_logout_001")
	assert.Error(t, err)
}

func TestLogout_NoSession(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	user := createTestUser(t, store, "logout2@example.com", "password123", "user")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", user.ID)
		// No session_jti set (e.g., API key auth)
		c.Next()
	})
	r.POST("/auth/logout", handler.Logout)

	req, _ := http.NewRequest("POST", "/auth/logout", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	// Should still succeed silently
	assert.Equal(t, http.StatusOK, w.Code)
}

// ---------- GetMe ----------

func TestGetMe_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	user := createTestUser(t, store, "me@example.com", "password123", "user")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", user.ID)
		c.Next()
	})
	r.GET("/auth/me", handler.GetMe)

	req, _ := http.NewRequest("GET", "/auth/me", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	assert.Equal(t, user.ID, resp["id"])
	assert.Equal(t, user.Email, resp["email"])
}

func TestGetMe_UserNotFound(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "usr_nonexistent")
		c.Next()
	})
	r.GET("/auth/me", handler.GetMe)

	req, _ := http.NewRequest("GET", "/auth/me", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

// ---------- UpdateMe ----------

func TestUpdateMe_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	user := createTestUser(t, store, "updateme@example.com", "password123", "user")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", user.ID)
		c.Next()
	})
	r.PUT("/auth/me", handler.UpdateMe)

	body, _ := json.Marshal(UpdateMeRequest{DisplayName: "John Doe"})
	req, _ := http.NewRequest("PUT", "/auth/me", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	updated, err := store.GetUserByID(user.ID)
	require.NoError(t, err)
	assert.Equal(t, "John Doe", updated.DisplayName)
}

func TestUpdateMe_InvalidBody(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	user := createTestUser(t, store, "updateme2@example.com", "password123", "user")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", user.ID)
		c.Next()
	})
	r.PUT("/auth/me", handler.UpdateMe)

	req, _ := http.NewRequest("PUT", "/auth/me", bytes.NewBufferString("not-json"))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

// ---------- ListSessions ----------

func TestListSessions_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	user := createTestUser(t, store, "sessions@example.com", "password123", "user")

	for i := 0; i < 3; i++ {
		ses := &storage.Session{
			JTI:       "jti_ls_" + string(rune('1'+i)),
			UserID:    user.ID,
			CreatedAt: time.Now(),
			ExpiresAt: time.Now().Add(7 * 24 * time.Hour),
		}
		require.NoError(t, store.CreateSession(ses))
	}

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", user.ID)
		c.Next()
	})
	r.GET("/auth/sessions", handler.ListSessions)

	req, _ := http.NewRequest("GET", "/auth/sessions", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	sessions := resp["sessions"].([]interface{})
	assert.Len(t, sessions, 3)
}

func TestListSessions_Empty(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	user := createTestUser(t, store, "sessions_empty@example.com", "password123", "user")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", user.ID)
		c.Next()
	})
	r.GET("/auth/sessions", handler.ListSessions)

	req, _ := http.NewRequest("GET", "/auth/sessions", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	sessions := resp["sessions"].([]interface{})
	assert.Empty(t, sessions)
}

// ---------- RevokeSession ----------

func TestRevokeSession_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	user := createTestUser(t, store, "revoke_ses@example.com", "password123", "user")

	session := &storage.Session{
		JTI:       "jti_revoke_001",
		UserID:    user.ID,
		CreatedAt: time.Now(),
		ExpiresAt: time.Now().Add(7 * 24 * time.Hour),
	}
	require.NoError(t, store.CreateSession(session))

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", user.ID)
		c.Next()
	})
	r.DELETE("/auth/sessions/:jti", handler.RevokeSession)

	req, _ := http.NewRequest("DELETE", "/auth/sessions/jti_revoke_001", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Session should be gone
	_, err := store.GetSession("jti_revoke_001")
	assert.Error(t, err)
}

func TestRevokeSession_NotFound(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	user := createTestUser(t, store, "revoke_ses2@example.com", "password123", "user")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", user.ID)
		c.Next()
	})
	r.DELETE("/auth/sessions/:jti", handler.RevokeSession)

	req, _ := http.NewRequest("DELETE", "/auth/sessions/nonexistent_jti", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func TestRevokeSession_WrongUser(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	user1 := createTestUser(t, store, "revoke_ses3@example.com", "password123", "user")
	user2 := createTestUser(t, store, "revoke_ses4@example.com", "password123", "user")

	session := &storage.Session{
		JTI:       "jti_wrong_user",
		UserID:    user2.ID,
		CreatedAt: time.Now(),
		ExpiresAt: time.Now().Add(7 * 24 * time.Hour),
	}
	require.NoError(t, store.CreateSession(session))

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", user1.ID)
		c.Next()
	})
	r.DELETE("/auth/sessions/:jti", handler.RevokeSession)

	req, _ := http.NewRequest("DELETE", "/auth/sessions/jti_wrong_user", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	// Should be 404 (session exists but belongs to different user)
	assert.Equal(t, http.StatusNotFound, w.Code)
}

// ---------- ListPushTokens ----------

func TestListPushTokens_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	user := createTestUser(t, store, "pushtokens@example.com", "password123", "user")

	pt := &storage.PushToken{
		ID:        "pt_handler_001",
		UserID:    user.ID,
		Platform:  "fcm",
		Token:     "firebase_token_xyz",
		CreatedAt: time.Now(),
	}
	require.NoError(t, store.SavePushToken(pt))

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", user.ID)
		c.Next()
	})
	r.GET("/auth/push-tokens", handler.ListPushTokens)

	req, _ := http.NewRequest("GET", "/auth/push-tokens", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	tokens := resp["tokens"].([]interface{})
	assert.Len(t, tokens, 1)
}

func TestListPushTokens_Empty(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	user := createTestUser(t, store, "nopushtokens@example.com", "password123", "user")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", user.ID)
		c.Next()
	})
	r.GET("/auth/push-tokens", handler.ListPushTokens)

	req, _ := http.NewRequest("GET", "/auth/push-tokens", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	tokens := resp["tokens"].([]interface{})
	assert.Empty(t, tokens)
}

// ---------- RegisterPushToken ----------

func TestRegisterPushToken_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	user := createTestUser(t, store, "regpush@example.com", "password123", "user")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", user.ID)
		c.Next()
	})
	r.POST("/auth/push-token", handler.RegisterPushToken)

	body, _ := json.Marshal(PushTokenRequest{Platform: "fcm", Token: "new_token_abc"})
	req, _ := http.NewRequest("POST", "/auth/push-token", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	assert.NotEmpty(t, resp["id"])
}

func TestRegisterPushToken_InvalidBody(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	user := createTestUser(t, store, "regpush2@example.com", "password123", "user")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", user.ID)
		c.Next()
	})
	r.POST("/auth/push-token", handler.RegisterPushToken)

	req, _ := http.NewRequest("POST", "/auth/push-token", bytes.NewBufferString("bad-json"))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

// ---------- DeletePushToken ----------

func TestDeletePushToken_Handler_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	user := createTestUser(t, store, "delpush@example.com", "password123", "user")

	pt := &storage.PushToken{
		ID:        "pt_del_handler_001",
		UserID:    user.ID,
		Platform:  "apns",
		Token:     "apns_token_xyz",
		CreatedAt: time.Now(),
	}
	require.NoError(t, store.SavePushToken(pt))

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", user.ID)
		c.Next()
	})
	r.DELETE("/auth/push-token/:id", handler.DeletePushToken)

	req, _ := http.NewRequest("DELETE", "/auth/push-token/pt_del_handler_001", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

// ---------- ForgotPassword ----------

func TestForgotPassword_ExistingUser(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestUser(t, store, "forgot@example.com", "password123", "user")

	r := gin.New()
	r.POST("/auth/forgot-password", handler.ForgotPassword)

	body, _ := json.Marshal(ForgotPasswordRequest{Email: "forgot@example.com"})
	req, _ := http.NewRequest("POST", "/auth/forgot-password", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	// Always returns 200 regardless of whether email exists (anti-enumeration)
	assert.Equal(t, http.StatusOK, w.Code)
}

func TestForgotPassword_NonExistentUser(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.POST("/auth/forgot-password", handler.ForgotPassword)

	body, _ := json.Marshal(ForgotPasswordRequest{Email: "nobody@example.com"})
	req, _ := http.NewRequest("POST", "/auth/forgot-password", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	// Should STILL return 200 to prevent user enumeration
	assert.Equal(t, http.StatusOK, w.Code)
}

func TestForgotPassword_InvalidEmail(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.POST("/auth/forgot-password", handler.ForgotPassword)

	body, _ := json.Marshal(map[string]string{"email": ""})
	req, _ := http.NewRequest("POST", "/auth/forgot-password", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}
