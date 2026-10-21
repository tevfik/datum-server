package authprovider

import (
	"bytes"
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"datum-go/internal/auth"
	"datum-go/internal/storage"
	"github.com/gin-gonic/gin"
	"github.com/golang-jwt/jwt/v5"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/mock"
)

// MockProvider mocks the storage.Provider interface
type MockProvider struct {
	storage.Provider
	mock.Mock
}

func (m *MockProvider) ValidateAuthClient(clientID, clientSecret string) (string, error) {
	args := m.Called(clientID, clientSecret)
	return args.String(0), args.Error(1)
}

func (m *MockProvider) GetAuthClientRolesAndEmails(appName string) (string, string, error) {
	args := m.Called(appName)
	return args.String(0), args.String(1), args.Error(2)
}

func (m *MockProvider) ListAuthClients() ([]map[string]interface{}, error) {
	args := m.Called()
	return args.Get(0).([]map[string]interface{}), args.Error(1)
}

func (m *MockProvider) CreateAuthClient(name, clientID, clientSecret, creatorEmail string, roles []string, emails []string) error {
	args := m.Called(name, clientID, clientSecret, creatorEmail, roles, emails)
	return args.Error(0)
}

func (m *MockProvider) DeleteAuthClient(name string) (bool, error) {
	args := m.Called(name)
	return args.Bool(0), args.Error(1)
}

func (m *MockProvider) GetUserByID(userID string) (*storage.User, error) {
	args := m.Called(userID)
	if args.Get(0) != nil {
		return args.Get(0).(*storage.User), args.Error(1)
	}
	return nil, args.Error(1)
}

func (m *MockProvider) GetUserByEmail(email string) (*storage.User, error) {
	args := m.Called(email)
	if args.Get(0) != nil {
		return args.Get(0).(*storage.User), args.Error(1)
	}
	return nil, args.Error(1)
}

func setupTestRouter(svc *AuthProviderService) *gin.Engine {
	gin.SetMode(gin.TestMode)
	// Make sure the auth package uses the same secret as our mock tokens
	auth.SetJWTSecret([]byte("this-is-a-very-long-secret-key-that-is-at-least-32-bytes-long"))

	r := gin.Default()
	authGroup := r.Group("/auth/provider")
	svc.RegisterRoutes(authGroup)
	return r
}

func addAuthHeader(req *http.Request) {
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"user_id": "admin_user_id",
		"email":   "admin@example.com",
		"role":    "admin",
		"exp":     time.Now().Add(time.Hour).Unix(),
	})
	tokenString, _ := token.SignedString([]byte("this-is-a-very-long-secret-key-that-is-at-least-32-bytes-long"))
	req.Header.Set("Authorization", "Bearer "+tokenString)
}

func TestHandleCheck_NoAuth(t *testing.T) {
	mockStore := new(MockProvider)
	svc := New([]byte("this-is-a-very-long-secret-key-that-is-at-least-32-bytes-long"), mockStore)
	r := setupTestRouter(svc)

	req, _ := http.NewRequest("GET", "/auth/provider/check", nil)
	req.Header.Set("X-Forwarded-Host", "test.example.com")
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	// Should redirect to login since there's no auth header or cookie
	assert.Equal(t, http.StatusFound, w.Code)
	assert.Contains(t, w.Header().Get("Location"), "login")
}

func TestListClients(t *testing.T) {
	mockStore := new(MockProvider)
	svc := New([]byte("this-is-a-very-long-secret-key-that-is-at-least-32-bytes-long"), mockStore)
	r := setupTestRouter(svc)

	expectedClients := []map[string]interface{}{
		{"name": "app1", "client_id": "client_123"},
	}
	mockStore.On("ListAuthClients").Return(expectedClients, nil)

	mockUser := &storage.User{ID: "admin_user_id", Email: "admin@example.com", Role: "admin"}
	mockStore.On("GetUserByID", "admin_user_id").Return(mockUser, nil)

	req, _ := http.NewRequest("GET", "/auth/provider/clients", nil)
	addAuthHeader(req)
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	var response []map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.Len(t, response, 1)
	assert.Equal(t, "app1", response[0]["name"])
}

func TestCreateClient(t *testing.T) {
	mockStore := new(MockProvider)
	svc := New([]byte("this-is-a-very-long-secret-key-that-is-at-least-32-bytes-long"), mockStore)
	r := setupTestRouter(svc)

	mockUser := &storage.User{ID: "admin_user_id", Email: "admin@example.com", Role: "admin"}
	mockStore.On("GetUserByID", "admin_user_id").Return(mockUser, nil)
	var nilSlice []string
	mockStore.On("CreateAuthClient", "new_app", mock.AnythingOfType("string"), mock.AnythingOfType("string"), "admin@example.com", []string{"admin"}, nilSlice).Return(nil)

	body := `{"name": "new_app", "allowed_roles": ["admin"]}`
	req, _ := http.NewRequest("POST", "/auth/provider/clients", bytes.NewBufferString(body))
	addAuthHeader(req)
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)
	var response map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.Equal(t, "new_app", response["name"])
	assert.NotEmpty(t, response["client_id"])
}

func TestDeleteClient_Success(t *testing.T) {
	mockStore := new(MockProvider)
	svc := New([]byte("this-is-a-very-long-secret-key-that-is-at-least-32-bytes-long"), mockStore)
	r := setupTestRouter(svc)

	mockUser := &storage.User{ID: "admin_user_id", Email: "admin@example.com", Role: "admin"}
	mockStore.On("GetUserByID", "admin_user_id").Return(mockUser, nil)
	mockStore.On("DeleteAuthClient", "app1").Return(true, nil)

	req, _ := http.NewRequest("DELETE", "/auth/provider/clients/app1", nil)
	addAuthHeader(req)
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

func TestDeleteClient_NotFound(t *testing.T) {
	mockStore := new(MockProvider)
	svc := New([]byte("this-is-a-very-long-secret-key-that-is-at-least-32-bytes-long"), mockStore)
	r := setupTestRouter(svc)

	mockUser := &storage.User{ID: "admin_user_id", Email: "admin@example.com", Role: "admin"}
	mockStore.On("GetUserByID", "admin_user_id").Return(mockUser, nil)
	mockStore.On("DeleteAuthClient", "app_not_found").Return(false, nil)

	req, _ := http.NewRequest("DELETE", "/auth/provider/clients/app_not_found", nil)
	addAuthHeader(req)
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func TestParseArray(t *testing.T) {
	assert.Empty(t, parseArray(""))
	assert.Empty(t, parseArray("{}"))
	assert.Equal(t, []string{"admin"}, parseArray("{admin}"))
	assert.Equal(t, []string{"admin", "user"}, parseArray("{admin,user}"))
	assert.Equal(t, []string{"admin", "user"}, parseArray("{admin, user}"))
}

func TestRenderLogin(t *testing.T) {
	t.Setenv("APP_URL", "https://datum.example.com")
	mockStore := new(MockProvider)
	svc := New([]byte("secret"), mockStore)
	r := setupTestRouter(svc)

	req, _ := http.NewRequest("GET", "/auth/provider/login?redirect=/dashboard&error=oops", nil)
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusFound, w.Code)
	assert.Equal(t, "https://datum.example.com/sso-login?redirect=/dashboard&error=oops", w.Header().Get("Location"))
}

func TestHandleUserLogin_Success(t *testing.T) {
	mockStore := new(MockProvider)
	svc := New([]byte("secret"), mockStore)
	r := setupTestRouter(svc)

	// User exists with matching password hash
	hashedPass, _ := auth.HashPassword("password123")
	mockUser := &storage.User{ID: "user1", Email: "test@example.com", Role: "user", Status: "active", PasswordHash: hashedPass}
	mockStore.On("GetUserByEmail", "test@example.com").Return(mockUser, nil)

	body := `email=test@example.com&password=password123&redirect=/home`
	req, _ := http.NewRequest("POST", "/auth/provider/login", bytes.NewBufferString(body))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusFound, w.Code)
	assert.Equal(t, "/home", w.Header().Get("Location"))
	assert.Contains(t, w.Header().Get("Set-Cookie"), "datum_session=")
}

func TestHandleUserLogin_InvalidCredentials(t *testing.T) {
	mockStore := new(MockProvider)
	svc := New([]byte("secret"), mockStore)
	r := setupTestRouter(svc)

	mockStore.On("GetUserByEmail", "test@example.com").Return(nil, errors.New("not found"))

	body := `email=test@example.com&password=wrong&redirect=/home`
	req, _ := http.NewRequest("POST", "/auth/provider/login", bytes.NewBufferString(body))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusFound, w.Code)
	assert.Contains(t, w.Header().Get("Location"), "/auth/provider/login?error=Invalid+credentials")
}

func TestHandleUserLogin_EmptyFields(t *testing.T) {
	mockStore := new(MockProvider)
	svc := New([]byte("secret"), mockStore)
	r := setupTestRouter(svc)

	body := `email=&password=&redirect=/home`
	req, _ := http.NewRequest("POST", "/auth/provider/login", bytes.NewBufferString(body))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusFound, w.Code)
	assert.Contains(t, w.Header().Get("Location"), "Email+and+password+required")
}

func TestHandleTokenIssuance_Success(t *testing.T) {
	mockStore := new(MockProvider)
	svc := New([]byte("secret"), mockStore)
	r := setupTestRouter(svc)

	mockStore.On("ValidateAuthClient", "valid_client", "valid_secret").Return("{admin,user}", nil)

	body := `client_id=valid_client&client_secret=valid_secret`
	req, _ := http.NewRequest("POST", "/auth/provider/token", bytes.NewBufferString(body))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	var response map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.NotEmpty(t, response["access_token"])
	assert.Equal(t, float64(86400), response["expires_in"])
}

func TestHandleTokenIssuance_Invalid(t *testing.T) {
	mockStore := new(MockProvider)
	svc := New([]byte("secret"), mockStore)
	r := setupTestRouter(svc)

	mockStore.On("ValidateAuthClient", "invalid", "invalid").Return("", errors.New("not found"))

	body := `client_id=invalid&client_secret=invalid`
	req, _ := http.NewRequest("POST", "/auth/provider/token", bytes.NewBufferString(body))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func TestHandleTokenValidation_Success(t *testing.T) {
	mockStore := new(MockProvider)
	svc := New([]byte("secret"), mockStore)
	r := setupTestRouter(svc)

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"user_id": "user123",
		"email":   "test@example.com",
		"role":    "user",
		"iat":     time.Now().Unix(),
	})
	tokenString, _ := token.SignedString([]byte("secret"))

	req, _ := http.NewRequest("GET", "/auth/provider/validate", nil)
	req.Header.Set("Authorization", "Bearer "+tokenString)
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	var response map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	assert.NoError(t, err)
	assert.True(t, response["valid"].(bool))
	assert.Equal(t, "user123", response["user_id"])
}

func TestHandleTokenValidation_Invalid(t *testing.T) {
	mockStore := new(MockProvider)
	svc := New([]byte("secret"), mockStore)
	r := setupTestRouter(svc)

	req, _ := http.NewRequest("GET", "/auth/provider/validate", nil)
	req.Header.Set("Authorization", "Bearer invalid-token")
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func TestHandleCheck_RolesAndEmails(t *testing.T) {
	mockStore := new(MockProvider)
	svc := New([]byte("secret"), mockStore)
	r := setupTestRouter(svc)

	// Create a token for an admin
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"user_id": "user123",
		"email":   "admin@example.com",
		"role":    "admin",
	})
	tokenString, _ := token.SignedString([]byte("secret"))

	// Test 1: Allowed by Role
	mockStore.On("GetAuthClientRolesAndEmails", "admin.example.com").Return("{admin}", "", nil).Once()

	req1, _ := http.NewRequest("GET", "/auth/provider/check", nil)
	req1.Header.Set("X-Forwarded-Host", "admin.example.com")
	req1.Header.Set("Authorization", "Bearer "+tokenString)
	w1 := httptest.NewRecorder()

	r.ServeHTTP(w1, req1)
	assert.Equal(t, http.StatusOK, w1.Code)

	// Test 2: Denied by Role
	mockStore.On("GetAuthClientRolesAndEmails", "user.example.com").Return("{user}", "", nil).Once()

	req2, _ := http.NewRequest("GET", "/auth/provider/check", nil)
	req2.Header.Set("X-Forwarded-Host", "user.example.com")
	req2.Header.Set("Authorization", "Bearer "+tokenString)
	w2 := httptest.NewRecorder()

	r.ServeHTTP(w2, req2)
	assert.Equal(t, http.StatusFound, w2.Code) // redirected to login with error

	// Test 3: Allowed by Email (despite role mismatch)
	mockStore.On("GetAuthClientRolesAndEmails", "specific.example.com").Return("{user}", "{admin@example.com}", nil).Once()

	req3, _ := http.NewRequest("GET", "/auth/provider/check", nil)
	req3.Header.Set("X-Forwarded-Host", "specific.example.com")
	req3.Header.Set("Authorization", "Bearer "+tokenString)
	w3 := httptest.NewRecorder()

	r.ServeHTTP(w3, req3)
	assert.Equal(t, http.StatusOK, w3.Code)
}

func TestHandleOAuthAuthorize_NoSession(t *testing.T) {
	mockStore := new(MockProvider)
	svc := New([]byte("this-is-a-very-long-secret-key-that-is-at-least-32-bytes-long"), mockStore)
	r := setupTestRouter(svc)

	expectedClients := []map[string]interface{}{
		{"name": "app1", "client_id": "client123"},
	}
	mockStore.On("ListAuthClients").Return(expectedClients, nil)

	req, _ := http.NewRequest("GET", "/auth/provider/oauth/authorize?client_id=client123&redirect_uri=http://example.com/callback&response_type=code&state=xyz", nil)
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	// Since there is no session cookie, it must redirect to the login page
	assert.Equal(t, http.StatusFound, w.Code)
	assert.Contains(t, w.Header().Get("Location"), "login")
}

func TestHandleOAuthAuthorize_WithSession(t *testing.T) {
	mockStore := new(MockProvider)
	svc := New([]byte("this-is-a-very-long-secret-key-that-is-at-least-32-bytes-long"), mockStore)
	r := setupTestRouter(svc)

	expectedClients := []map[string]interface{}{
		{"name": "app1", "client_id": "client123"},
	}
	mockStore.On("ListAuthClients").Return(expectedClients, nil)

	// Generate a valid datum_session cookie
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"user_id": "user123",
		"email":   "user@example.com",
		"role":    "user",
		"exp":     time.Now().Add(time.Hour).Unix(),
	})
	tokenString, _ := token.SignedString([]byte("this-is-a-very-long-secret-key-that-is-at-least-32-bytes-long"))

	req, _ := http.NewRequest("GET", "/auth/provider/oauth/authorize?client_id=client123&redirect_uri=http://example.com/callback&response_type=code&state=xyz", nil)
	req.AddCookie(&http.Cookie{
		Name:  "datum_session",
		Value: tokenString,
	})
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusFound, w.Code)
	location := w.Header().Get("Location")
	assert.Contains(t, location, "http://example.com/callback")
	assert.Contains(t, location, "code=")
	assert.Contains(t, location, "state=xyz")
}

func TestHandleOAuthToken_Success(t *testing.T) {
	mockStore := new(MockProvider)
	svc := New([]byte("this-is-a-very-long-secret-key-that-is-at-least-32-bytes-long"), mockStore)
	r := setupTestRouter(svc)

	// Seed code
	code := "test_code_123"
	svc.oauthCodes.Store(code, oauthCode{
		UserID:      "user123",
		ClientID:    "client123",
		RedirectURI: "http://example.com/callback",
		ExpiresAt:   time.Now().Add(5 * time.Minute),
		State:       "xyz",
	})

	mockStore.On("ValidateAuthClient", "client123", "secret123").Return("{user}", nil)
	mockUser := &storage.User{ID: "user123", Email: "user@example.com", Role: "user"}
	mockStore.On("GetUserByID", "user123").Return(mockUser, nil)

	body := "grant_type=authorization_code&code=test_code_123&redirect_uri=http://example.com/callback&client_id=client123&client_secret=secret123"
	req, _ := http.NewRequest("POST", "/auth/provider/oauth/token", bytes.NewBufferString(body))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	var resp map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &resp)
	assert.NoError(t, err)
	assert.NotEmpty(t, resp["access_token"])
	assert.NotEmpty(t, resp["id_token"])
	assert.Equal(t, "Bearer", resp["token_type"])
}

func TestHandleOAuthUserinfo_Success(t *testing.T) {
	mockStore := new(MockProvider)
	svc := New([]byte("this-is-a-very-long-secret-key-that-is-at-least-32-bytes-long"), mockStore)
	r := setupTestRouter(svc)

	mockUser := &storage.User{ID: "user123", Email: "user@example.com", Role: "user"}
	mockStore.On("GetUserByID", "user123").Return(mockUser, nil)

	// Generate access token
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"user_id": "user123",
		"email":   "user@example.com",
		"role":    "user",
		"exp":     time.Now().Add(time.Hour).Unix(),
	})
	tokenString, _ := token.SignedString([]byte("this-is-a-very-long-secret-key-that-is-at-least-32-bytes-long"))

	req, _ := http.NewRequest("GET", "/auth/provider/oauth/userinfo", nil)
	req.Header.Set("Authorization", "Bearer "+tokenString)
	w := httptest.NewRecorder()

	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	var resp map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &resp)
	assert.NoError(t, err)
	assert.Equal(t, "user123", resp["sub"])
	assert.Equal(t, "user@example.com", resp["email"])
	assert.Equal(t, "user", resp["name"]) // display name falls back to email local part
}
