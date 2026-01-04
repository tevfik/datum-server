package main

import (
	"bytes"
	"datum-go/internal/auth"
	"datum-go/internal/storage"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
)

func TestAuthHandlers(t *testing.T) {
	// Setup temporary storage
	tmpDir, _ := os.MkdirTemp("", "datum-test-auth-*")
	defer os.RemoveAll(tmpDir)

	testStore, err := storage.New(tmpDir+"/meta.db", tmpDir+"/tsdata", 24*time.Hour)
	assert.NoError(t, err)
	defer testStore.Close()

	// Assign global store for handlers
	store = testStore

	// Setup Gin
	gin.SetMode(gin.TestMode)
	r := gin.New()

	// Register routes manually to permit testing
	authProtectedGroup := r.Group("/auth")
	authProtectedGroup.Use(auth.AuthMiddleware()) // Uses the JWT middleware
	{
		authProtectedGroup.PUT("/password", changePasswordHandler)
		authProtectedGroup.DELETE("/user", deleteSelfHandler)
	}

	// Create a test user
	password := "securepassword123"
	hashedPassword, _ := auth.HashPassword(password)
	userID := "user_test_1"
	user := &storage.User{
		ID:           userID,
		Email:        "test@example.com",
		PasswordHash: hashedPassword,
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err = store.CreateUser(user)
	assert.NoError(t, err)

	// Generate a valid token for this user
	token, _ := auth.GenerateToken(userID, user.Email, user.Role)
	authHeader := "Bearer " + token

	t.Run("Change Password - Success", func(t *testing.T) {
		payload := map[string]string{
			"old_password": password,
			"new_password": "newsecurepassword123",
		}
		body, _ := json.Marshal(payload)

		req, _ := http.NewRequest("PUT", "/auth/password", bytes.NewBuffer(body))
		req.Header.Set("Authorization", authHeader)
		req.Header.Set("Content-Type", "application/json")
		w := httptest.NewRecorder()

		r.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)

		// Verify password changed in DB
		updatedUser, _ := store.GetUserByID(userID)
		assert.True(t, auth.CheckPassword(updatedUser.PasswordHash, "newsecurepassword123"))
	})

	t.Run("Change Password - Wrong Old Password", func(t *testing.T) {
		payload := map[string]string{
			"old_password": "wrongpassword",
			"new_password": "newsecurepassword123",
		}
		body, _ := json.Marshal(payload)

		req, _ := http.NewRequest("PUT", "/auth/password", bytes.NewBuffer(body))
		req.Header.Set("Authorization", authHeader)
		w := httptest.NewRecorder()

		r.ServeHTTP(w, req)

		assert.Equal(t, http.StatusUnauthorized, w.Code)
	})

	t.Run("Delete Account - Success", func(t *testing.T) {
		req, _ := http.NewRequest("DELETE", "/auth/user", nil)
		req.Header.Set("Authorization", authHeader)
		w := httptest.NewRecorder()

		r.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)

		// Verify user is gone
		_, err := store.GetUserByID(userID)
		assert.Error(t, err) // Should return error (not found)
	})

	t.Run("Delete Account - Unauthorized", func(t *testing.T) {
		req, _ := http.NewRequest("DELETE", "/auth/user", nil)
		// No Auth Header
		w := httptest.NewRecorder()

		r.ServeHTTP(w, req)

		assert.Equal(t, http.StatusUnauthorized, w.Code)
	})
}
