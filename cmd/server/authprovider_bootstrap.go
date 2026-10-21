package main

import (
	"crypto/rand"
	"datum-go/internal/api/authprovider"
	"datum-go/internal/storage"
	"os"

	"github.com/gin-gonic/gin"
	"github.com/rs/zerolog/log"
)

// initializeAuthProvider creates the SSO / OAuth2 auth provider service and
// mounts it under /auth/provider. Must be called before any NoRoute handler is set.
func initializeAuthProvider(r *gin.Engine, store storage.Provider) {
	secret := []byte(os.Getenv("JWT_SECRET"))
	if len(secret) == 0 {
		secret = make([]byte, 32)
		if _, err := rand.Read(secret); err != nil {
			log.Fatal().Err(err).Msg("auth provider: cannot generate signing secret")
		}
		log.Warn().Msg("auth provider: JWT_SECRET not set, sessions are invalidated on restart")
	}

	svc := authprovider.New(secret, store)
	svc.RegisterRoutes(r.Group("/auth/provider"))

	log.Info().Msg("auth provider initialized at /auth/provider (oauth/authorize, oauth/token, oauth/userinfo, login, check, clients)")
}
