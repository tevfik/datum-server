package main

import (
	"net/http"
	"time"

	"datum-go/internal/auth"
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
