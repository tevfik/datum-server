package auth

import (
	"context"
	"crypto/sha256"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"strings"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/storage"

	"github.com/gin-gonic/gin"
	"golang.org/x/oauth2"
	"golang.org/x/oauth2/github"
	"golang.org/x/oauth2/google"
)

// oauthProviders holds the configured OAuth2 providers.
// Only providers with CLIENT_ID set are enabled.
var oauthProviders = map[string]*oauth2.Config{}

func init() {
	if id := os.Getenv("OAUTH_GOOGLE_CLIENT_ID"); id != "" {
		oauthProviders["google"] = &oauth2.Config{
			ClientID:     id,
			ClientSecret: os.Getenv("OAUTH_GOOGLE_CLIENT_SECRET"),
			Scopes:       []string{"openid", "profile", "email"},
			Endpoint:     google.Endpoint,
		}
	}
	if id := os.Getenv("OAUTH_GITHUB_CLIENT_ID"); id != "" {
		oauthProviders["github"] = &oauth2.Config{
			ClientID:     id,
			ClientSecret: os.Getenv("OAUTH_GITHUB_CLIENT_SECRET"),
			Scopes:       []string{"read:user", "user:email"},
			Endpoint:     github.Endpoint,
		}
	}
}

// oauthCallbackURL builds the callback URL for the given provider.
func (h *Handler) oauthCallbackURL(provider string) string {
	return fmt.Sprintf("%s/auth/oauth/%s/callback", h.PublicURL, provider)
}

// GetOAuthProviders returns the list of configured OAuth providers.
// GET /auth/providers
func (h *Handler) GetOAuthProviders(c *gin.Context) {
	providers := make([]string, 0, len(oauthProviders))
	for name := range oauthProviders {
		providers = append(providers, name)
	}
	c.JSON(http.StatusOK, gin.H{"providers": providers})
}

// OAuthRedirect initiates the OAuth2 flow.
// GET /auth/oauth/:provider?redirect_uri=<app_redirect>&code_challenge=<pkce>
//
// Mobile apps (PKCE): pass code_challenge (S256-hashed verifier) and redirect_uri.
// Web apps: omit code_challenge; redirect_uri defaults to /auth/oauth/:provider/callback.
//
// Supported providers: google, github (configured via env vars).
func (h *Handler) OAuthRedirect(c *gin.Context) {
	provider := c.Param("provider")
	cfg, ok := oauthProviders[provider]
	if !ok {
		c.JSON(http.StatusBadRequest, gin.H{"error": fmt.Sprintf("OAuth provider '%s' not configured", provider)})
		return
	}

	// Clone config to set per-request redirect URI
	cfgCopy := *cfg
	cfgCopy.RedirectURL = h.oauthCallbackURL(provider)

	// Build state: encodes app redirect_uri + optional PKCE code_challenge
	appRedirect := c.Query("redirect_uri")
	codeChallenge := c.Query("code_challenge")
	state := encodeOAuthState(appRedirect, codeChallenge)

	authURL := cfgCopy.AuthCodeURL(state, oauth2.AccessTypeOnline)
	c.Redirect(http.StatusFound, authURL)
}

// OAuthCallback handles the provider's callback and issues a datum token pair.
// GET /auth/oauth/:provider/callback?code=...&state=...
func (h *Handler) OAuthCallback(c *gin.Context) {
	provider := c.Param("provider")
	cfg, ok := oauthProviders[provider]
	if !ok {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Unknown provider"})
		return
	}

	cfgCopy := *cfg
	cfgCopy.RedirectURL = h.oauthCallbackURL(provider)

	code := c.Query("code")
	stateRaw := c.Query("state")
	appRedirect, codeChallenge := decodeOAuthState(stateRaw)

	// Exchange authorization code for provider token
	token, err := cfgCopy.Exchange(context.Background(), code)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Failed to exchange OAuth code"})
		return
	}

	// Fetch user info from provider
	providerEmail, providerName, err := fetchProviderUser(provider, token.AccessToken)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to fetch user info from provider"})
		return
	}

	// Find or create user
	user, err := h.Store.GetUserByEmail(providerEmail)
	if err != nil {
		// Auto-register: check if registration is open
		config, _ := h.Store.GetSystemConfig()
		if !config.AllowRegister && !h.Store.IsSystemInitialized() {
			c.JSON(http.StatusForbidden, gin.H{"error": "Registration is disabled"})
			return
		}

		// Create new user (no password — OAuth-only account)
		user = &storage.User{
			ID:          generateID("usr"),
			Email:       providerEmail,
			DisplayName: providerName,
			Role:        "user",
			Status:      "active",
			CreatedAt:   time.Now(),
		}
		// Hash a random string as placeholder password (user can never log in with it)
		randomBytes := make([]byte, 16)
		if _, rerr := io.ReadFull(strings.NewReader(token.AccessToken+providerEmail), randomBytes); rerr == nil {
			if ph, herr := auth.HashPassword(base64.StdEncoding.EncodeToString(randomBytes)); herr == nil {
				user.PasswordHash = ph
			}
		}
		if user.PasswordHash == "" {
			// Fallback
			user.PasswordHash, _ = auth.HashPassword(token.AccessToken[:min(32, len(token.AccessToken))])
		}
		if err := h.Store.CreateUser(user); err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to create user"})
			return
		}
	}

	h.Store.UpdateUserLastLogin(user.ID)

	resp, err := issueTokenPair(h.Store, user, c.GetHeader("User-Agent"), c.ClientIP())
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate token"})
		return
	}

	// Mobile PKCE flow: redirect back to the app deep-link with tokens
	if appRedirect != "" && codeChallenge != "" {
		sep := "?"
		if strings.Contains(appRedirect, "?") {
			sep = "&"
		}
		redirect := fmt.Sprintf("%s%saccess_token=%s&refresh_token=%s&token_type=Bearer",
			appRedirect, sep, resp.Token, resp.RefreshToken)
		c.Redirect(http.StatusFound, redirect)
		return
	}

	// Web flow: redirect to the SPA callback page with tokens in query params
	spaCallback := fmt.Sprintf("%s/auth/oauth/callback?token=%s&refresh_token=%s&user_id=%s&email=%s&role=%s",
		h.PublicURL, resp.Token, resp.RefreshToken, resp.UserID, user.Email, user.Role)
	c.Redirect(http.StatusFound, spaCallback)
}

// fetchProviderUser retrieves email and display name from the OAuth provider.
func fetchProviderUser(provider, accessToken string) (email, name string, err error) {
	switch provider {
	case "google":
		return fetchGoogleUser(accessToken)
	case "github":
		return fetchGitHubUser(accessToken)
	default:
		return "", "", fmt.Errorf("unsupported provider: %s", provider)
	}
}

func fetchGoogleUser(accessToken string) (email, name string, err error) {
	req, _ := http.NewRequest("GET", "https://www.googleapis.com/oauth2/v2/userinfo", nil)
	req.Header.Set("Authorization", "Bearer "+accessToken)
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return "", "", err
	}
	defer resp.Body.Close()
	var info struct {
		Email string `json:"email"`
		Name  string `json:"name"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&info); err != nil {
		return "", "", err
	}
	return strings.ToLower(info.Email), info.Name, nil
}

func fetchGitHubUser(accessToken string) (email, name string, err error) {
	// Primary email from /user
	req, _ := http.NewRequest("GET", "https://api.github.com/user", nil)
	req.Header.Set("Authorization", "Bearer "+accessToken)
	req.Header.Set("Accept", "application/vnd.github.v3+json")
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return "", "", err
	}
	defer resp.Body.Close()
	var userInfo struct {
		Name  string `json:"name"`
		Email string `json:"email"`
		Login string `json:"login"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&userInfo); err != nil {
		return "", "", err
	}
	name = userInfo.Name
	if name == "" {
		name = userInfo.Login
	}
	email = strings.ToLower(userInfo.Email)

	// If email is private, fetch from /user/emails
	if email == "" {
		email, _ = fetchGitHubPrimaryEmail(accessToken)
	}
	if email == "" {
		return "", "", fmt.Errorf("GitHub: could not retrieve email (ensure user:email scope)")
	}
	return email, name, nil
}

func fetchGitHubPrimaryEmail(accessToken string) (string, error) {
	req, _ := http.NewRequest("GET", "https://api.github.com/user/emails", nil)
	req.Header.Set("Authorization", "Bearer "+accessToken)
	req.Header.Set("Accept", "application/vnd.github.v3+json")
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	var emails []struct {
		Email   string `json:"email"`
		Primary bool   `json:"primary"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&emails); err != nil {
		return "", err
	}
	for _, e := range emails {
		if e.Primary {
			return strings.ToLower(e.Email), nil
		}
	}
	return "", fmt.Errorf("no primary email")
}

// ============ PKCE / State helpers ============

// encodeOAuthState packs appRedirect and codeChallenge into a compact state string.
func encodeOAuthState(appRedirect, codeChallenge string) string {
	raw := appRedirect + "|" + codeChallenge
	h := sha256.Sum256([]byte(raw))
	// State = base64(appRedirect|codeChallenge) + "." + checksum[:8]
	return base64.URLEncoding.EncodeToString([]byte(raw)) + "." + base64.URLEncoding.EncodeToString(h[:4])
}

// decodeOAuthState extracts appRedirect and codeChallenge from state.
func decodeOAuthState(state string) (appRedirect, codeChallenge string) {
	parts := strings.SplitN(state, ".", 2)
	if len(parts) == 0 {
		return "", ""
	}
	decoded, err := base64.URLEncoding.DecodeString(parts[0])
	if err != nil {
		return "", ""
	}
	halves := strings.SplitN(string(decoded), "|", 2)
	if len(halves) == 2 {
		return halves[0], halves[1]
	}
	return halves[0], ""
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}
