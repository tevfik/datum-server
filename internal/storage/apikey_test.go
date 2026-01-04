package storage

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
)

func TestCreateUserAPIKey(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create a user first (needed for ForeignKey-like behavior if we enforced it,
	// though BuntDB is NoSQL, good practice for integration)
	user := &User{ID: "u1", Email: "test@example.com"}
	storage.CreateUser(user)

	apiKey := &APIKey{
		ID:        "key_1",
		UserID:    "u1",
		Name:      "Test Key",
		Key:       "ak_1234567890abcdef",
		CreatedAt: time.Now(),
	}

	err := storage.CreateUserAPIKey(apiKey)
	assert.NoError(t, err)

	// Verify lookup by key string (Auth flow)
	foundUser, err := storage.GetUserByUserAPIKey("ak_1234567890abcdef")
	assert.NoError(t, err)
	assert.Equal(t, "u1", foundUser.ID)
}

func TestGetUserAPIKeys(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create keys for u1
	storage.CreateUserAPIKey(&APIKey{ID: "k1", UserID: "u1", Name: "K1", Key: "ak_1", CreatedAt: time.Now()})
	storage.CreateUserAPIKey(&APIKey{ID: "k2", UserID: "u1", Name: "K2", Key: "ak_2", CreatedAt: time.Now()})

	// Create key for u2
	storage.CreateUserAPIKey(&APIKey{ID: "k3", UserID: "u2", Name: "K3", Key: "ak_3", CreatedAt: time.Now()})

	// List u1 keys
	keys, err := storage.GetUserAPIKeys("u1")
	assert.NoError(t, err)
	assert.Len(t, keys, 2)
	assert.Equal(t, "K1", keys[0].Name)
	assert.Equal(t, "K2", keys[1].Name)

	// List u2 keys
	keys2, err := storage.GetUserAPIKeys("u2")
	assert.NoError(t, err)
	assert.Len(t, keys2, 1)
}

func TestDeleteUserAPIKey(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "u1", Email: "test@example.com"}
	storage.CreateUser(user)

	key := &APIKey{ID: "k1", UserID: "u1", Name: "Delete Me", Key: "ak_delete", CreatedAt: time.Now()}
	storage.CreateUserAPIKey(key)

	// Verify it exists
	u, err := storage.GetUserByUserAPIKey("ak_delete")
	assert.NoError(t, err)
	assert.NotNil(t, u)

	// Delete
	err = storage.DeleteUserAPIKey("u1", "k1")
	assert.NoError(t, err)

	// Verify Auth fails
	_, err = storage.GetUserByUserAPIKey("ak_delete")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "invalid api key")

	// Verify List is empty
	keys, _ := storage.GetUserAPIKeys("u1")
	assert.Empty(t, keys)
}

func TestDeleteUserAPIKey_Unauthorized(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	key := &APIKey{ID: "k1", UserID: "u1", Name: "K1", Key: "ak_1"}
	storage.CreateUserAPIKey(key)

	// Try to delete u1's key as u2
	err := storage.DeleteUserAPIKey("u2", "k1")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "unauthorized")
}
