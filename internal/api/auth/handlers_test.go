package auth

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"testing"
	"time"

	internalauth "datum-go/internal/auth"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func setupTestEnv(t *testing.T) (*Handler, storage.Provider, func()) {
	gin.SetMode(gin.TestMode)
	tmpDir := t.TempDir()
	store, err := storage.New(
		filepath.Join(tmpDir, "meta.db"),
		filepath.Join(tmpDir, "tsdata"),
		24*time.Hour,
	)
	require.NoError(t, err)

	// Initialize system so registration works
	require.NoError(t, store.InitializeSystem("test", true, 7))

	handler := NewHandler(store, nil, "http://localhost:8090")
	return handler, store, func() { store.Close() }
}

func createTestUser(t *testing.T, store storage.Provider, email, password, role string) *storage.User {
	t.Helper()
	hash, err := internalauth.HashPassword(password)
	require.NoError(t, err)
	user := &storage.User{
		ID:           "usr_" + email,
		Email:        email,
		PasswordHash: hash,
		Role:         role,
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	require.NoError(t, store.CreateUser(user))
	return user
}

// ---------- Register ----------

func TestRegister_Success(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.POST("/auth/register", handler.Register)

	body, _ := json.Marshal(RegisterRequest{
		Email:    "test@example.com",
		Password: "password123",
	})
	req, _ := http.NewRequest("POST", "/auth/register", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	assert.NotEmpty(t, resp["user_id"])
	assert.NotEmpty(t, resp["token"])
	assert.Equal(t, "user", resp["role"])
}

func TestRegister_DuplicateEmail(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestUser(t, store, "dup@example.com", "password123", "user")

	r := gin.New()
	r.POST("/auth/register", handler.Register)

	body, _ := json.Marshal(RegisterRequest{
		Email:    "dup@example.com",
		Password: "password123",
	})
	req, _ := http.NewRequest("POST", "/auth/register", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusConflict, w.Code)
}

func TestRegister_InvalidEmail(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.POST("/auth/register", handler.Register)

	body, _ := json.Marshal(map[string]string{
		"email":    "not-an-email",
		"password": "password123",
	})
	req, _ := http.NewRequest("POST", "/auth/register", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestRegister_ShortPassword(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.POST("/auth/register", handler.Register)

	body, _ := json.Marshal(RegisterRequest{
		Email:    "short@example.com",
		Password: "short",
	})
	req, _ := http.NewRequest("POST", "/auth/register", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func TestRegister_SystemNotInitialized(t *testing.T) {
	gin.SetMode(gin.TestMode)
	tmpDir := t.TempDir()
	store, err := storage.New(
		filepath.Join(tmpDir, "meta.db"),
		filepath.Join(tmpDir, "tsdata"),
		24*time.Hour,
	)
	require.NoError(t, err)
	defer store.Close()

	handler := NewHandler(store, nil, "")

	r := gin.New()
	r.POST("/auth/register", handler.Register)

	body, _ := json.Marshal(RegisterRequest{
		Email:    "test@example.com",
		Password: "password123",
	})
	req, _ := http.NewRequest("POST", "/auth/register", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)
}

// ---------- Login ----------

func TestLogin_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestUser(t, store, "login@example.com", "password123", "user")

	r := gin.New()
	r.POST("/auth/login", handler.Login)

	body, _ := json.Marshal(LoginRequest{
		Email:    "login@example.com",
		Password: "password123",
	})
	req, _ := http.NewRequest("POST", "/auth/login", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	assert.NotEmpty(t, resp["token"])
	assert.NotEmpty(t, resp["refresh_token"])
	assert.Equal(t, "user", resp["role"])
}

func TestLogin_WrongPassword(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestUser(t, store, "login@example.com", "correctpassword", "user")

	r := gin.New()
	r.POST("/auth/login", handler.Login)

	body, _ := json.Marshal(LoginRequest{
		Email:    "login@example.com",
		Password: "wrongpassword",
	})
	req, _ := http.NewRequest("POST", "/auth/login", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func TestLogin_NonExistentUser(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.POST("/auth/login", handler.Login)

	body, _ := json.Marshal(LoginRequest{
		Email:    "nobody@example.com",
		Password: "password123",
	})
	req, _ := http.NewRequest("POST", "/auth/login", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func TestLogin_SuspendedUser(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	user := createTestUser(t, store, "suspended@example.com", "password123", "user")
	require.NoError(t, store.UpdateUser(user.ID, "user", "suspended"))

	r := gin.New()
	r.POST("/auth/login", handler.Login)

	body, _ := json.Marshal(LoginRequest{
		Email:    "suspended@example.com",
		Password: "password123",
	})
	req, _ := http.NewRequest("POST", "/auth/login", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)
}

// ---------- ChangePassword ----------

func TestChangePassword_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestUser(t, store, "chpass@example.com", "oldpassword1", "user")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "usr_chpass@example.com")
		c.Next()
	})
	r.PUT("/auth/password", handler.ChangePassword)

	body, _ := json.Marshal(ChangePasswordRequest{
		OldPassword: "oldpassword1",
		NewPassword: "newpassword1",
	})
	req, _ := http.NewRequest("PUT", "/auth/password", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify new password works
	user, _ := store.GetUserByEmail("chpass@example.com")
	assert.True(t, internalauth.CheckPassword(user.PasswordHash, "newpassword1"))
}

func TestChangePassword_WrongOldPassword(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestUser(t, store, "chpass2@example.com", "oldpassword1", "user")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "usr_chpass2@example.com")
		c.Next()
	})
	r.PUT("/auth/password", handler.ChangePassword)

	body, _ := json.Marshal(ChangePasswordRequest{
		OldPassword: "wrongold",
		NewPassword: "newpassword1",
	})
	req, _ := http.NewRequest("PUT", "/auth/password", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

// ---------- DeleteSelf ----------

func TestDeleteSelf_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestUser(t, store, "delete@example.com", "password123", "user")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "usr_delete@example.com")
		c.Next()
	})
	r.DELETE("/auth/user", handler.DeleteSelf)

	req, _ := http.NewRequest("DELETE", "/auth/user", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify user is gone
	_, err := store.GetUserByEmail("delete@example.com")
	assert.Error(t, err)
}

// ---------- Admin Routes ----------

func TestListUsers_AsAdmin(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestUser(t, store, "user1@example.com", "password123", "user")
	createTestUser(t, store, "user2@example.com", "password123", "user")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "admin")
		c.Set("user_role", "admin")
		c.Next()
	})
	r.GET("/admin/users", handler.ListUsers)

	req, _ := http.NewRequest("GET", "/admin/users", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var resp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &resp))
	users := resp["users"].([]interface{})
	assert.GreaterOrEqual(t, len(users), 2)
}

func TestAdminDeleteUser(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestUser(t, store, "victim@example.com", "password123", "user")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "admin")
		c.Set("user_role", "admin")
		c.Next()
	})
	r.DELETE("/admin/users/:id", handler.AdminDeleteUser)

	req, _ := http.NewRequest("DELETE", "/admin/users/usr_victim@example.com", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestUpdateUserStatus(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestUser(t, store, "target@example.com", "password123", "user")

	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", "admin")
		c.Set("user_role", "admin")
		c.Next()
	})
	r.PUT("/admin/users/:id", handler.UpdateUserStatus)

	body, _ := json.Marshal(map[string]string{"status": "suspended"})
	req, _ := http.NewRequest("PUT", "/admin/users/usr_target@example.com", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify in DB
	user, _ := store.GetUserByID("usr_target@example.com")
	assert.Equal(t, "suspended", user.Status)
}

// ---------- RefreshToken ----------

func TestRefreshToken_Success(t *testing.T) {
	handler, store, cleanup := setupTestEnv(t)
	defer cleanup()

	createTestUser(t, store, "refresh@example.com", "password123", "user")

	// First login to get a refresh token
	r := gin.New()
	r.POST("/auth/login", handler.Login)
	r.POST("/auth/refresh", handler.RefreshToken)

	body, _ := json.Marshal(LoginRequest{
		Email:    "refresh@example.com",
		Password: "password123",
	})
	req, _ := http.NewRequest("POST", "/auth/login", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)
	require.Equal(t, http.StatusOK, w.Code)

	var loginResp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &loginResp))
	refreshToken := loginResp["refresh_token"].(string)

	// Now use refresh token
	body, _ = json.Marshal(RefreshRequest{RefreshToken: refreshToken})
	req, _ = http.NewRequest("POST", "/auth/refresh", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w = httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var refreshResp map[string]interface{}
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &refreshResp))
	assert.NotEmpty(t, refreshResp["token"])
	assert.NotEmpty(t, refreshResp["refresh_token"])
}

func TestRefreshToken_Invalid(t *testing.T) {
	handler, _, cleanup := setupTestEnv(t)
	defer cleanup()

	r := gin.New()
	r.POST("/auth/refresh", handler.RefreshToken)

	body, _ := json.Marshal(RefreshRequest{RefreshToken: "invalid_token"})
	req, _ := http.NewRequest("POST", "/auth/refresh", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}
