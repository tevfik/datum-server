package auth

import (
	"fmt"
	"net/http"
	"os"
	"strings"
	"time"

	"datum-go/internal/logger"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/golang-jwt/jwt/v5"
)

type SupabaseProvider struct {
	Store     storage.Provider
	JwtSecret []byte
}

func NewSupabaseProvider(s storage.Provider, secret string) *SupabaseProvider {
	if secret == "" {
		secret = os.Getenv("SUPABASE_JWT_SECRET")
	}
	return &SupabaseProvider{
		Store:     s,
		JwtSecret: []byte(secret),
	}
}

// LoginHandler - Supabase handles login on client side
func (p *SupabaseProvider) LoginHandler(c *gin.Context) {
	c.JSON(http.StatusMethodNotAllowed, gin.H{
		"error": "Login should be performed via Supabase Client SDK directly.",
	})
}

// RegisterHandler - Supabase handles registration on client side
func (p *SupabaseProvider) RegisterHandler(c *gin.Context) {
	c.JSON(http.StatusMethodNotAllowed, gin.H{
		"error": "Registration should be performed via Supabase Client SDK directly.",
	})
}

// ForgotPasswordHandler - Supabase handles this
func (p *SupabaseProvider) ForgotPasswordHandler(c *gin.Context) {
	c.JSON(http.StatusMethodNotAllowed, gin.H{
		"error": "Password reset should be performed via Supabase Client SDK directly.",
	})
}

// ResetPasswordHandler - Supabase handles this
func (p *SupabaseProvider) ResetPasswordHandler(c *gin.Context) {
	c.JSON(http.StatusMethodNotAllowed, gin.H{
		"error": "Password reset should be performed via Supabase Client SDK directly.",
	})
}

// AuthMiddleware verifies the Supabase JWT and performs JIT provisioning
func (p *SupabaseProvider) AuthMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		authHeader := c.GetHeader("Authorization")
		if authHeader == "" {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Authorization header required"})
			c.Abort()
			return
		}

		parts := strings.Split(authHeader, " ")
		if len(parts) != 2 || parts[0] != "Bearer" {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid authorization header format"})
			c.Abort()
			return
		}

		tokenString := parts[1]

		// Parse and validate JWT using Supabase Secret (HS256)
		token, err := jwt.Parse(tokenString, func(token *jwt.Token) (interface{}, error) {
			if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
				return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
			}
			return p.JwtSecret, nil
		})

		if err != nil || !token.Valid {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid Supabase token"})
			c.Abort()
			return
		}

		claims, ok := token.Claims.(jwt.MapClaims)
		if !ok {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid token claims"})
			c.Abort()
			return
		}

		// Extract Supabase specific claims
		// 'sub' is the UUID of the user in Supabase
		userID, _ := claims["sub"].(string)
		email, _ := claims["email"].(string)

		if userID == "" || email == "" {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Token missing sub or email"})
			c.Abort()
			return
		}

		// JIT Provisioning: Ensure user exists in local store
		// We use the SAME ID as Supabase for consistency
		_, err = p.Store.GetUserByID(userID)
		if err != nil {
			// User not found, create them (JIT)
			logger.GetLogger().Info().Str("user_id", userID).Msg("JIT Provisioning new Supabase user")

			newUser := &storage.User{
				ID:           userID,
				Email:        email,
				Role:         "user", // Default role
				Status:       "active",
				CreatedAt:    time.Now(),
				PasswordHash: "external_auth", // Placeholder
			}

			if err := p.Store.CreateUser(newUser); err != nil {
				logger.GetLogger().Error().Err(err).Msg("Failed to JIT provision user")
				c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to provision user"})
				c.Abort()
				return
			}
		}

		// Set context vars
		c.Set("user_id", userID)
		c.Set("email", email)
		c.Set("role", "user") // Or extract from claims if using custom claims
		c.Next()
	}
}
