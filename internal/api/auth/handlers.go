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
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
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
	// Prune expired entries
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
	PublicURL    string
}

// NewHandler creates a new auth handler with dependencies.
func NewHandler(store storage.Provider, emailService *email.EmailSender, publicURL string) *Handler {
	return &Handler{
		Store:        store,
		EmailService: emailService,
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
	}

	// Password Reset Web Page (for deep linking fallback)
	r.GET("/reset-password", h.ResetPasswordWeb)

	// Authenticated user routes
	authProtectedGroup := r.Group("/auth")
	authProtectedGroup.Use(authMiddleware)
	{
		authProtectedGroup.PUT("/password", h.ChangePassword)
		authProtectedGroup.DELETE("/user", h.DeleteSelf)
	}
}

// RegisterAdminRoutes registers admin authentication/user routes.
func (h *Handler) RegisterAdminRoutes(r *gin.RouterGroup) {
	r.GET("", h.ListUsers)
	r.DELETE("/:id", h.AdminDeleteUser)
	r.PUT("/:id", h.UpdateUserStatus)
}

// ============ Request/Response types ============

// RegisterRequest holds registration data.
type RegisterRequest struct {
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required,min=8"`
}

// LoginRequest holds login data.
type LoginRequest struct {
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required"`
}

// RefreshRequest holds refresh token data.
type RefreshRequest struct {
	RefreshToken string `json:"refresh_token" binding:"required"`
}

// ChangePasswordRequest holds password change data.
type ChangePasswordRequest struct {
	OldPassword string `json:"old_password" binding:"required"`
	NewPassword string `json:"new_password" binding:"required,min=8"`
}

// ForgotPasswordRequest holds forgot password data.
type ForgotPasswordRequest struct {
	Email string `json:"email" binding:"required,email"`
}

// TokenResetPasswordRequest holds token-based password reset data.
type TokenResetPasswordRequest struct {
	Token       string `json:"token" binding:"required"`
	NewPassword string `json:"new_password" binding:"required,min=8"`
}

// ============ Handlers ============

// Register handles user registration.
// POST /auth/register
func (h *Handler) Register(c *gin.Context) {
	// Check if system is initialized
	if !h.Store.IsSystemInitialized() {
		c.JSON(http.StatusForbidden, gin.H{
			"error":          "System not initialized. Please complete setup first.",
			"setup_required": true,
		})
		return
	}

	// Check if registration is allowed
	config, _ := h.Store.GetSystemConfig()
	if !config.AllowRegister {
		c.JSON(http.StatusForbidden, gin.H{
			"error": "Public registration is disabled. Contact administrator.",
		})
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

	userID := generateID("usr")
	user := &storage.User{
		ID:           userID,
		Email:        req.Email,
		PasswordHash: hashedPassword,
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}

	if err := h.Store.CreateUser(user); err != nil {
		c.JSON(http.StatusConflict, gin.H{"error": "User already exists"})
		return
	}

	token, err := auth.GenerateToken(userID, req.Email, "user")
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate token"})
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"user_id": userID,
		"token":   token,
		"role":    "user",
	})
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
		logger.GetLogger().Warn().Str("email", req.Email).Msg("Login failed: User suspended")
		c.JSON(http.StatusForbidden, gin.H{"error": "Account suspended. Contact administrator."})
		return
	}

	if !auth.CheckPassword(user.PasswordHash, req.Password) {
		logger.GetLogger().Warn().Str("email", req.Email).Msg("Login failed: Password mismatch")
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid credentials"})
		return
	}

	h.Store.UpdateUserLastLogin(user.ID)

	accessToken, refreshToken, err := auth.GenerateTokenPair(user.ID, user.Email, user.Role)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate token"})
		return
	}

	if err := h.Store.UpdateUserRefreshToken(user.ID, refreshToken); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to save session"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"token":         accessToken,
		"refresh_token": refreshToken,
		"user_id":       user.ID,
		"email":         user.Email,
		"role":          user.Role,
		"expires_at":    time.Now().Add(15 * time.Minute).Format(time.RFC3339),
	})
}

// RefreshToken handles token refresh.
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

	user, err := h.Store.GetUserByID(claims.UserID)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "User not found"})
		return
	}

	if user.RefreshToken != req.RefreshToken {
		logger.GetLogger().Warn().Str("user_id", user.ID).Msg("Refresh token mismatch (potential reuse)")
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid refresh token (revoked)"})
		return
	}

	accessToken, newRefreshToken, err := auth.GenerateTokenPair(user.ID, user.Email, user.Role)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate tokens"})
		return
	}

	if err := h.Store.UpdateUserRefreshToken(user.ID, newRefreshToken); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to save refresh token"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"token":         accessToken,
		"refresh_token": newRefreshToken,
	})
}

// ChangePassword handles password updates.
// PUT /auth/password
func (h *Handler) ChangePassword(c *gin.Context) {
	userID := c.GetString("user_id")
	if userID == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

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
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid old password"})
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

	c.JSON(http.StatusOK, gin.H{"status": "Password updated successfully"})
}

// DeleteSelf handles account deletion.
// DELETE /auth/user
func (h *Handler) DeleteSelf(c *gin.Context) {
	userID := c.GetString("user_id")
	if userID == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

	if err := h.Store.DeleteUser(userID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to delete account"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "Account deleted successfully"})
}

// ResetPasswordWeb serves the HTML page for password reset (deep link fallback).
// GET /reset-password
func (h *Handler) ResetPasswordWeb(c *gin.Context) {
	token := c.Query("token")

	appSchemeURL := strings.Replace(h.PublicURL, "https://", "datum://", 1)
	appSchemeURL = strings.Replace(appSchemeURL, "http://", "datum://", 1)

	c.Header("Content-Type", "text/html")
	c.String(http.StatusOK, fmt.Sprintf(`
<!DOCTYPE html>
<html>
<head>
	<title>Reset Password</title>
	<meta name="viewport" content="width=device-width, initial-scale=1">
	<style>
		body { font-family: sans-serif; text-align: center; padding: 20px; background: #f4f4f4; }
		.container { background: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); max-width: 400px; margin: 0 auto; }
		h2 { color: #333; }
		p { color: #666; }
		.btn { display: inline-block; background: #007bff; color: white; padding: 12px 24px; text-decoration: none; border-radius: 4px; margin-top: 20px; font-weight: bold; }
		.footer-link { display: block; margin-top: 15px; color: #999; font-size: 12px; text-decoration: none; }
	</style>
</head>
<body>
	<div class="container">
		<h2>Reset Password</h2>
		<p>To reset your password, please open this link in the Datum Mobile App.</p>
		<p>If the app did not open automatically, tap the button below:</p>
		<a href="%s/reset-password?token=%s" class="btn">Open in App</a>
		<a href="%s/reset-password?token=%s" class="footer-link">Or try universal link</a>
	</div>
	<script>
		window.location.href = "%s/reset-password?token=%s";
	</script>
</body>
</html>
`, appSchemeURL, token, h.PublicURL, token, appSchemeURL, token))
}

// ForgotPassword handles password reset requests.
// POST /auth/forgot-password
func (h *Handler) ForgotPassword(c *gin.Context) {
	var req ForgotPasswordRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	req.Email = strings.ToLower(req.Email)

	// Always respond with the same message and timing to prevent email enumeration
	defer func(start time.Time) {
		elapsed := time.Since(start)
		minDuration := 200 * time.Millisecond
		if elapsed < minDuration {
			time.Sleep(minDuration - elapsed)
		}
	}(time.Now())

	const successMsg = "If an account exists with this email, a reset link has been sent."

	// Per-email rate limiting (silent — returns same success message)
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
		// Don't reveal internal error
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

	// Delete the token first to ensure single-use (prevent race conditions)
	h.Store.DeleteResetToken(req.Token)

	hashedPassword, err := auth.HashPassword(req.NewPassword)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to hash password"})
		return
	}

	if err := h.Store.UpdateUserPassword(user.ID, hashedPassword); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update password"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Password updated successfully. You can now login."})
}

// ListUsers returns all users (admin only).
// GET /admin/users
func (h *Handler) ListUsers(c *gin.Context) {
	users, err := h.Store.ListAllUsers() // Using ListAllUsers from interface
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to list users"})
		return
	}
	// Sanitize - remove password hashes
	for i := range users {
		users[i].PasswordHash = ""
	}
	c.JSON(http.StatusOK, gin.H{"users": users})
}

// AdminDeleteUser deletes a user by ID (admin only).
// DELETE /admin/users/:id
func (h *Handler) AdminDeleteUser(c *gin.Context) {
	userID := c.Param("id")
	if userID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "User ID required"})
		return
	}

	// Prevent deleting self? Frontend handles it, but good to enforce.
	selfID := c.GetString("user_id")
	if userID == selfID {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Cannot delete your own account via admin endpoint"})
		return
	}

	if err := h.Store.DeleteUser(userID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to delete user"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"status": "User deleted"})
}

// UpdateUserStatus updates a user's status (active/suspended) (admin only).
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
	c.JSON(http.StatusOK, gin.H{"status": "User status updated"})
}

// ============ Helper Functions ============

func generateID(prefix string) string {
	bytes := make([]byte, 8)
	rand.Read(bytes)
	return fmt.Sprintf("%s_%s", prefix, hex.EncodeToString(bytes))
}
