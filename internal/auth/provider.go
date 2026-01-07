package auth

import "github.com/gin-gonic/gin"

// AuthProvider interface defines the methods required for an authentication provider
type AuthProvider interface {
	// RegisterHandler handles user registration requests
	RegisterHandler(c *gin.Context)

	// LoginHandler handles user login requests
	LoginHandler(c *gin.Context)

	// ForgotPasswordHandler handles password reset requests
	ForgotPasswordHandler(c *gin.Context)

	// ResetPasswordHandler handles the actual password reset process
	ResetPasswordHandler(c *gin.Context)

	// AuthMiddleware returns a Gin middleware that verifies authentication
	AuthMiddleware() gin.HandlerFunc
}
