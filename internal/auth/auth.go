package auth

import (
	"crypto/rand"
	"encoding/hex"
	"errors"
	"os"
	"strconv"
	"sync"
	"time"

	"github.com/golang-jwt/jwt/v5"
	"github.com/rs/zerolog/log"
	"golang.org/x/crypto/bcrypt"
)

var (
	ErrInvalidCredentials = errors.New("invalid credentials")
	ErrUserExists         = errors.New("user already exists")
	ErrInvalidToken       = errors.New("invalid token")
)

// JWT secret - loaded from environment or generated
var (
	jwtSecret   = initJWTSecret()
	jwtSecretMu sync.RWMutex
)

// initJWTSecret initializes JWT secret from env or generates a secure random one
func initJWTSecret() []byte {
	if secret := os.Getenv("JWT_SECRET"); secret != "" {
		if len(secret) < 32 {
			log.Warn().Msg("JWT_SECRET is too short (< 32 chars), using anyway but consider strengthening it")
		}
		return []byte(secret)
	}

	// Generate a secure random secret (32 bytes = 256 bits)
	bytes := make([]byte, 32)
	if _, err := rand.Read(bytes); err != nil {
		log.Fatal().Err(err).Msg("Failed to generate JWT secret")
	}
	secret := hex.EncodeToString(bytes)
	log.Warn().Msg("No JWT_SECRET set, generated random secret (will invalidate tokens on restart)")
	log.Warn().Msg("Set JWT_SECRET environment variable to persist tokens across restarts")
	return []byte(secret)
}

// SetJWTSecret sets the JWT secret explicitly (e.g. from a persistent file)
func SetJWTSecret(secret []byte) {
	if len(secret) >= 32 {
		jwtSecretMu.Lock()
		jwtSecret = secret
		jwtSecretMu.Unlock()
		log.Info().Msg("JWT secret updated from persistence")
	} else {
		log.Warn().Msg("Attempted to set weak JWT secret from persistence, ignoring")
	}
}

// getJWTSecret returns the current JWT secret (thread-safe)
func getJWTSecret() []byte {
	jwtSecretMu.RLock()
	defer jwtSecretMu.RUnlock()
	return jwtSecret
}

type Claims struct {
	UserID string `json:"user_id"`
	Email  string `json:"email"`
	Role   string `json:"role"`
	jwt.RegisteredClaims
}

// HashPassword hashes a password using bcrypt
func HashPassword(password string) (string, error) {
	bytes, err := bcrypt.GenerateFromPassword([]byte(password), bcrypt.DefaultCost)
	return string(bytes), err
}

// CheckPassword compares a hashed password with a plain text password
func CheckPassword(hashedPassword, password string) bool {
	err := bcrypt.CompareHashAndPassword([]byte(hashedPassword), []byte(password))
	return err == nil
}

// GenerateToken creates a JWT token for a user
func GenerateToken(userID, email, role string) (string, error) {
	claims := Claims{
		UserID: userID,
		Email:  email,
		Role:   role,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(7 * 24 * time.Hour)), // 7 days
			IssuedAt:  jwt.NewNumericDate(time.Now()),
		},
	}

	secret := getJWTSecret()
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	return token.SignedString(secret)
}

// GenerateTokenPair creates an access token (15 min) and a refresh token (30 days)
func GenerateTokenPair(userID, email, role string) (accessToken string, refreshToken string, err error) {
	secret := getJWTSecret()

	// 1. Access Token (Short-lived)
	accessClaims := Claims{
		UserID: userID,
		Email:  email,
		Role:   role,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(time.Duration(getAccessExpiryMinutes()) * time.Minute)), // Configurable duration
			IssuedAt:  jwt.NewNumericDate(time.Now()),
		},
	}
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, accessClaims)
	accessToken, err = token.SignedString(secret)
	if err != nil {
		return "", "", err
	}

	// 2. Refresh Token (Long-lived, Opaque or JWT)
	// We use a long-lived JWT for simplicity so it carries user info, but we will check it against DB whitelist
	refreshClaims := Claims{
		UserID: userID,
		Email:  email,
		Role:   role,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(time.Duration(getRefreshExpiryDays()) * 24 * time.Hour)),
			IssuedAt:  jwt.NewNumericDate(time.Now()),
			Subject:   "refresh",
		},
	}
	rToken := jwt.NewWithClaims(jwt.SigningMethodHS256, refreshClaims)
	refreshToken, err = rToken.SignedString(secret)
	if err != nil {
		return "", "", err
	}

	return accessToken, refreshToken, nil
}

// ValidateToken validates a JWT token and returns the claims
func ValidateToken(tokenString string) (*Claims, error) {
	token, err := jwt.ParseWithClaims(tokenString, &Claims{}, func(token *jwt.Token) (interface{}, error) {
		return getJWTSecret(), nil
	})

	if err != nil {
		return nil, err
	}

	if claims, ok := token.Claims.(*Claims); ok && token.Valid {
		return claims, nil
	}

	return nil, ErrInvalidToken
}

// GenerateAPIKey generates a random API key for devices (sk_ prefix)
// Standardized to 16 bytes (32 hex chars)
func GenerateAPIKey() (string, error) {
	bytes := make([]byte, 16) // 16 bytes = 32 hex chars
	if _, err := rand.Read(bytes); err != nil {
		return "", err
	}
	return "sk_" + hex.EncodeToString(bytes), nil
}

// GenerateUserAPIKey generates a persistent API key for users (ak_ prefix)
func GenerateUserAPIKey() (string, error) {
	bytes := make([]byte, 16) // 16 bytes = 32 hex chars
	if _, err := rand.Read(bytes); err != nil {
		return "", err
	}
	return "ak_" + hex.EncodeToString(bytes), nil
}

// getRefreshExpiryDays reads JWT_REFRESH_EXPIRY from env or returns default (30)
func getRefreshExpiryDays() int {
	val := os.Getenv("JWT_REFRESH_EXPIRY")
	if val == "" {
		return 30
	}
	days, err := strconv.Atoi(val)
	if err != nil || days <= 0 {
		log.Warn().Str("val", val).Msg("Invalid JWT_REFRESH_EXPIRY, using default 30 days")
		return 30
	}
	return days
}

// getAccessExpiryMinutes reads JWT_ACCESS_EXPIRY_MINUTES from env or returns default (15)
func getAccessExpiryMinutes() int {
	val := os.Getenv("JWT_ACCESS_EXPIRY_MINUTES")
	if val == "" {
		return 15 // Default 15 minutes
	}
	mins, err := strconv.Atoi(val)
	if err != nil || mins <= 0 {
		log.Warn().Str("val", val).Msg("Invalid JWT_ACCESS_EXPIRY_MINUTES, using default 15 minutes")
		return 15
	}
	return mins
}
