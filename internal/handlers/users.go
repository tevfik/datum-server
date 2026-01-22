package handlers

import (
	"net/http"
	"strings"

	"datum-go/internal/auth"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// ============ User Management Handlers ============

type CreateUserRequest struct {
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required,min=8"`
	Role     string `json:"role"`
}

func (h *AdminHandler) CreateUserHandler(c *gin.Context) {
	var req CreateUserRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	req.Email = strings.ToLower(req.Email)

	// Set defaults
	if req.Role == "" {
		req.Role = "user"
	}

	// Validate role
	if req.Role != "admin" && req.Role != "user" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid role. Must be 'admin' or 'user'"})
		return
	}

	// Hash password
	hashedPassword, err := auth.HashPassword(req.Password)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to hash password"})
		return
	}

	userID := generateIDString(16)
	user := &storage.User{
		ID:           userID,
		Email:        req.Email,
		PasswordHash: hashedPassword,
		Role:         req.Role,
		Status:       "active",
		CreatedAt:    timeNow(),
	}

	if err := h.Store.CreateUser(user); err != nil {
		c.JSON(http.StatusConflict, gin.H{"error": "Email already exists"})
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"message": "User created",
		"user_id": userID,
		"email":   req.Email,
		"role":    req.Role,
	})
}

func (h *AdminHandler) ListUsersHandler(c *gin.Context) {
	users, err := h.Store.ListAllUsers()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	deviceCounts, err := h.Store.GetUserDeviceCounts()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	// Remove password hashes from response
	var safeUsers []gin.H
	for _, u := range users {
		safeUsers = append(safeUsers, gin.H{
			"id":            u.ID,
			"email":         u.Email,
			"role":          u.Role,
			"status":        u.Status,
			"created_at":    u.CreatedAt,
			"updated_at":    u.UpdatedAt,
			"last_login_at": u.LastLoginAt,
			"device_count":  deviceCounts[u.ID],
		})
	}

	c.JSON(http.StatusOK, gin.H{"users": safeUsers})
}

func (h *AdminHandler) GetUserHandler(c *gin.Context) {
	userID := c.Param("user_id")

	user, err := h.Store.GetUserByID(userID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	devices, _ := h.Store.GetUserDevices(userID)

	c.JSON(http.StatusOK, gin.H{
		"id":            user.ID,
		"email":         user.Email,
		"role":          user.Role,
		"status":        user.Status,
		"created_at":    user.CreatedAt,
		"updated_at":    user.UpdatedAt,
		"last_login_at": user.LastLoginAt,
		"devices":       devices,
	})
}

type UpdateUserRequest struct {
	Role   string `json:"role"`   // "admin", "user"
	Status string `json:"status"` // "active", "suspended"
}

func (h *AdminHandler) UpdateUserHandler(c *gin.Context) {
	userID := c.Param("user_id")

	var req UpdateUserRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Validate role and status
	if req.Role != "" && req.Role != "admin" && req.Role != "user" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid role. Must be 'admin' or 'user'"})
		return
	}
	if req.Status != "" && req.Status != "active" && req.Status != "suspended" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid status. Must be 'active' or 'suspended'"})
		return
	}

	// Prevent admin from suspending or demoting themselves
	currentUserID := c.GetString("user_id")
	if userID == currentUserID {
		if req.Status == "suspended" {
			c.JSON(http.StatusForbidden, gin.H{"error": "Cannot suspend your own account"})
			return
		}
		if req.Role == "user" {
			c.JSON(http.StatusForbidden, gin.H{"error": "Cannot demote your own account"})
			return
		}
	}

	// Prevent demoting the last admin
	if req.Role == "user" {
		stats, _ := h.Store.GetDatabaseStats()
		adminCount, _ := stats["admin_users"].(int)
		if adminCount <= 1 {
			user, _ := h.Store.GetUserByID(userID)
			if user.Role == "admin" {
				c.JSON(http.StatusForbidden, gin.H{"error": "Cannot demote the last admin"})
				return
			}
		}
	}

	if err := h.Store.UpdateUser(userID, req.Role, req.Status); err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "User updated"})
}

func (h *AdminHandler) DeleteUserHandler(c *gin.Context) {
	userID := c.Param("user_id")
	currentUserID := c.GetString("user_id")

	// Prevent self-deletion
	if userID == currentUserID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Cannot delete yourself"})
		return
	}

	// Prevent deleting the last admin
	user, err := h.Store.GetUserByID(userID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	if user.Role == "admin" {
		stats, _ := h.Store.GetDatabaseStats()
		adminCount, _ := stats["admin_users"].(int)
		if adminCount <= 1 {
			c.JSON(http.StatusForbidden, gin.H{"error": "Cannot delete the last admin"})
			return
		}
	}

	if err := h.Store.DeleteUser(userID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "User and associated devices deleted"})
}

type ResetPasswordRequest struct {
	NewPassword string `json:"new_password"`
}

func (h *AdminHandler) ResetPasswordHandler(c *gin.Context) {
	username := c.Param("username")

	var req ResetPasswordRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// If no password provided, generate random one
	if req.NewPassword == "" {
		req.NewPassword = generateIDString(8) // 16 char hex string
	}

	// Get user
	user, err := h.Store.GetUserByEmail(username)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	// Hash new password
	hashedPassword, err := auth.HashPassword(req.NewPassword)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to hash password"})
		return
	}

	// Update password
	if err := h.Store.UpdateUserPassword(user.ID, hashedPassword); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	// Generate new token for immediate use
	newToken, _ := auth.GenerateToken(user.ID, user.Email, user.Role)

	c.JSON(http.StatusOK, gin.H{
		"message":      "Password reset successfully",
		"new_password": req.NewPassword,
		"token":        newToken,
	})
}
