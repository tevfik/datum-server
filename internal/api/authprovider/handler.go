package authprovider

import (
	"crypto/rand"
	"encoding/hex"
	"net/http"
	"net/url"
	"os"
	"strings"
	"sync"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/storage"
	"github.com/gin-gonic/gin"
	"github.com/golang-jwt/jwt/v5"
)

// oauthCode represents a temporary authorization code
type oauthCode struct {
	UserID      string
	ClientID    string
	RedirectURI string
	ExpiresAt   time.Time
	State       string
}

// AuthProviderService handles the auth provider API endpoints.
type AuthProviderService struct {
	jwtSecret  []byte
	store      storage.Provider
	oauthCodes sync.Map // map[string]oauthCode
}

// New creates a new AuthProvider service.
func New(jwtSecret []byte, store storage.Provider) *AuthProviderService {
	return &AuthProviderService{jwtSecret: jwtSecret, store: store}
}

// RegisterRoutes mounts all auth provider routes onto the router.
// IMPORTANT: Must be called BEFORE any NoRoute handler is set!
func (svc *AuthProviderService) RegisterRoutes(r *gin.RouterGroup) {
	// Register ALL routes directly on the passed RouterGroup
	// This ensures they are at the correct path level

	r.POST("/token", svc.handleTokenIssuance)
	r.GET("/validate", svc.handleTokenValidation)
	r.POST("/validate", svc.handleTokenValidation)

	// Traefik ForwardAuth endpoint
	r.GET("/check", svc.handleCheck)

	// User login pages for SSO
	r.GET("/login", svc.renderLogin)
	r.POST("/login", svc.handleUserLogin)

	// OAuth2 / OIDC Authorization Code Flow endpoints
	r.GET("/oauth/authorize", svc.handleOAuthAuthorize)
	r.POST("/oauth/token", svc.handleOAuthToken)
	r.GET("/oauth/userinfo", svc.handleOAuthUserinfo)
	r.POST("/oauth/userinfo", svc.handleOAuthUserinfo)

	r.GET("/health", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{
			"status":      "healthy",
			"service":     "iot-datum-auth-provider",
			"jwt_valid":   true,
			"timestamp":   time.Now().Unix(),
			"description": "Federated authentication service",
		})
	})

	// Admin only clients endpoint
	clientsGroup := r.Group("/clients")
	clientsGroup.Use(auth.UserAuthMiddleware(svc.store))
	clientsGroup.Use(auth.AdminMiddleware(svc.store))
	clientsGroup.GET("", svc.listClients)
	clientsGroup.POST("", svc.createClient)
	clientsGroup.DELETE("/:name", svc.deleteClient)
}

// HandleTokenIssuance handles OAuth2 Client Credentials flow.
func (svc *AuthProviderService) handleTokenIssuance(c *gin.Context) {
	clientID := c.PostForm("client_id")
	clientSecret := c.PostForm("client_secret")

	if clientID == "" || clientSecret == "" {
		c.JSON(http.StatusBadRequest, gin.H{
			"error": "client_id and client_secret required",
		})
		return
	}

	// Validate against DB
	scopesStr, err := svc.store.ValidateAuthClient(clientID, clientSecret)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{
			"error": "invalid client_id or client_secret",
		})
		return
	}

	var scopes []string
	if scopesStr != "" {
		// Postgres array representation: {read,write}
		scopesStr = strings.Trim(scopesStr, "{}")
		scopes = strings.Split(scopesStr, ",")
	} else {
		scopes = []string{"read"}
	}

	// Issue JWT access token
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"user_id": clientID + "@service.example.com",
		"email":   clientID + "@auth-service.example.com",
		"role":    "service",
		"iss":     "iot-datum-bezg-in",
		"aud":     clientID,
		"iat":     time.Now().Unix(),
		"exp":     time.Now().Add(24 * time.Hour).Unix(),
	})

	tokenString, err := token.SignedString(svc.jwtSecret)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error": "Failed to generate token",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"access_token": tokenString,
		"token_type":   "Bearer",
		"expires_in":   86400,
		"scope":        strings.Join(scopes, ","),
		"client_id":    clientID,
	})
}

// HandleTokenValidation validates JWT tokens from other services.
func (svc *AuthProviderService) handleTokenValidation(c *gin.Context) {
	authHeader := c.GetHeader("Authorization")
	if authHeader == "" {
		c.JSON(http.StatusUnauthorized, gin.H{
			"valid": false,
			"error": "No authorization header",
		})
		return
	}

	tokenString := authHeader[7:] // Strip "Bearer "
	if tokenString == "" {
		c.JSON(http.StatusUnauthorized, gin.H{
			"valid": false,
			"error": "Empty token",
		})
		return
	}

	// Validate JWT token
	token, err := jwt.Parse(tokenString, func(token *jwt.Token) (interface{}, error) {
		return svc.jwtSecret, nil
	})

	if err != nil || !token.Valid {
		c.JSON(http.StatusUnauthorized, gin.H{
			"valid": false,
			"error": "Invalid token",
		})
		return
	}

	claims, ok := token.Claims.(jwt.MapClaims)
	if !ok {
		c.JSON(http.StatusUnauthorized, gin.H{
			"valid": false,
			"error": "Invalid claims",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"valid":     true,
		"user_id":   claims["user_id"].(string),
		"email":     claims["email"].(string),
		"role":      claims["role"].(string),
		"provider":  "iot-datum",
		"issued_at": time.Unix(int64(claims["iat"].(float64)), 0).Format(time.RFC3339),
	})
}

func (svc *AuthProviderService) getBaseURL(c *gin.Context) string {
	baseURL := os.Getenv("APP_URL")
	if baseURL == "" {
		baseURL = os.Getenv("SERVER_URL")
	}
	if baseURL == "" && c != nil {
		if host := c.GetHeader("X-Forwarded-Host"); host != "" {
			scheme := c.GetHeader("X-Forwarded-Proto")
			if scheme == "" {
				scheme = "https"
			}
			baseURL = scheme + "://" + host
		}
	}
	if baseURL == "" {
		baseURL = "https://datum.example.com" // Default fallback
	}
	return strings.TrimSuffix(baseURL, "/")
}

// handleCheck validates Traefik forwardAuth requests.
func (svc *AuthProviderService) handleCheck(c *gin.Context) {
	var userID string
	var email string
	var role string

	// Helper to redirect to login
	redirectToLogin := func() {
		// Use X-Forwarded-Proto, X-Forwarded-Host, X-Forwarded-Uri to build original URL
		scheme := c.GetHeader("X-Forwarded-Proto")
		if scheme == "" {
			scheme = "https"
		}
		host := c.GetHeader("X-Forwarded-Host")
		uri := c.GetHeader("X-Forwarded-Uri")
		redirectURL := ""
		if host != "" {
			redirectURL = "?redirect=" + url.QueryEscape(scheme+"://"+host+uri)
		}
		c.Redirect(http.StatusFound, svc.getBaseURL(c)+"/auth/provider/login"+redirectURL)
		c.Abort()
	}

	// --- 1. Try X-API-Key header (for Traefik-injected API keys) ---
	apiKey := c.GetHeader("X-API-Key")
	if apiKey == "" {
		authHeader := c.GetHeader("Authorization")
		if authHeader != "" && strings.HasPrefix(authHeader, "Bearer ") {
			bearerVal := authHeader[7:]
			// Heuristic: device/user API keys start with sk_ or ak_
			if strings.HasPrefix(bearerVal, "sk_") || strings.HasPrefix(bearerVal, "ak_") {
				apiKey = bearerVal
			}
		} else if authHeader != "" && strings.HasPrefix(authHeader, "ApiKey ") {
			apiKey = authHeader[7:]
		}
	}

	if apiKey != "" {
		// Try device API key first (sk_...)
		device, err := svc.store.GetDeviceByAPIKey(apiKey)
		if err == nil && device != nil && device.APIKey == apiKey {
			owner, ownerErr := svc.store.GetUserByID(device.UserID)
			userID = device.UserID
			email = "service." + device.Name
			role = "device"
			if ownerErr == nil && owner != nil {
				email = owner.Email
				role = owner.Role
			}
		} else {
			// Fallback: try user API key (ak_...)
			uAPI, err := svc.store.GetUserByUserAPIKey(apiKey)
			if err == nil && uAPI != nil {
				userID = uAPI.ID
				email = uAPI.Email
				role = uAPI.Role
			}
		}
	}

	// --- 2. Try JWT Bearer token ---
	if userID == "" {
		var tokenString string
		authHeader := c.GetHeader("Authorization")
		if authHeader != "" && strings.HasPrefix(authHeader, "Bearer ") {
			tokenString = authHeader[7:]
			if strings.HasPrefix(tokenString, "sk_") || strings.HasPrefix(tokenString, "ak_") {
				tokenString = "" // Already handled by API key check
			}
		}
		if tokenString == "" {
			cookie, err := c.Cookie("datum_session")
			if err == nil {
				tokenString = cookie
			}
		}

		if tokenString != "" {
			token, err := jwt.Parse(tokenString, func(token *jwt.Token) (interface{}, error) {
				return svc.jwtSecret, nil
			})
			if err == nil && token.Valid {
				claims, ok := token.Claims.(jwt.MapClaims)
				if ok {
					userID, _ = claims["user_id"].(string)
					email, _ = claims["email"].(string)
					role, _ = claims["role"].(string)
				}
			}
		}
	}

	// --- 3. No valid auth found → redirect to login ---
	if userID == "" {
		redirectToLogin()
		return
	}

	// --- 4. Check per-service authorization (X-Forwarded-Host) ---
	forwardedHost := c.GetHeader("X-Forwarded-Host")
	if forwardedHost != "" {
		allowedRoles, allowedEmails, err := svc.store.GetAuthClientRolesAndEmails(forwardedHost)
		if err == nil {
			allowed := false

			// Check roles array
			if allowedRoles != "" {
				roles := strings.Split(strings.Trim(allowedRoles, "{}"), ",")
				for _, r := range roles {
					if strings.TrimSpace(r) == role || strings.TrimSpace(r) == "*" {
						allowed = true
						break
					}
				}
			}

			// Check emails array
			if !allowed && allowedEmails != "" {
				emails := strings.Split(strings.Trim(allowedEmails, "{}"), ",")
				for _, e := range emails {
					if strings.TrimSpace(e) == email {
						allowed = true
						break
					}
				}
			}

			// If not allowed, reject!
			if !allowed {
				c.Redirect(http.StatusFound, svc.getBaseURL(c)+"/auth/provider/login?error=Bu+uygulamaya+erisim+yetkiniz+yok")
				c.Abort()
				return
			}
		}
	}

	// --- 5. Authenticated — set forwarded headers ---
	c.Header("X-Datum-User", email)
	c.Header("X-Datum-User-ID", userID)
	c.Header("X-Datum-Role", role)
	c.Status(http.StatusOK)
}

// renderLogin redirects to the SSO login page in the frontend
func (svc *AuthProviderService) renderLogin(c *gin.Context) {
	redirect := c.Query("redirect")
	errorMsg := c.Query("error")

	loginURL := svc.getBaseURL(c) + "/sso-login?redirect=" + url.QueryEscape(redirect)
	if errorMsg != "" {
		loginURL += "&error=" + url.QueryEscape(errorMsg)
	}

	c.Redirect(http.StatusFound, loginURL)
}

// handleUserLogin handles form submission for SSO
func (svc *AuthProviderService) handleUserLogin(c *gin.Context) {
	email := strings.ToLower(strings.TrimSpace(c.PostForm("email")))
	password := c.PostForm("password")
	redirect := c.PostForm("redirect")

	if redirect == "" {
		redirect = "/"
	}

	if email == "" || password == "" {
		c.Redirect(http.StatusFound, "/auth/provider/login?error=Email+and+password+required&redirect="+url.QueryEscape(redirect))
		return
	}

	user, err := svc.store.GetUserByEmail(email)
	if err != nil || user.Status == "suspended" || !auth.CheckPassword(user.PasswordHash, password) {
		c.Redirect(http.StatusFound, "/auth/provider/login?error=Invalid+credentials&redirect="+url.QueryEscape(redirect))
		return
	}

	// Create JWT token for datum_session
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"user_id": user.ID,
		"email":   user.Email,
		"role":    user.Role,
		"iss":     "iot-datum",
		"iat":     time.Now().Unix(),
		"exp":     time.Now().Add(24 * 7 * time.Hour).Unix(),
	})

	tokenString, err := token.SignedString(svc.jwtSecret)
	if err != nil {
		c.Redirect(http.StatusFound, "/auth/provider/login?error=Server+error&redirect="+url.QueryEscape(redirect))
		return
	}

	// Extract root domain for cookie
	host := c.Request.Host
	if strings.Contains(host, ":") {
		parts := strings.Split(host, ":")
		host = parts[0]
	}
	hostParts := strings.Split(host, ".")
	cookieDomain := ""
	if len(hostParts) >= 2 {
		cookieDomain = "." + hostParts[len(hostParts)-2] + "." + hostParts[len(hostParts)-1]
	}

	// Set cookie with dynamic domain
	c.SetCookie("datum_session", tokenString, 86400*7, "/", cookieDomain, false, true)

	c.Redirect(http.StatusFound, redirect)
}

// GenerateRandomID generates a random hex string for client ID.
func GenerateRandomID(length int) (string, error) {
	bytes := make([]byte, length/2)
	if _, err := rand.Read(bytes); err != nil {
		return "", err
	}
	return "client_" + hex.EncodeToString(bytes), nil
}

// listClients returns all registered auth clients.
func (svc *AuthProviderService) listClients(c *gin.Context) {
	clients, err := svc.store.ListAuthClients()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to query clients"})
		return
	}

	// ListAuthClients might return nil if no clients exist, make sure we return an empty array instead of null
	if clients == nil {
		clients = []map[string]interface{}{}
	}

	c.JSON(http.StatusOK, clients)
}

type createClientRequest struct {
	Name          string   `json:"name" binding:"required"`
	AllowedRoles  []string `json:"allowed_roles"`
	AllowedEmails []string `json:"allowed_emails"`
}

func (svc *AuthProviderService) createClient(c *gin.Context) {
	var req createClientRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	userID := c.GetString("user_id")
	user, err := svc.store.GetUserByID(userID)
	var creatorEmail string
	if err == nil && user != nil {
		creatorEmail = user.Email
	} else {
		creatorEmail = "admin"
	}

	clientID, _ := GenerateRandomID(16)
	clientSecret, _ := GenerateRandomID(32)

	err = svc.store.CreateAuthClient(req.Name, clientID, clientSecret, creatorEmail, req.AllowedRoles, req.AllowedEmails)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to create client: " + err.Error()})
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"message":       "client created successfully",
		"name":          req.Name,
		"client_id":     clientID,
		"client_secret": clientSecret, // Only returned once
	})
}

func (svc *AuthProviderService) deleteClient(c *gin.Context) {
	name := c.Param("name")
	if name == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "name is required"})
		return
	}

	found, err := svc.store.DeleteAuthClient(name)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to delete client"})
		return
	}

	if !found {
		c.JSON(http.StatusNotFound, gin.H{"error": "client not found"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "client deleted successfully"})
}

// parseArray parses a postgres array like "{admin,user}" into a []string slice.
func parseArray(pgArray string) []string {
	if pgArray == "" || pgArray == "{}" {
		return []string{}
	}
	trimmed := strings.Trim(pgArray, "{}")
	parts := strings.Split(trimmed, ",")
	for i := range parts {
		parts[i] = strings.TrimSpace(parts[i])
	}
	return parts
}

func (svc *AuthProviderService) handleOAuthAuthorize(c *gin.Context) {
	clientID := c.Query("client_id")
	redirectURI := c.Query("redirect_uri")
	responseType := c.Query("response_type")
	state := c.Query("state")

	if clientID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "client_id is required"})
		return
	}
	if responseType != "code" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "unsupported_response_type"})
		return
	}

	clients, err := svc.store.ListAuthClients()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to list clients"})
		return
	}
	clientExists := false
	for _, cl := range clients {
		if cl["client_id"] == clientID {
			clientExists = true
			break
		}
	}
	if !clientExists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized_client"})
		return
	}

	var tokenString string
	cookie, err := c.Cookie("datum_session")
	if err == nil {
		tokenString = cookie
	}

	requestURL := c.Request.URL.RequestURI()

	redirectToLogin := func() {
		c.Redirect(http.StatusFound, svc.getBaseURL(c)+"/auth/provider/login?redirect="+url.QueryEscape(svc.getBaseURL(c)+requestURL))
		c.Abort()
	}

	if tokenString == "" {
		redirectToLogin()
		return
	}

	token, err := jwt.Parse(tokenString, func(token *jwt.Token) (interface{}, error) {
		return svc.jwtSecret, nil
	})
	if err != nil || !token.Valid {
		redirectToLogin()
		return
	}

	claims, ok := token.Claims.(jwt.MapClaims)
	if !ok {
		redirectToLogin()
		return
	}

	userID, _ := claims["user_id"].(string)
	if userID == "" {
		redirectToLogin()
		return
	}

	codeBytes := make([]byte, 16)
	if _, err := rand.Read(codeBytes); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "server_error"})
		return
	}
	code := hex.EncodeToString(codeBytes)

	svc.oauthCodes.Store(code, oauthCode{
		UserID:      userID,
		ClientID:    clientID,
		RedirectURI: redirectURI,
		ExpiresAt:   time.Now().Add(5 * time.Minute),
		State:       state,
	})

	sep := "?"
	if strings.Contains(redirectURI, "?") {
		sep = "&"
	}
	targetURL := redirectURI + sep + "code=" + code
	if state != "" {
		targetURL += "&state=" + state
	}

	c.Redirect(http.StatusFound, targetURL)
}

func (svc *AuthProviderService) handleOAuthToken(c *gin.Context) {
	grantType := c.PostForm("grant_type")
	code := c.PostForm("code")
	redirectURI := c.PostForm("redirect_uri")

	clientID, clientSecret, ok := c.Request.BasicAuth()
	if !ok {
		clientID = c.PostForm("client_id")
		clientSecret = c.PostForm("client_secret")
	}

	if grantType != "authorization_code" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "unsupported_grant_type"})
		return
	}
	if code == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "code is required"})
		return
	}

	_, err := svc.store.ValidateAuthClient(clientID, clientSecret)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "invalid_client"})
		return
	}

	val, exists := svc.oauthCodes.Load(code)
	if !exists {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_grant", "error_description": "code not found"})
		return
	}
	codeEntry := val.(oauthCode)

	svc.oauthCodes.Delete(code)

	if time.Now().After(codeEntry.ExpiresAt) {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_grant", "error_description": "code expired"})
		return
	}

	if codeEntry.ClientID != clientID {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_grant", "error_description": "client mismatch"})
		return
	}

	if codeEntry.RedirectURI != "" && redirectURI != "" && codeEntry.RedirectURI != redirectURI {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_grant", "error_description": "redirect_uri mismatch"})
		return
	}

	user, err := svc.store.GetUserByID(codeEntry.UserID)
	if err != nil || user == nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_grant", "error_description": "user not found"})
		return
	}

	now := time.Now()
	exp := now.Add(1 * time.Hour)

	accessTokenClaims := jwt.MapClaims{
		"sub":     user.ID,
		"user_id": user.ID,
		"email":   user.Email,
		"role":    user.Role,
		"name":    userDisplayName(user),
		"iss":     svc.getBaseURL(c),
		"aud":     clientID,
		"exp":     exp.Unix(),
		"iat":     now.Unix(),
	}
	accessToken := jwt.NewWithClaims(jwt.SigningMethodHS256, accessTokenClaims)
	accessTokenString, err := accessToken.SignedString(svc.jwtSecret)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "server_error"})
		return
	}

	idTokenClaims := jwt.MapClaims{
		"sub":            user.ID,
		"iss":            svc.getBaseURL(c),
		"aud":            clientID,
		"exp":            exp.Unix(),
		"iat":            now.Unix(),
		"email":          user.Email,
		"email_verified": true,
		"name":           userDisplayName(user),
		"role":           user.Role,
	}
	idToken := jwt.NewWithClaims(jwt.SigningMethodHS256, idTokenClaims)
	idTokenString, err := idToken.SignedString(svc.jwtSecret)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "server_error"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"access_token": accessTokenString,
		"id_token":     idTokenString,
		"token_type":   "Bearer",
		"expires_in":   3600,
	})
}

func (svc *AuthProviderService) handleOAuthUserinfo(c *gin.Context) {
	authHeader := c.GetHeader("Authorization")
	var tokenString string
	if authHeader != "" && strings.HasPrefix(authHeader, "Bearer ") {
		tokenString = authHeader[7:]
	} else {
		tokenString = c.Query("access_token")
	}

	if tokenString == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	token, err := jwt.Parse(tokenString, func(token *jwt.Token) (interface{}, error) {
		return svc.jwtSecret, nil
	})
	if err != nil || !token.Valid {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "invalid_token"})
		return
	}

	claims, ok := token.Claims.(jwt.MapClaims)
	if !ok {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "invalid_claims"})
		return
	}

	userID, _ := claims["user_id"].(string)
	if userID == "" {
		userID, _ = claims["sub"].(string)
	}

	if userID == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "invalid_user"})
		return
	}

	user, err := svc.store.GetUserByID(userID)
	if err != nil || user == nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"sub":            user.ID,
		"email":          user.Email,
		"email_verified": true,
		"name":           userDisplayName(user),
		"role":           user.Role,
	})
}

// userDisplayName returns a human-readable name for the user. The storage
// model has no display name field, so the local part of the email is used.
func userDisplayName(u *storage.User) string {
	if i := strings.Index(u.Email, "@"); i > 0 {
		return u.Email[:i]
	}
	return u.Email
}
