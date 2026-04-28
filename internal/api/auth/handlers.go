// Package auth provides HTTP handlers for authentication endpoints.
package auth

import (
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"net/http"
	"strings"
	"sync"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/email"
	"datum-go/internal/logger"
	"datum-go/internal/notify"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

// resetRateLimit enforces per-email throttling on password reset requests.
// Allows at most 3 reset requests per email per 15 minutes.
var resetRateLimit = struct {
	mu       sync.Mutex
	attempts map[string][]time.Time
}{attempts: make(map[string][]time.Time)}

func isResetRateLimited(email string) bool {
	const maxAttempts = 3
	const window = 15 * time.Minute

	resetRateLimit.mu.Lock()
	defer resetRateLimit.mu.Unlock()

	now := time.Now()
	recent := resetRateLimit.attempts[email][:0]
	for _, t := range resetRateLimit.attempts[email] {
		if now.Sub(t) < window {
			recent = append(recent, t)
		}
	}
	resetRateLimit.attempts[email] = recent

	if len(recent) >= maxAttempts {
		return true
	}
	resetRateLimit.attempts[email] = append(recent, now)
	return false
}

// Handler provides authentication HTTP handlers.
type Handler struct {
	Store        storage.Provider
	EmailService *email.EmailSender
	Notifier     *notify.NtfyClient
	Dispatcher   *notify.Dispatcher
	PublicURL    string
}

// NewHandler creates a new auth handler with dependencies.
func NewHandler(store storage.Provider, emailService *email.EmailSender, notifier *notify.NtfyClient, dispatcher *notify.Dispatcher, publicURL string) *Handler {
	return &Handler{
		Store:        store,
		EmailService: emailService,
		Notifier:     notifier,
		Dispatcher:   dispatcher,
		PublicURL:    publicURL,
	}
}

// RegisterRoutes registers all auth routes.
func (h *Handler) RegisterRoutes(r gin.IRouter, authMiddleware gin.HandlerFunc) {
	authGroup := r.Group("/auth")
	{
		authGroup.POST("/register", h.Register)
		authGroup.POST("/login", h.Login)
		authGroup.POST("/refresh", h.RefreshToken)
		authGroup.POST("/forgot-password", h.ForgotPassword)
		authGroup.POST("/reset-password", h.CompleteResetPassword)

		// OAuth
		authGroup.GET("/providers", h.GetOAuthProviders)
		authGroup.GET("/oauth/:provider", h.OAuthRedirect)
		authGroup.GET("/oauth/:provider/callback", h.OAuthCallback)
	}

	// Authenticated user routes
	authProtectedGroup := r.Group("/auth")
	authProtectedGroup.Use(authMiddleware)
	{
		authProtectedGroup.GET("/me", h.GetMe)
		authProtectedGroup.PUT("/me", h.UpdateMe)
		authProtectedGroup.PUT("/password", h.ChangePassword)
		authProtectedGroup.POST("/logout", h.Logout)
		authProtectedGroup.GET("/sessions", h.ListSessions)
		authProtectedGroup.DELETE("/sessions/:jti", h.RevokeSession)
		authProtectedGroup.DELETE("/user", h.DeleteSelf)

		// Push notification tokens
		authProtectedGroup.GET("/push-tokens", h.ListPushTokens)
		authProtectedGroup.POST("/push-token", h.RegisterPushToken)
		authProtectedGroup.DELETE("/push-token/:id", h.DeletePushToken)
	}
}

// RegisterAdminRoutes registers admin authentication/user routes.
func (h *Handler) RegisterAdminRoutes(r *gin.RouterGroup) {
	r.GET("", h.ListUsers)
	r.DELETE("/:id", h.AdminDeleteUser)
	r.PUT("/:id", h.UpdateUserStatus)
}

// ============ Request/Response types ============

type RegisterRequest struct {
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required,min=8"`
	Name     string `json:"name"`
}

type LoginRequest struct {
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required"`
}

type RefreshRequest struct {
	RefreshToken string `json:"refresh_token" binding:"required"`
}

type ChangePasswordRequest struct {
	OldPassword string `json:"old_password" binding:"required"`
	NewPassword string `json:"new_password" binding:"required,min=8"`
}

type ForgotPasswordRequest struct {
	Email string `json:"email" binding:"required,email"`
}

type TokenResetPasswordRequest struct {
	Token       string `json:"token" binding:"required"`
	NewPassword string `json:"new_password" binding:"required,min=8"`
}

type UpdateMeRequest struct {
	DisplayName string `json:"display_name"`
}

type PushTokenRequest struct {
	Platform string `json:"platform" binding:"required,oneof=fcm apns ntfy"`
	Token    string `json:"token" binding:"required"`
}

// authResponse is the standard login/register response.
type authResponse struct {
	Token        string    `json:"token"`
	RefreshToken string    `json:"refresh_token"`
	UserID       string    `json:"user_id"`
	Email        string    `json:"email"`
	Role         string    `json:"role"`
	ExpiresAt    time.Time `json:"expires_at"`
}

// userResponse is returned from /auth/me.
type userResponse struct {
	ID          string    `json:"id"`
	Email       string    `json:"email"`
	Role        string    `json:"role"`
	Status      string    `json:"status"`
	DisplayName string    `json:"display_name,omitempty"`
	NtfyTopic   string    `json:"ntfy_topic,omitempty"`
	CreatedAt   time.Time `json:"created_at"`
	UpdatedAt   time.Time `json:"updated_at,omitempty"`
	LastLoginAt time.Time `json:"last_login_at,omitempty"`
}

// ============ Helpers ============

func generateID(prefix string) string {
	bytes := make([]byte, 8)
	rand.Read(bytes)
	return fmt.Sprintf("%s_%s", prefix, hex.EncodeToString(bytes))
}

// ntfyTopicForUser derives a stable, non-guessable ntfy topic from user ID.
// Uses a consistent prefix so it's re-derivable without DB storage.
func ntfyTopicForUser(userID string) string {
	return "datum-" + userID
}

func issueTokenPair(store storage.Provider, user *storage.User, userAgent, ip string) (*authResponse, error) {
	accessToken, refreshToken, jti, err := auth.GenerateTokenPair(user.ID, user.Email, user.Role)
	if err != nil {
		return nil, err
	}

	// Create multi-device session entry
	session := &storage.Session{
		JTI:       jti,
		UserID:    user.ID,
		CreatedAt: time.Now(),
		ExpiresAt: time.Now().Add(30 * 24 * time.Hour),
		UserAgent: userAgent,
		IP:        ip,
	}
	if err := store.CreateSession(session); err != nil {
		return nil, err
	}

	// Keep legacy single-token field for backward compat
	store.UpdateUserRefreshToken(user.ID, refreshToken)

	return &authResponse{
		Token:        accessToken,
		RefreshToken: refreshToken,
		UserID:       user.ID,
		Email:        user.Email,
		Role:         user.Role,
		ExpiresAt:    time.Now().Add(15 * time.Minute),
	}, nil
}

// ============ Auth Handlers ============

// Register handles user registration.
// POST /auth/register
func (h *Handler) Register(c *gin.Context) {
	if !h.Store.IsSystemInitialized() {
		c.JSON(http.StatusForbidden, gin.H{
			"error":          "System not initialized. Please complete setup first.",
			"setup_required": true,
		})
		return
	}

	config, _ := h.Store.GetSystemConfig()
	if !config.AllowRegister {
		c.JSON(http.StatusForbidden, gin.H{"error": "Public registration is disabled. Contact administrator."})
		return
	}

	var req RegisterRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	req.Email = strings.ToLower(req.Email)

	hashedPassword, err := auth.HashPassword(req.Password)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to hash password"})
		return
	}

	user := &storage.User{
		ID:           generateID("usr"),
		Email:        req.Email,
		PasswordHash: hashedPassword,
		DisplayName:  req.Name,
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}

	if err := h.Store.CreateUser(user); err != nil {
		c.JSON(http.StatusConflict, gin.H{"error": "User already exists"})
		return
	}

	resp, err := issueTokenPair(h.Store, user, c.GetHeader("User-Agent"), c.ClientIP())
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate token"})
		return
	}

	c.JSON(http.StatusCreated, resp)
}

// Login handles user login.
// POST /auth/login
func (h *Handler) Login(c *gin.Context) {
	var req LoginRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	req.Email = strings.ToLower(req.Email)

	user, err := h.Store.GetUserByEmail(req.Email)
	if err != nil {
		logger.GetLogger().Warn().Str("email", req.Email).Err(err).Msg("Login failed: User not found")
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid credentials"})
		return
	}

	if user.Status == "suspended" {
		c.JSON(http.StatusForbidden, gin.H{"error": "Account suspended. Contact administrator."})
		return
	}

	if !auth.CheckPassword(user.PasswordHash, req.Password) {
		logger.GetLogger().Warn().Str("email", req.Email).Msg("Login failed: Password mismatch")
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid credentials"})
		return
	}

	h.Store.UpdateUserLastLogin(user.ID)

	resp, err := issueTokenPair(h.Store, user, c.GetHeader("User-Agent"), c.ClientIP())
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate token"})
		return
	}

	c.JSON(http.StatusOK, resp)
}

// RefreshToken handles token refresh with session rotation.
// POST /auth/refresh
func (h *Handler) RefreshToken(c *gin.Context) {
	var req RefreshRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	claims, err := auth.ValidateToken(req.RefreshToken)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid refresh token"})
		return
	}

	sub, _ := claims.GetSubject()
	if sub != "refresh" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Not a refresh token"})
		return
	}

	// Validate session exists (multi-device support)
	jti := claims.ID
	if jti == "" {
		// Legacy token without JTI: fall back to single-token DB check
		user, err := h.Store.GetUserByID(claims.UserID)
		if err != nil || user.RefreshToken != req.RefreshToken {
			logger.GetLogger().Warn().Str("user_id", claims.UserID).Msg("Refresh token mismatch (legacy)")
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid refresh token"})
			return
		}
	} else {
		if _, err := h.Store.GetSession(jti); err != nil {
			logger.GetLogger().Warn().Str("jti", jti).Msg("Session not found — possible token reuse attack")
			// Revoke all sessions for this user as a precaution
			h.Store.DeleteAllUserSessions(claims.UserID)
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid refresh token (revoked)"})
			return
		}
		// Delete old session (rotation)
		h.Store.DeleteSession(jti)
	}

	user, err := h.Store.GetUserByID(claims.UserID)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "User not found"})
		return
	}

	resp, err := issueTokenPair(h.Store, user, c.GetHeader("User-Agent"), c.ClientIP())
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate tokens"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"token":         resp.Token,
		"refresh_token": resp.RefreshToken,
		"expires_at":    resp.ExpiresAt,
	})
}

// Logout revokes the current session.
// POST /auth/logout
func (h *Handler) Logout(c *gin.Context) {
	jti := auth.GetSessionJTI(c)
	if jti != "" {
		h.Store.DeleteSession(jti)
	}
	c.JSON(http.StatusOK, gin.H{"message": "Logged out successfully"})
}

// RevokeSession deletes a specific session by JTI (for multi-device logout).
// DELETE /auth/sessions/:jti
func (h *Handler) RevokeSession(c *gin.Context) {
	userID := c.GetString("user_id")
	jti := c.Param("jti")

	// Verify the session belongs to the requesting user
	session, err := h.Store.GetSession(jti)
	if err != nil || session.UserID != userID {
		c.JSON(http.StatusNotFound, gin.H{"error": "Session not found"})
		return
	}

	h.Store.DeleteSession(jti)
	c.JSON(http.StatusOK, gin.H{"message": "Session revoked"})
}

// ListSessions returns active sessions for the current user.
// GET /auth/sessions
func (h *Handler) ListSessions(c *gin.Context) {
	userID := c.GetString("user_id")
	sessions, err := h.Store.GetUserSessions(userID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to list sessions"})
		return
	}
	if sessions == nil {
		sessions = []*storage.Session{}
	}
	c.JSON(http.StatusOK, gin.H{"sessions": sessions})
}

// GetMe returns the authenticated user's profile.
// GET /auth/me
func (h *Handler) GetMe(c *gin.Context) {
	userID := c.GetString("user_id")
	user, err := h.Store.GetUserByID(userID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	resp := userResponse{
		ID:          user.ID,
		Email:       user.Email,
		Role:        user.Role,
		Status:      user.Status,
		DisplayName: user.DisplayName,
		CreatedAt:   user.CreatedAt,
		UpdatedAt:   user.UpdatedAt,
		LastLoginAt: user.LastLoginAt,
	}
	if h.Notifier != nil {
		resp.NtfyTopic = ntfyTopicForUser(user.ID)
	}

	c.JSON(http.StatusOK, resp)
}

// UpdateMe updates the authenticated user's profile.
// PUT /auth/me
func (h *Handler) UpdateMe(c *gin.Context) {
	userID := c.GetString("user_id")
	var req UpdateMeRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if err := h.Store.UpdateUserProfile(userID, req.DisplayName); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update profile"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Profile updated"})
}

// ChangePassword handles password updates.
// PUT /auth/password
func (h *Handler) ChangePassword(c *gin.Context) {
	userID := c.GetString("user_id")

	var req ChangePasswordRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	user, err := h.Store.GetUserByID(userID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	if !auth.CheckPassword(user.PasswordHash, req.OldPassword) {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid current password"})
		return
	}

	hashedPassword, err := auth.HashPassword(req.NewPassword)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to hash password"})
		return
	}

	if err := h.Store.UpdateUserPassword(userID, hashedPassword); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update password"})
		return
	}

	// Revoke all other sessions after password change
	h.Store.DeleteAllUserSessions(userID)

	c.JSON(http.StatusOK, gin.H{"message": "Password updated. All other sessions have been revoked."})
}

// DeleteSelf handles account deletion.
// DELETE /auth/user
func (h *Handler) DeleteSelf(c *gin.Context) {
	userID := c.GetString("user_id")
	h.Store.DeleteAllUserSessions(userID)
	if err := h.Store.DeleteUser(userID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to delete account"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"message": "Account deleted successfully"})
}

// ============ Push Notification Handlers ============

// ListPushTokens returns all push tokens for the current user.
// GET /auth/push-tokens
func (h *Handler) ListPushTokens(c *gin.Context) {
	userID := c.GetString("user_id")
	tokens, err := h.Store.GetUserPushTokens(userID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to list push tokens"})
		return
	}
	if tokens == nil {
		tokens = []*storage.PushToken{}
	}
	c.JSON(http.StatusOK, gin.H{"tokens": tokens})
}

// RegisterPushToken registers a push notification token for the current user.
// POST /auth/push-token
func (h *Handler) RegisterPushToken(c *gin.Context) {
	userID := c.GetString("user_id")
	var req PushTokenRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	pt := &storage.PushToken{
		ID:        uuid.New().String(),
		UserID:    userID,
		Platform:  req.Platform,
		Token:     req.Token,
		CreatedAt: time.Now(),
	}
	if err := h.Store.SavePushToken(pt); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to save push token"})
		return
	}

	c.JSON(http.StatusCreated, gin.H{"id": pt.ID, "message": "Push token registered"})
}

// DeletePushToken removes a push notification token.
// DELETE /auth/push-token/:id
func (h *Handler) DeletePushToken(c *gin.Context) {
	userID := c.GetString("user_id")
	tokenID := c.Param("id")
	if err := h.Store.DeletePushToken(userID, tokenID); err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Token not found"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"message": "Push token removed"})
}

// ============ Password Reset Handlers ============

// ForgotPassword handles password reset requests.
// POST /auth/forgot-password
func (h *Handler) ForgotPassword(c *gin.Context) {
	var req ForgotPasswordRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	req.Email = strings.ToLower(req.Email)

	// Anti-timing: always take at least 200ms
	defer func(start time.Time) {
		if elapsed := time.Since(start); elapsed < 200*time.Millisecond {
			time.Sleep(200*time.Millisecond - elapsed)
		}
	}(time.Now())

	const successMsg = "If an account exists with this email, a reset link has been sent."

	if isResetRateLimited(req.Email) {
		c.JSON(http.StatusOK, gin.H{"message": successMsg})
		return
	}

	user, err := h.Store.GetUserByEmail(req.Email)
	if err != nil {
		c.JSON(http.StatusOK, gin.H{"message": successMsg})
		return
	}

	tokenBytes := make([]byte, 32)
	if _, err := rand.Read(tokenBytes); err != nil {
		c.JSON(http.StatusOK, gin.H{"message": successMsg})
		return
	}
	token := hex.EncodeToString(tokenBytes)

	if err := h.Store.SavePasswordResetToken(user.ID, token, 1*time.Hour); err != nil {
		c.JSON(http.StatusOK, gin.H{"message": successMsg})
		return
	}

	if h.EmailService != nil {
		go func() {
			if err := h.EmailService.SendResetEmail(user.Email, token); err != nil {
				logger.GetLogger().Error().Err(err).Str("email", user.Email).Msg("Failed to send password reset email")
			}
		}()
	}

	// Push notification to all user channels (mobile devices + ntfy)
	if h.Dispatcher != nil {
		h.Dispatcher.NotifyUser(user.ID,
			"Password Reset Requested",
			"A password reset was requested for your Datum account. If this wasn't you, contact support.",
			notify.PriorityDefault)
	}

	c.JSON(http.StatusOK, gin.H{"message": successMsg})
}

// CompleteResetPassword handles the actual password reset using a token.
// POST /auth/reset-password
func (h *Handler) CompleteResetPassword(c *gin.Context) {
	var req TokenResetPasswordRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	user, err := h.Store.GetUserByResetToken(req.Token)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid or expired token"})
		return
	}

	// Delete token first (single-use, race-safe)
	h.Store.DeleteResetToken(req.Token)

	// Revoke all sessions after password reset
	h.Store.DeleteAllUserSessions(user.ID)

	hashedPassword, err := auth.HashPassword(req.NewPassword)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to hash password"})
		return
	}

	if err := h.Store.UpdateUserPassword(user.ID, hashedPassword); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update password"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Password updated successfully. You can now log in."})
}

// ============ Admin Handlers ============

// ListUsers returns all users (admin only).
// GET /admin/users
func (h *Handler) ListUsers(c *gin.Context) {
	users, err := h.Store.ListAllUsers()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to list users"})
		return
	}

	type userWithDeviceCount struct {
		storage.User
		DeviceCount int `json:"device_count"`
	}

	result := make([]userWithDeviceCount, len(users))
	for i, u := range users {
		u.PasswordHash = ""
		u.RefreshToken = ""
		deviceCount := 0
		if devices, err := h.Store.GetUserDevices(u.ID); err == nil {
			deviceCount = len(devices)
		}
		result[i] = userWithDeviceCount{User: u, DeviceCount: deviceCount}
	}
	c.JSON(http.StatusOK, gin.H{"users": result})
}

// AdminDeleteUser deletes a user by ID (admin only).
// DELETE /admin/users/:id
func (h *Handler) AdminDeleteUser(c *gin.Context) {
	userID := c.Param("id")
	if userID == c.GetString("user_id") {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Cannot delete your own account via admin endpoint"})
		return
	}
	h.Store.DeleteAllUserSessions(userID)
	if err := h.Store.DeleteUser(userID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to delete user"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"message": "User deleted"})
}

// UpdateUserStatus updates a user's status (admin only).
// PUT /admin/users/:id
func (h *Handler) UpdateUserStatus(c *gin.Context) {
	userID := c.Param("id")
	var req struct {
		Status string `json:"status" binding:"required,oneof=active suspended"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	user, err := h.Store.GetUserByID(userID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	if err := h.Store.UpdateUser(userID, user.Role, req.Status); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update user status"})
		return
	}

	// If suspending, revoke all sessions
	if req.Status == "suspended" {
		h.Store.DeleteAllUserSessions(userID)
	}

	c.JSON(http.StatusOK, gin.H{"message": "User status updated"})
}
