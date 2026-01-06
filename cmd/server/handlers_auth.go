package main

import (
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"net/http"
	"strings"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/logger"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// ============ Auth Request/Response types ============

type RegisterRequest struct {
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required,min=8"`
}

type LoginRequest struct {
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required"`
}

// ============ Auth Handlers ============

// registerHandler handles user registration
// POST /auth/register
func registerHandler(c *gin.Context) {
	// Check if system is initialized
	if !store.IsSystemInitialized() {
		c.JSON(http.StatusForbidden, gin.H{
			"error":          "System not initialized. Please complete setup first.",
			"setup_required": true,
		})
		return
	}

	// Check if registration is allowed
	config, _ := store.GetSystemConfig()
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
		Role:         "user",   // Default role for registered users
		Status:       "active", // Active by default
		CreatedAt:    time.Now(),
	}

	if err := store.CreateUser(user); err != nil {
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

// loginHandler handles user login
// POST /auth/login
func loginHandler(c *gin.Context) {
	var req LoginRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	req.Email = strings.ToLower(req.Email)

	user, err := store.GetUserByEmail(req.Email)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid credentials"})
		return
	}

	// Check if user is suspended
	if user.Status == "suspended" {
		c.JSON(http.StatusForbidden, gin.H{"error": "Account suspended. Contact administrator."})
		return
	}

	if !auth.CheckPassword(user.PasswordHash, req.Password) {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid credentials"})
		return
	}

	// Update last login
	store.UpdateUserLastLogin(user.ID)

	token, err := auth.GenerateToken(user.ID, user.Email, user.Role)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate token"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"token":      token,
		"user_id":    user.ID,
		"email":      user.Email,
		"role":       user.Role,
		"expires_at": time.Now().Add(24 * time.Hour).Format(time.RFC3339),
	})
}

type ChangePasswordRequest struct {
	OldPassword string `json:"old_password" binding:"required"`
	NewPassword string `json:"new_password" binding:"required,min=8"`
}

// changePasswordHandler handles password updates
// PUT /auth/password
func changePasswordHandler(c *gin.Context) {
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

	user, err := store.GetUserByID(userID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	// Verify old password
	if !auth.CheckPassword(user.PasswordHash, req.OldPassword) {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid old password"})
		return
	}

	// Hash new password
	hashedPassword, err := auth.HashPassword(req.NewPassword)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to hash password"})
		return
	}

	if err := store.UpdateUserPassword(userID, hashedPassword); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update password"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "Password updated successfully"})
}

// deleteSelfHandler handles account deletion
// DELETE /auth/user
func deleteSelfHandler(c *gin.Context) {
	userID := c.GetString("user_id")
	if userID == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

	if err := store.DeleteUser(userID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to delete account"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "Account deleted successfully"})
}

var GlobalPublicURL string

// resetPasswordWebHandler serves the HTML page for password reset (deep link fallback)
func resetPasswordWebHandler(c *gin.Context) {
	token := c.Query("token")

	// Create custom scheme URL (datum://...) from the PublicURL
	// Assuming GlobalPublicURL is e.g. "https://datum.bezg.in" -> "datum://datum.bezg.in"
	// This ensures the "Open in App" button forces the app to open via Scheme, bypassing HTTPS verification issues.
	appSchemeURL := strings.Replace(GlobalPublicURL, "https://", "datum://", 1)
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
					
					<!-- Primary Action: Custom Scheme (forces app open) -->
					<p>If the app did not open automatically, tap the button below:</p>
					<a href="%s/reset-password?token=%s" class="btn">Open in App</a>
					
					<!-- Fallback: Universal Link -->
					<a href="%s/reset-password?token=%s" class="footer-link">Or try universal link</a>
				</div>
				<script>
					// Auto-redirect to custom scheme
					window.location.href = "%s/reset-password?token=%s";
				</script>
			</body>
			</html>
		`, appSchemeURL, token, GlobalPublicURL, token, appSchemeURL, token))
}

// ============ Password Reset Handlers ============

type ForgotPasswordRequest struct {
	Email string `json:"email" binding:"required,email"`
}

type TokenResetPasswordRequest struct {
	Token       string `json:"token" binding:"required"`
	NewPassword string `json:"new_password" binding:"required,min=8"`
}

// forgotPasswordHandler handles password reset requests
// POST /auth/forgot-password
func forgotPasswordHandler(c *gin.Context) {
	var req ForgotPasswordRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	req.Email = strings.ToLower(req.Email)

	// Always return 200 to prevent user enumeration
	// We only log errors internally
	user, err := store.GetUserByEmail(req.Email)
	if err != nil {
		// User not found - simulate timing to avoid side-channel attacks (simple sleep)
		time.Sleep(100 * time.Millisecond)
		c.JSON(http.StatusOK, gin.H{"message": "If an account exists with this email, a reset link has been sent."})
		return
	}

	// Generate a random token (32 bytes = 64 hex chars)
	tokenBytes := make([]byte, 32)
	if _, err := rand.Read(tokenBytes); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate token"})
		return
	}
	token := hex.EncodeToString(tokenBytes)

	// Save token with 1 hour TTL
	if err := store.SavePasswordResetToken(user.ID, token, 1*time.Hour); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to save reset token"})
		return
	}

	// Send email
	if emailService != nil {
		go func() {
			if err := emailService.SendResetEmail(user.Email, token); err != nil {
				logger.GetLogger().Error().Err(err).Str("email", user.Email).Msg("Failed to send password reset email")
			}
		}()
	}

	c.JSON(http.StatusOK, gin.H{"message": "If an account exists with this email, a reset link has been sent."})
}

// completeResetPasswordHandler handles the actual password reset using a token
// POST /auth/reset-password
func completeResetPasswordHandler(c *gin.Context) {
	var req TokenResetPasswordRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	user, err := store.GetUserByResetToken(req.Token)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid or expired token"})
		return
	}

	// Hash new password
	hashedPassword, err := auth.HashPassword(req.NewPassword)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to hash password"})
		return
	}

	// Update password
	if err := store.UpdateUserPassword(user.ID, hashedPassword); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update password"})
		return
	}

	// Delete token (so it can't be reused)
	store.DeleteResetToken(req.Token)

	c.JSON(http.StatusOK, gin.H{"message": "Password updated successfully. You can now login."})
}
