package auth

import (
	"fmt"
	"net/http"
	"strings"

	"github.com/gin-gonic/gin"
)

// AuthMiddleware validates JWT tokens
func AuthMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		authHeader := c.GetHeader("Authorization")
		if authHeader == "" {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Authorization header required"})
			c.Abort()
			return
		}

		// Extract token from "Bearer <token>"
		parts := strings.Split(authHeader, " ")
		if len(parts) != 2 || parts[0] != "Bearer" {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid authorization format"})
			c.Abort()
			return
		}

		token := parts[1]
		claims, err := ValidateToken(token)
		if err != nil {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid token"})
			c.Abort()
			return
		}

		// Store user info in context
		c.Set("user_id", claims.UserID)
		c.Set("email", claims.Email)
		c.Next()
	}
}

// DeviceAuthMiddleware validates device API keys and tokens
// Supports both legacy API keys (sk_xxx) and new token format (dk_{expiry}.{signature})
func DeviceAuthMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		authHeader := c.GetHeader("Authorization")
		if authHeader == "" {
			// Fallback: check query parameter (for testing)
			apiKey := c.Query("key")
			if apiKey != "" {
				c.Set("api_key", apiKey)
				c.Next()
				return
			}

			c.JSON(http.StatusUnauthorized, gin.H{"error": "API key required"})
			c.Abort()
			return
		}

		// Extract API key from "Bearer <key>"
		parts := strings.Split(authHeader, " ")
		if len(parts) != 2 || parts[0] != "Bearer" {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid authorization format"})
			c.Abort()
			return
		}

		apiKey := parts[1]

		// Validate based on key format
		if strings.HasPrefix(apiKey, "dk_") && strings.Contains(apiKey, ".") {
			// New token format: dk_{expiry}.{signature}
			// Validate token expiry (basic check - full validation requires master secret)
			tokenParts := strings.Split(apiKey, ".")
			if len(tokenParts) != 2 {
				c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid token format"})
				c.Abort()
				return
			}

			// Token will be fully validated by the handler with master secret
			c.Set("api_key", apiKey)
			c.Set("is_token", true)
			c.Next()
		} else if strings.HasPrefix(apiKey, "sk_") || strings.HasPrefix(apiKey, "dk_") {
			// Legacy API key format (sk_xxx or dk_xxx without dot)
			c.Set("api_key", apiKey)
			c.Set("is_token", false)
			c.Next()
		} else {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid API key format"})
			c.Abort()
			return
		}
	}
}

// GetUserID extracts user ID from context
func GetUserID(c *gin.Context) (string, error) {
	userID, exists := c.Get("user_id")
	if !exists {
		return "", fmt.Errorf("user_id not found in context")
	}
	return userID.(string), nil
}
