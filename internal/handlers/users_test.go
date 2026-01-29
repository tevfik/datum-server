package handlers

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"datum-go/internal/storage"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestCreateUserHandler(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	r := setupRouter(handler)
	r.POST("/users", handler.CreateUserHandler)

	t.Run("Valid Request", func(t *testing.T) {
		reqBody := map[string]string{
			"email":    "test@example.com",
			"password": "strongpassword123",
			"role":     "user",
		}
		body, _ := json.Marshal(reqBody)

		req, _ := http.NewRequest("POST", "/users", bytes.NewBuffer(body))
		w := httptest.NewRecorder()

		r.ServeHTTP(w, req)

		assert.Equal(t, http.StatusCreated, w.Code)

		var resp map[string]interface{}
		err := json.Unmarshal(w.Body.Bytes(), &resp)
		assert.NoError(t, err)
		assert.Equal(t, "test@example.com", resp["email"])
		assert.NotEmpty(t, resp["user_id"])
	})

	t.Run("Duplicate Email", func(t *testing.T) {
		// Create user first
		user := &storage.User{
			ID:           "existing_user",
			Email:        "duplicate@example.com",
			PasswordHash: "hash",
			Role:         "user",
			Status:       "active",
			CreatedAt:    time.Now(),
		}
		require.NoError(t, handler.Store.CreateUser(user))

		reqBody := map[string]string{
			"email":    "duplicate@example.com",
			"password": "anotherpassword",
			"role":     "user",
		}
		body, _ := json.Marshal(reqBody)

		req, _ := http.NewRequest("POST", "/users", bytes.NewBuffer(body))
		w := httptest.NewRecorder()

		r.ServeHTTP(w, req)

		assert.Equal(t, http.StatusConflict, w.Code)
	})
}

func TestListUsersHandler(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	// Create user
	user := &storage.User{
		ID:           "user_list_test",
		Email:        "list@example.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	require.NoError(t, handler.Store.CreateUser(user))

	r := setupRouter(handler)
	r.GET("/users", handler.ListUsersHandler)

	req, _ := http.NewRequest("GET", "/users", nil)
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &resp)
	assert.NoError(t, err)

	users := resp["users"].([]interface{})
	assert.GreaterOrEqual(t, len(users), 1)
}

func TestGetUserHandler(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	user := &storage.User{
		ID:           "user_get_test",
		Email:        "get@example.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	require.NoError(t, handler.Store.CreateUser(user))

	r := setupRouter(handler)
	r.GET("/users/:user_id", handler.GetUserHandler)

	req, _ := http.NewRequest("GET", "/users/user_get_test", nil)
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &resp)
	assert.NoError(t, err)
	assert.Equal(t, "get@example.com", resp["email"])
}

func TestUpdateUserHandler(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	user := &storage.User{
		ID:           "user_update_test",
		Email:        "update@example.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	require.NoError(t, handler.Store.CreateUser(user))

	r := setupRouter(handler)
	r.PUT("/users/:user_id", handler.UpdateUserHandler)

	reqBody := map[string]string{
		"role": "admin",
	}
	body, _ := json.Marshal(reqBody)

	req, _ := http.NewRequest("PUT", "/users/user_update_test", bytes.NewBuffer(body))
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	updated, _ := handler.Store.GetUserByID("user_update_test")
	assert.Equal(t, "admin", updated.Role)
}

func TestResetPasswordHandler(t *testing.T) {
	handler, cleanup := setupTestEnv(t)
	defer cleanup()

	// Initial password hash
	initialHash := "initial_hash"
	user := &storage.User{
		ID:           "user_reset_test",
		Email:        "reset@example.com",
		PasswordHash: initialHash,
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	require.NoError(t, handler.Store.CreateUser(user))

	r := setupRouter(handler)
	r.POST("/users/reset-password/:username", handler.ResetPasswordHandler)

	t.Run("With New Password", func(t *testing.T) {
		reqBody := map[string]string{
			"new_password": "newStrongPassword123!",
		}
		body, _ := json.Marshal(reqBody)

		req, _ := http.NewRequest("POST", "/users/reset-password/reset@example.com", bytes.NewBuffer(body))
		w := httptest.NewRecorder()

		r.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)

		updated, _ := handler.Store.GetUserByID("user_reset_test")
		assert.NotEqual(t, initialHash, updated.PasswordHash)
	})

	t.Run("Auto Generate Password", func(t *testing.T) {
		req, _ := http.NewRequest("POST", "/users/reset-password/reset@example.com", strings.NewReader("{}"))
		w := httptest.NewRecorder()

		r.ServeHTTP(w, req)

		assert.Equal(t, http.StatusOK, w.Code)

		var resp map[string]interface{}
		json.Unmarshal(w.Body.Bytes(), &resp)
		assert.NotEmpty(t, resp["new_password"])
	})
}
