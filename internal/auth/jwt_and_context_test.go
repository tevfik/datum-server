package auth

import (
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// ============ SetJWTSecret / GetJWTSecret Tests ============

func TestSetJWTSecret_Valid(t *testing.T) {
	// Save original secret to restore later
	original := GetJWTSecret()
	defer SetJWTSecret(original)

	newSecret := []byte("this-is-a-valid-32-byte-secret!!")
	require.Equal(t, 32, len(newSecret))

	SetJWTSecret(newSecret)
	result := GetJWTSecret()
	assert.Equal(t, newSecret, result)
}

func TestSetJWTSecret_TooShort(t *testing.T) {
	// Save original
	original := GetJWTSecret()
	defer SetJWTSecret(original)

	// Setting a too-short secret should be ignored
	short := []byte("short")
	SetJWTSecret(short)

	result := GetJWTSecret()
	// Should NOT have changed (short secret rejected)
	assert.Equal(t, original, result)
}

func TestGetJWTSecret_ReturnsDefensiveCopy(t *testing.T) {
	s1 := GetJWTSecret()
	s2 := GetJWTSecret()

	assert.Equal(t, s1, s2)

	// Modifying s1 should NOT affect s2 or the stored secret
	if len(s1) > 0 {
		s1[0] = 0xFF
	}

	s3 := GetJWTSecret()
	// s3 should still equal original s2
	assert.Equal(t, s2, s3)
}

// ============ GetUserRole / GetSessionJTI Tests ============

func TestGetUserRole_Success(t *testing.T) {
	gin.SetMode(gin.TestMode)
	c, _ := gin.CreateTestContext(nil)
	c.Set("role", "admin")

	role, err := GetUserRole(c)
	assert.NoError(t, err)
	assert.Equal(t, "admin", role)
}

func TestGetUserRole_Missing(t *testing.T) {
	gin.SetMode(gin.TestMode)
	c, _ := gin.CreateTestContext(nil)

	_, err := GetUserRole(c)
	assert.Error(t, err)
}

func TestGetSessionJTI_Present(t *testing.T) {
	gin.SetMode(gin.TestMode)
	c, _ := gin.CreateTestContext(nil)
	c.Set("session_jti", "jti_abc123")

	jti := GetSessionJTI(c)
	assert.Equal(t, "jti_abc123", jti)
}

func TestGetSessionJTI_Missing(t *testing.T) {
	gin.SetMode(gin.TestMode)
	c, _ := gin.CreateTestContext(nil)

	jti := GetSessionJTI(c)
	assert.Equal(t, "", jti)
}

func TestGetSessionJTI_WrongType(t *testing.T) {
	gin.SetMode(gin.TestMode)
	c, _ := gin.CreateTestContext(nil)
	// Set wrong type (not string)
	c.Set("session_jti", 12345)

	jti := GetSessionJTI(c)
	assert.Equal(t, "", jti)
}

// ============ getRefreshExpiryDays / getAccessExpiryMinutes Tests ============

func TestGetRefreshExpiryDays_Default(t *testing.T) {
	t.Setenv("JWT_REFRESH_EXPIRY", "")
	days := getRefreshExpiryDays()
	assert.Equal(t, 30, days)
}

func TestGetRefreshExpiryDays_CustomValue(t *testing.T) {
	t.Setenv("JWT_REFRESH_EXPIRY", "90")
	days := getRefreshExpiryDays()
	assert.Equal(t, 90, days)
}

func TestGetRefreshExpiryDays_InvalidValue(t *testing.T) {
	t.Setenv("JWT_REFRESH_EXPIRY", "not-a-number")
	days := getRefreshExpiryDays()
	assert.Equal(t, 30, days) // Falls back to default
}

func TestGetRefreshExpiryDays_ZeroValue(t *testing.T) {
	t.Setenv("JWT_REFRESH_EXPIRY", "0")
	days := getRefreshExpiryDays()
	assert.Equal(t, 30, days) // Falls back to default (0 is invalid)
}

func TestGetRefreshExpiryDays_NegativeValue(t *testing.T) {
	t.Setenv("JWT_REFRESH_EXPIRY", "-5")
	days := getRefreshExpiryDays()
	assert.Equal(t, 30, days) // Falls back to default (negative is invalid)
}

func TestGetAccessExpiryMinutes_Default(t *testing.T) {
	t.Setenv("JWT_ACCESS_EXPIRY_MINUTES", "")
	mins := getAccessExpiryMinutes()
	assert.Equal(t, 15, mins)
}

func TestGetAccessExpiryMinutes_CustomValue(t *testing.T) {
	t.Setenv("JWT_ACCESS_EXPIRY_MINUTES", "60")
	mins := getAccessExpiryMinutes()
	assert.Equal(t, 60, mins)
}

func TestGetAccessExpiryMinutes_InvalidValue(t *testing.T) {
	t.Setenv("JWT_ACCESS_EXPIRY_MINUTES", "abc")
	mins := getAccessExpiryMinutes()
	assert.Equal(t, 15, mins) // Falls back to default
}

func TestGetAccessExpiryMinutes_ZeroValue(t *testing.T) {
	t.Setenv("JWT_ACCESS_EXPIRY_MINUTES", "0")
	mins := getAccessExpiryMinutes()
	assert.Equal(t, 15, mins) // Falls back to default
}
