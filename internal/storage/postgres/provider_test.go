package postgres

import (
	"os"
	"testing"
	"time"

	"datum-go/internal/storage"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// setupPostgres Test Helper
func setupPostgres(t *testing.T) (*PostgresStore, func()) {
	dbURL := os.Getenv("DATABASE_URL")
	if dbURL == "" {
		t.Skip("Skipping Postgres integration tests: DATABASE_URL not set")
	}

	// Use a unique DB/Suffix if possible, but for now assuming specific test DB
	store, err := New(dbURL)
	require.NoError(t, err)

	// Clean up data before/after?
	// For integration tests, we usually want to start clean.
	err = store.ResetSystem()
	require.NoError(t, err)

	return store, func() {
		store.Close()
	}
}

func TestPostgres_UserOps(t *testing.T) {
	s, cleanup := setupPostgres(t)
	defer cleanup()

	user := &storage.User{
		ID:           "pg_test_user",
		Email:        "pg_test@example.com",
		PasswordHash: "hash123",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}

	// Create
	err := s.CreateUser(user)
	assert.NoError(t, err)

	// Get
	u, err := s.GetUserByID(user.ID)
	assert.NoError(t, err)
	assert.Equal(t, user.Email, u.Email)

	// List
	users, err := s.ListAllUsers()
	assert.NoError(t, err)
	assert.GreaterOrEqual(t, len(users), 1)

	// Update
	err = s.UpdateUser(user.ID, "admin", "suspended")
	assert.NoError(t, err)

	u2, _ := s.GetUserByID(user.ID)
	assert.Equal(t, "admin", u2.Role)
	assert.Equal(t, "suspended", u2.Status)

	// Delete
	err = s.DeleteUser(user.ID)
	assert.NoError(t, err)

	_, err = s.GetUserByID(user.ID)
	assert.Error(t, err)
}

func TestPostgres_DeviceOps(t *testing.T) {
	s, cleanup := setupPostgres(t)
	defer cleanup()

	// Need a user first
	s.CreateUser(&storage.User{ID: "pg_dev_owner", Email: "owner@test.com", Status: "active"})

	device := &storage.Device{
		ID:        "pg_device_1",
		UserID:    "pg_dev_owner",
		Name:      "Postgres Device",
		Type:      "sensor",
		Status:    "active",
		APIKey:    "pg_ak_12345",
		CreatedAt: time.Now(),
	}

	// Create
	err := s.CreateDevice(device)
	assert.NoError(t, err)

	// Get
	d, err := s.GetDevice(device.ID)
	assert.NoError(t, err)
	assert.Equal(t, device.Name, d.Name)

	// Update Token
	newToken := "pg_dk_newtoken"
	expiry := time.Now().Add(24 * time.Hour)
	d, err = s.RotateDeviceKey(device.ID, newToken, expiry, 7)
	assert.NoError(t, err)
	assert.Equal(t, newToken, d.CurrentToken)

	// Get by Token
	d2, isToken, err := s.GetDeviceByToken(newToken)
	assert.NoError(t, err)
	assert.True(t, isToken)
	assert.Equal(t, device.ID, d2.ID)
}

func TestPostgres_DataOps(t *testing.T) {
	s, cleanup := setupPostgres(t)
	defer cleanup()

	s.CreateUser(&storage.User{ID: "pg_data_owner", Email: "data@test.com", Status: "active"})
	s.CreateDevice(&storage.Device{ID: "pg_data_dev", UserID: "pg_data_owner", Name: "Data Dev", Status: "active"})

	// Store Data
	pt := &storage.DataPoint{
		DeviceID:  "pg_data_dev",
		Timestamp: time.Now(),
		Data:      map[string]interface{}{"temp": 42.5},
	}
	err := s.StoreData(pt)
	assert.NoError(t, err)

	// Get Latest (Shadow)
	latest, err := s.GetLatestData("pg_data_dev")
	assert.NoError(t, err)
	assert.NotNil(t, latest)
	// JSON unmarshals numbers as float64
	assert.Equal(t, 42.5, latest.Data["temp"])

	// Get History
	hist, err := s.GetDataHistory("pg_data_dev", 10)
	assert.NoError(t, err)
	assert.Len(t, hist, 1)
}

func TestPostgres_SystemOps(t *testing.T) {
	s, cleanup := setupPostgres(t)
	defer cleanup()

	// Config
	err := s.InitializeSystem("Test Platform", true, 30)
	assert.NoError(t, err)

	inited := s.IsSystemInitialized()
	assert.True(t, inited)

	conf, err := s.GetSystemConfig()
	assert.NoError(t, err)
	assert.Equal(t, "Test Platform", conf.PlatformName)
}
