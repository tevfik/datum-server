package storage

import (
	"fmt"
	"path/filepath"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestGetUserDeviceCounts(t *testing.T) {
	// Setup
	tmpDir := t.TempDir()
	metaPath := filepath.Join(tmpDir, "test_counts.db")
	dataPath := filepath.Join(tmpDir, "tsdata_counts")

	store, err := New(metaPath, dataPath, 7*24*time.Hour)
	require.NoError(t, err)
	defer store.Close()

	// Create a user with 5 devices
	userID := "user_counts_test"
	user := &User{
		ID:        userID,
		Email:     "counts@example.com",
		Role:      "user",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	require.NoError(t, store.CreateUser(user))

	for i := 0; i < 5; i++ {
		device := &Device{
			ID:        fmt.Sprintf("dev_counts_%d", i),
			UserID:    userID,
			Name:      "Device",
			Type:      "sensor",
			APIKey:    fmt.Sprintf("ak_counts_%d", i),
			Status:    "active",
			CreatedAt: time.Now(),
		}
		require.NoError(t, store.CreateDevice(device))
	}

	// Create another user with 0 devices
	user2 := &User{ID: "user_empty", Email: "empty@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, store.CreateUser(user2))

	// Verify
	counts, err := store.GetUserDeviceCounts()
	require.NoError(t, err)

	assert.Equal(t, 5, counts[userID])
	assert.Equal(t, 0, counts["user_empty"])
}

func BenchmarkGetUserDeviceCounts(b *testing.B) {
	// Setup
	tmpDir := b.TempDir()
	metaPath := filepath.Join(tmpDir, "bench_counts.db")
	dataPath := filepath.Join(tmpDir, "tsdata_counts")

	store, err := New(metaPath, dataPath, 7*24*time.Hour)
	require.NoError(b, err)
	defer store.Close()

	// Populate with users and devices
	numUsers := 100
	devicesPerUser := 10

	for i := 0; i < numUsers; i++ {
		userID := fmt.Sprintf("user_%d", i)
		user := &User{
			ID:        userID,
			Email:     fmt.Sprintf("user%d@example.com", i),
			Role:      "user",
			Status:    "active",
			CreatedAt: time.Now(),
		}
		require.NoError(b, store.CreateUser(user))

		for j := 0; j < devicesPerUser; j++ {
			device := &Device{
				ID:        fmt.Sprintf("dev_%d_%d", i, j),
				UserID:    userID,
				Name:      "Device",
				Type:      "sensor",
				APIKey:    fmt.Sprintf("ak_%d_%d", i, j),
				Status:    "active",
				CreatedAt: time.Now(),
			}
			require.NoError(b, store.CreateDevice(device))
		}
	}

	// Get list of users for N+1 benchmark
	users, _ := store.ListAllUsers()

	b.Run("N+1 Query", func(b *testing.B) {
		for i := 0; i < b.N; i++ {
			// Simulate the N+1 pattern found in the handler
			for _, u := range users {
				devices, _ := store.GetUserDevices(u.ID)
				_ = len(devices)
			}
		}
	})

	b.Run("Optimized Query", func(b *testing.B) {
		for i := 0; i < b.N; i++ {
			counts, err := store.GetUserDeviceCounts()
			if err != nil {
				b.Fatal(err)
			}
            // Basic sanity check to ensure we aren't getting empty map
            if len(counts) != numUsers {
                 b.Fatalf("Expected %d users in counts, got %d", numUsers, len(counts))
            }
		}
	})
}
