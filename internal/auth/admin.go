package auth

import (
	"net/http"
	"strings"

	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/golang-jwt/jwt/v5"
)

// AdminMiddleware requires the user to have admin role
func AdminMiddleware(store *storage.Storage) gin.HandlerFunc {
	return func(c *gin.Context) {
		authHeader := c.GetHeader("Authorization")
		if authHeader == "" {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Authorization header required"})
			c.Abort()
			return
		}

		tokenString := strings.TrimPrefix(authHeader, "Bearer ")

		token, err := jwt.Parse(tokenString, func(token *jwt.Token) (interface{}, error) {
			return jwtSecret, nil
		})

		if err != nil || !token.Valid {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid token"})
			c.Abort()
			return
		}

		claims, ok := token.Claims.(jwt.MapClaims)
		if !ok {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid claims"})
			c.Abort()
			return
		}

		userID, ok := claims["user_id"].(string)
		if !ok {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid user ID"})
			c.Abort()
			return
		}

		// Get user from storage and check role
		user, err := store.GetUserByID(userID)
		if err != nil {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "User not found"})
			c.Abort()
			return
		}

		if user.Role != "admin" {
			c.JSON(http.StatusForbidden, gin.H{"error": "Admin access required"})
			c.Abort()
			return
		}

		// Check if user is suspended
		if user.Status == "suspended" {
			c.JSON(http.StatusForbidden, gin.H{"error": "Account suspended"})
			c.Abort()
			return
		}

		c.Set("user_id", userID)
		c.Set("user_role", user.Role)
		c.Next()
	}
}

// GetUserFromToken extracts user info from JWT token and checks status
func CheckUserStatus(store *storage.Storage, userID string) error {
	user, err := store.GetUserByID(userID)
	if err != nil {
		return err
	}

	if user.Status == "suspended" {
		return jwt.ErrTokenInvalidClaims
	}

	return nil
}
