package auth_test

import (
	"bytes"
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

// Mock Storage Provider
type MockStorage struct {
	mock.Mock
	storage.Provider // Embed interface to satisfy it partially
}

func (m *MockStorage) IsSystemInitialized() bool {
	args := m.Called()
	return args.Bool(0)
}

func (m *MockStorage) GetSystemConfig() (*storage.SystemConfig, error) {
	args := m.Called()
	return args.Get(0).(*storage.SystemConfig), args.Error(1)
}

func (m *MockStorage) CreateUser(u *storage.User) error {
	args := m.Called(u)
	return args.Error(0)
}

func (m *MockStorage) GetUserByEmail(email string) (*storage.User, error) {
	args := m.Called(email)
	if args.Get(0) == nil {
		return nil, args.Error(1)
	}
	return args.Get(0).(*storage.User), args.Error(1)
}
func (m *MockStorage) GetUserByID(id string) (*storage.User, error) {
	args := m.Called(id)
	if args.Get(0) == nil {
		return nil, args.Error(1)
	}
	return args.Get(0).(*storage.User), args.Error(1)
}

// --- Local Provider Tests ---

func TestLocalRegister(t *testing.T) {
	gin.SetMode(gin.TestMode)
	mockStore := new(MockStorage)
	provider := auth.NewLocalProvider(mockStore)

	// Setup valid initial state
	mockStore.On("IsSystemInitialized").Return(true)
	mockStore.On("GetSystemConfig").Return(&storage.SystemConfig{AllowRegister: true}, nil)
	mockStore.On("CreateUser", mock.Anything).Return(nil)

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)

	reqBody := `{"email":"test@example.com", "password":"password123"}`
	c.Request, _ = http.NewRequest("POST", "/auth/register", bytes.NewBufferString(reqBody))
	c.Request.Header.Set("Content-Type", "application/json")

	provider.RegisterHandler(c)

	assert.Equal(t, http.StatusCreated, w.Code)
	mockStore.AssertCalled(t, "CreateUser", mock.Anything)
}

// --- Supabase Provider Tests ---

func TestSupabaseMethodsDisabled(t *testing.T) {
	gin.SetMode(gin.TestMode)
	mockStore := new(MockStorage)
	provider := auth.NewSupabaseProvider(mockStore, "secret")

	// Verify Login returns 405
	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	provider.LoginHandler(c)
	assert.Equal(t, http.StatusMethodNotAllowed, w.Code)
}

func TestSupabaseMiddlewareJIT(t *testing.T) {
	gin.SetMode(gin.TestMode)
	mockStore := new(MockStorage)
	secret := "supersecretkeyforcestingpurposesonly123"
	provider := auth.NewSupabaseProvider(mockStore, secret)

	// Mock JIT: User does not exist, so CreateUser should be called
	mockStore.On("GetUserByID", "user123").Return(nil, assert.AnError) // Simulate Not Found
	mockStore.On("CreateUser", mock.MatchedBy(func(u *storage.User) bool {
		return u.ID == "user123" && u.Email == "jit@example.com"
	})).Return(nil)

	// Create a valid JWT signed with the secret
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"sub":   "user123",
		"email": "jit@example.com",
		"exp":   time.Now().Add(time.Hour).Unix(),
	})
	tokenString, _ := token.SignedString([]byte(secret))

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request, _ = http.NewRequest("GET", "/protected", nil)
	c.Request.Header.Set("Authorization", "Bearer "+tokenString)

	// Execute Middleware
	handler := provider.AuthMiddleware()
	handler(c)

	// Check if middleware allowed request to proceed (Next called)
	// Gin specific: if aborted, IsAborted() is true. If not, it assumes success.
	assert.False(t, c.IsAborted())

	val, exists := c.Get("user_id")
	assert.True(t, exists)
	assert.Equal(t, "user123", val)

	mockStore.AssertCalled(t, "CreateUser", mock.Anything)
}
