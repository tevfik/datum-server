package main

import (
	"net/http"
	"strings"

	"datum-go/internal/auth"
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
			claims, err := auth.ValidateToken(token)
			if err != nil {
				c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid token"})
				c.Abort()
				return
			}

			// Valid JWT -> Set Context
			c.Set("user_id", claims.UserID)
			c.Set("email", claims.Email)
			c.Set("role", claims.Role)
			c.Set("auth_method", "jwt")
			c.Next()
		}
	}
}
