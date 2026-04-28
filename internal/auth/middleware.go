package auth

import (
	"fmt"
	"net/http"
	"strings"

	"datum-go/internal/logger"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
)

// UserAuthMiddleware handles authentication for Users via JWT or Persistent API Key (ak_)
func UserAuthMiddleware(store storage.Provider) gin.HandlerFunc {
	return func(c *gin.Context) {
		authHeader := c.GetHeader("Authorization")
		var token string

		// 1. Extract token/key
		if authHeader != "" {
			parts := strings.Split(authHeader, " ")
			if len(parts) == 2 && parts[0] == "Bearer" {
				token = parts[1]
			}
		}

		// Fallback to query param (common for streaming/OTA)
		if token == "" {
			token = c.Query("token")
		}

		if token == "" {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Authorization required"})
			c.Abort()
			return
		}

		// 2. Identify Token Type
		if strings.HasPrefix(token, "ak_") {
			// === Persistent User API Key ===
			user, err := store.GetUserByUserAPIKey(token)
			if err != nil {
				c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid API Key"})
				c.Abort()
				return
			}

			// Valid Key -> Set Context
			c.Set("user_id", user.ID)
			c.Set("email", user.Email)
			c.Set("role", user.Role)
			c.Set("auth_method", "api_key")
			c.Next()

		} else {
			// === JWT Token ===
			claims, err := ValidateToken(token)
			if err != nil {
				c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid token"})
				c.Abort()
				return
			}

			// Valid JWT -> Set Context
			c.Set("user_id", claims.UserID)
			c.Set("email", claims.Email)
			c.Set("role", claims.Role)
			c.Set("session_jti", claims.ID) // session / JTI for logout & session mgmt
			c.Set("auth_method", "jwt")
			c.Next()
		}
	}
}

// HybridAuthMiddleware allows EITHER User Auth OR Device Auth
func HybridAuthMiddleware(store storage.Provider) gin.HandlerFunc {
	return func(c *gin.Context) {
		authHeader := c.GetHeader("Authorization")
		token := ""
		if authHeader != "" {
			parts := strings.Split(authHeader, " ")
			if len(parts) == 2 && parts[0] == "Bearer" {
				token = parts[1]
			}
		}

		// Debug Log
		logger.GetLogger().Info().Str("token_prefix", func() string {
			if len(token) > 4 {
				return token[:4]
			}
			return "short"
		}()).Msg("HybridAuthMiddleware Check")

		// Device Auth Detection (sk_ or dk_)
		if strings.HasPrefix(token, "sk_") || strings.HasPrefix(token, "dk_") {
			logger.GetLogger().Info().Msg("Routing to DeviceAuthMiddleware")
			DeviceAuthMiddleware()(c)
			return
		}

		// User Auth Fallback (JWT or ak_)
		UserAuthMiddleware(store)(c)
	}
}

// DeviceAuthMiddleware validates device API keys and tokens
// Supports both legacy API keys (sk_xxx) and new token format (dk_{expiry}.{signature})
func DeviceAuthMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		authHeader := c.GetHeader("Authorization")
		if authHeader == "" {
			// Fallback: check query parameter (for testing or OTA)
			apiKey := c.Query("key")
			if apiKey == "" {
				apiKey = c.Query("token")
			}
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

// GetUserRole extracts user role from context
func GetUserRole(c *gin.Context) (string, error) {
	role, exists := c.Get("role")
	if !exists {
		return "", fmt.Errorf("role not found in context")
	}
	return role.(string), nil
}

// GetSessionJTI extracts the session JTI (JWT ID) from context.
// Returns empty string if auth was via API key (no session JTI).
func GetSessionJTI(c *gin.Context) string {
	if jti, exists := c.Get("session_jti"); exists {
		if s, ok := jti.(string); ok {
			return s
		}
	}
	return ""
}
