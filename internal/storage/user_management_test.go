package storage

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// ============ User Management Tests ============

func TestUpdateUser(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create user
	user := &User{
		ID:           "user_update_001",
		Email:        "update@test.com",
		PasswordHash: "hash123",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	// Update role to admin
	err = storage.UpdateUser("user_update_001", "admin", "")
	require.NoError(t, err)

	// Verify update
	updated, err := storage.GetUserByID("user_update_001")
	require.NoError(t, err)
	assert.Equal(t, "admin", updated.Role)
	assert.Equal(t, "active", updated.Status) // Should remain unchanged
}

func TestUpdateUserStatus(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create user
	user := &User{
		ID:           "user_status_001",
		Email:        "status@test.com",
		PasswordHash: "hash123",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	storage.CreateUser(user)

	// Update status to suspended
	err := storage.UpdateUser("user_status_001", "", "suspended")
	require.NoError(t, err)

	// Verify update
	updated, err := storage.GetUserByID("user_status_001")
	require.NoError(t, err)
	assert.Equal(t, "suspended", updated.Status)
	assert.Equal(t, "user", updated.Role) // Should remain unchanged
}

func TestUpdateUserBothFields(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create user
	user := &User{
		ID:           "user_both_001",
		Email:        "both@test.com",
		PasswordHash: "hash123",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	storage.CreateUser(user)

	// Update both role and status
	err := storage.UpdateUser("user_both_001", "admin", "suspended")
	require.NoError(t, err)

	// Verify updates
	updated, err := storage.GetUserByID("user_both_001")
	require.NoError(t, err)
	assert.Equal(t, "admin", updated.Role)
	assert.Equal(t, "suspended", updated.Status)
}

func TestUpdateUserNotFound(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	err := storage.UpdateUser("user_nonexistent", "admin", "active")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "user not found")
}

func TestUpdateUserLastLogin(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create user
	user := &User{
		ID:           "user_login_001",
		Email:        "login@test.com",
		PasswordHash: "hash123",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	storage.CreateUser(user)

	// Record login time
	beforeLogin := time.Now()
	err := storage.UpdateUserLastLogin("user_login_001")
	require.NoError(t, err)
	afterLogin := time.Now()

	// Verify last login was updated
	updated, err := storage.GetUserByID("user_login_001")
	require.NoError(t, err)
	assert.True(t, updated.LastLoginAt.After(beforeLogin) || updated.LastLoginAt.Equal(beforeLogin))
	assert.True(t, updated.LastLoginAt.Before(afterLogin) || updated.LastLoginAt.Equal(afterLogin))
}

func TestUpdateUserPassword(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create user
	user := &User{
		ID:           "user_pwd_001",
		Email:        "password@test.com",
		PasswordHash: "old_hash_123",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	storage.CreateUser(user)

	// Update password
	newHash := "new_hash_456"
	err := storage.UpdateUserPassword("user_pwd_001", newHash)
	require.NoError(t, err)

	// Verify password was updated
	updated, err := storage.GetUserByID("user_pwd_001")
	require.NoError(t, err)
	assert.Equal(t, newHash, updated.PasswordHash)
}

func TestDeleteUser(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create user with devices
	user := &User{
		ID:           "user_delete_001",
		Email:        "delete@test.com",
		PasswordHash: "hash123",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	storage.CreateUser(user)

	// Create devices for user
	for i := 1; i <= 3; i++ {
		device := &Device{
			ID:        "dev_delete_" + string(rune('0'+i)),
			UserID:    "user_delete_001",
			Name:      "Device " + string(rune('0'+i)),
			Type:      "sensor",
			APIKey:    "sk_delete_" + string(rune('0'+i)),
			Status:    "active",
			CreatedAt: time.Now(),
		}
		storage.CreateDevice(device)
	}

	// Delete user
	err := storage.DeleteUser("user_delete_001")
	require.NoError(t, err)

	// Verify user is deleted
	_, err = storage.GetUserByID("user_delete_001")
	assert.Error(t, err)

	// Verify user's devices are also deleted
	devices, err := storage.GetUserDevices("user_delete_001")
	require.NoError(t, err)
	assert.Empty(t, devices)
}

func TestDeleteUserCascade(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create user
	user := &User{
		ID:           "user_cascade_001",
		Email:        "cascade@test.com",
		PasswordHash: "hash123",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	storage.CreateUser(user)

	// Create device
	device := &Device{
		ID:        "dev_cascade_001",
		UserID:    "user_cascade_001",
		Name:      "Cascade Device",
		Type:      "sensor",
		APIKey:    "sk_cascade_001",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	// Verify device exists
	_, err := storage.GetDevice("dev_cascade_001")
	require.NoError(t, err)

	// Delete user (should cascade delete devices)
	err = storage.DeleteUser("user_cascade_001")
	require.NoError(t, err)

	// Verify device is also deleted
	_, err = storage.GetDevice("dev_cascade_001")
	assert.Error(t, err)

	// Verify API key index is cleaned up
	_, err = storage.GetDeviceByAPIKey("sk_cascade_001")
	assert.Error(t, err)
}

func TestDeleteUserNotFound(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	err := storage.DeleteUser("user_nonexistent")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "user not found")
}

// ============ Admin Device Management Tests ============

func TestForceDeleteDevice(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create user and device
	user := &User{
		ID:           "user_force_001",
		Email:        "force@test.com",
		PasswordHash: "hash123",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	storage.CreateUser(user)

	device := &Device{
		ID:        "dev_force_001",
		UserID:    "user_force_001",
		Name:      "Force Delete Device",
		Type:      "sensor",
		APIKey:    "sk_force_001",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	// Force delete device (admin action, bypasses ownership check)
	err := storage.ForceDeleteDevice("dev_force_001")
	require.NoError(t, err)

	// Verify device is deleted
	_, err = storage.GetDevice("dev_force_001")
	assert.Error(t, err)

	// Verify removed from user's device list
	devices, err := storage.GetUserDevices("user_force_001")
	require.NoError(t, err)
	assert.Empty(t, devices)
}

func TestForceDeleteDeviceNotFound(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	err := storage.ForceDeleteDevice("dev_nonexistent")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "device not found")
}

func TestListAllDevices(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create multiple users with devices
	for i := 1; i <= 2; i++ {
		user := &User{
			ID:           "user_list_" + string(rune('0'+i)),
			Email:        "user" + string(rune('0'+i)) + "@test.com",
			PasswordHash: "hash123",
			Role:         "user",
			Status:       "active",
			CreatedAt:    time.Now(),
		}
		storage.CreateUser(user)

		for j := 1; j <= 3; j++ {
			device := &Device{
				ID:        "dev_list_" + string(rune('0'+i)) + "_" + string(rune('0'+j)),
				UserID:    "user_list_" + string(rune('0'+i)),
				Name:      "Device " + string(rune('0'+i)) + "-" + string(rune('0'+j)),
				Type:      "sensor",
				APIKey:    "sk_list_" + string(rune('0'+i)) + "_" + string(rune('0'+j)),
				Status:    "active",
				CreatedAt: time.Now(),
			}
			storage.CreateDevice(device)
		}
	}

	// List all devices (admin view)
	devices, err := storage.ListAllDevices()
	require.NoError(t, err)
	assert.Len(t, devices, 6) // 2 users × 3 devices each
}

func TestUpdateDeviceStatus(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create device
	device := &Device{
		ID:        "dev_status_001",
		UserID:    "user_test",
		Name:      "Status Device",
		Type:      "sensor",
		APIKey:    "sk_status_001",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	// Update status to banned
	err := storage.UpdateDevice("dev_status_001", "banned")
	require.NoError(t, err)

	// Verify status was updated
	updated, err := storage.GetDevice("dev_status_001")
	require.NoError(t, err)
	assert.Equal(t, "banned", updated.Status)
}

func TestUpdateDeviceNotFound(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	err := storage.UpdateDevice("dev_nonexistent", "banned")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "device not found")
}

// ============ Integration Tests ============

func TestUserManagementLifecycle(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// 1. Create user
	user := &User{
		ID:           "user_lifecycle_001",
		Email:        "lifecycle@test.com",
		PasswordHash: "initial_hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	// 2. User logs in
	err = storage.UpdateUserLastLogin("user_lifecycle_001")
	require.NoError(t, err)

	// 3. User changes password
	err = storage.UpdateUserPassword("user_lifecycle_001", "new_hash")
	require.NoError(t, err)

	// 4. Admin promotes to admin role
	err = storage.UpdateUser("user_lifecycle_001", "admin", "")
	require.NoError(t, err)

	// 5. Admin suspends user
	err = storage.UpdateUser("user_lifecycle_001", "", "suspended")
	require.NoError(t, err)

	// 6. Verify final state
	final, err := storage.GetUserByID("user_lifecycle_001")
	require.NoError(t, err)
	assert.Equal(t, "admin", final.Role)
	assert.Equal(t, "suspended", final.Status)
	assert.Equal(t, "new_hash", final.PasswordHash)
	assert.False(t, final.LastLoginAt.IsZero())

	// 7. Admin deletes user
	err = storage.DeleteUser("user_lifecycle_001")
	require.NoError(t, err)

	_, err = storage.GetUserByID("user_lifecycle_001")
	assert.Error(t, err)
}
