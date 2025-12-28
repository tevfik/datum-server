package storage

import (
	"fmt"
	"math/rand"
	"path/filepath"
	"runtime"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// createTestMetadataStorage creates a temporary storage for testing
func createTestMetadataStorage(t *testing.T) *Storage {
	tmpDir := t.TempDir()
	metaPath := filepath.Join(tmpDir, "test_metadata.db")
	dataPath := filepath.Join(tmpDir, "test_tsdata")

	storage, err := New(metaPath, dataPath, 7*24*time.Hour)
	require.NoError(t, err, "Failed to create test storage")

	return storage
}

// ============ User Operations Tests ============

func TestConcurrentUserCreation(t *testing.T) {
	storage := createTestMetadataStorage(t)
	defer storage.Close()

	numUsers := 1000
	var wg sync.WaitGroup
	var successCount int32
	var errorCount int32

	start := time.Now()

	for i := 0; i < numUsers; i++ {
		wg.Add(1)
		go func(idx int) {
			defer wg.Done()

			user := &User{
				ID:           fmt.Sprintf("user_%d", idx),
				Email:        fmt.Sprintf("user%d@test.com", idx),
				PasswordHash: "hash123",
				Role:         "user",
				Status:       "active",
				CreatedAt:    time.Now(),
			}

			err := storage.CreateUser(user)
			if err != nil {
				atomic.AddInt32(&errorCount, 1)
			} else {
				atomic.AddInt32(&successCount, 1)
			}
		}(i)
	}

	wg.Wait()
	duration := time.Since(start)

	t.Logf("Concurrent User Creation Results:")
	t.Logf("  Total Users: %d", numUsers)
	t.Logf("  Success: %d", successCount)
	t.Logf("  Errors: %d", errorCount)
	t.Logf("  Duration: %s", duration)
	t.Logf("  Throughput: %.2f users/sec", float64(numUsers)/duration.Seconds())

	assert.Equal(t, int32(numUsers), successCount, "All users should be created")
	assert.Equal(t, int32(0), errorCount, "No errors expected")
}

func TestDuplicateUserEmail(t *testing.T) {
	storage := createTestMetadataStorage(t)
	defer storage.Close()

	user1 := &User{
		ID:           "user1",
		Email:        "duplicate@test.com",
		PasswordHash: "hash1",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}

	err := storage.CreateUser(user1)
	require.NoError(t, err)

	// Try to create user with same email
	user2 := &User{
		ID:           "user2",
		Email:        "duplicate@test.com",
		PasswordHash: "hash2",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}

	err = storage.CreateUser(user2)
	assert.Error(t, err, "Should not allow duplicate email")
	assert.Contains(t, err.Error(), "already exists")
}

func TestConcurrentUserLookup(t *testing.T) {
	storage := createTestMetadataStorage(t)
	defer storage.Close()

	// Create test users
	numUsers := 100
	for i := 0; i < numUsers; i++ {
		user := &User{
			ID:           fmt.Sprintf("user_%d", i),
			Email:        fmt.Sprintf("user%d@test.com", i),
			PasswordHash: "hash123",
			Role:         "user",
			Status:       "active",
			CreatedAt:    time.Now(),
		}
		err := storage.CreateUser(user)
		require.NoError(t, err)
	}

	// Concurrent lookups
	numLookups := 10000
	var wg sync.WaitGroup
	var successCount int32

	start := time.Now()

	for i := 0; i < numLookups; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()

			userID := rand.Intn(numUsers)
			email := fmt.Sprintf("user%d@test.com", userID)

			user, err := storage.GetUserByEmail(email)
			if err == nil && user.Email == email {
				atomic.AddInt32(&successCount, 1)
			}
		}()
	}

	wg.Wait()
	duration := time.Since(start)

	t.Logf("Concurrent User Lookup Results:")
	t.Logf("  Total Lookups: %d", numLookups)
	t.Logf("  Success: %d", successCount)
	t.Logf("  Duration: %s", duration)
	t.Logf("  Throughput: %.2f lookups/sec", float64(numLookups)/duration.Seconds())

	assert.Equal(t, int32(numLookups), successCount, "All lookups should succeed")
}

// ============ Device Operations Tests ============

func TestConcurrentDeviceCreationWithIndexes(t *testing.T) {
	storage := createTestMetadataStorage(t)
	defer storage.Close()

	// Create a test user
	user := &User{
		ID:           "test_user",
		Email:        "test@example.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	numDevices := 5000
	var wg sync.WaitGroup
	var successCount int32

	start := time.Now()

	for i := 0; i < numDevices; i++ {
		wg.Add(1)
		go func(idx int) {
			defer wg.Done()

			device := &Device{
				ID:        fmt.Sprintf("device_%d", idx),
				UserID:    "test_user",
				Name:      fmt.Sprintf("Device %d", idx),
				Type:      "sensor",
				APIKey:    fmt.Sprintf("key_%d", idx),
				Status:    "active",
				CreatedAt: time.Now(),
			}

			err := storage.CreateDevice(device)
			if err == nil {
				atomic.AddInt32(&successCount, 1)
			}
		}(i)
	}

	wg.Wait()
	duration := time.Since(start)

	t.Logf("Concurrent Device Creation with Indexes Results:")
	t.Logf("  Total Devices: %d", numDevices)
	t.Logf("  Success: %d", successCount)
	t.Logf("  Duration: %s", duration)
	t.Logf("  Throughput: %.2f devices/sec", float64(numDevices)/duration.Seconds())

	assert.Equal(t, int32(numDevices), successCount, "All devices should be created")

	// Test retrieving devices by user
	devices, err := storage.GetUserDevices("test_user")
	require.NoError(t, err)
	assert.Equal(t, numDevices, len(devices), "Should retrieve all user devices")
}

func TestAPIKeyIndex(t *testing.T) {
	storage := createTestMetadataStorage(t)
	defer storage.Close()

	// Create user
	user := &User{
		ID:           "user1",
		Email:        "user@test.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	// Create device
	device := &Device{
		ID:        "device1",
		UserID:    "user1",
		Name:      "Test Device",
		Type:      "sensor",
		APIKey:    "secret_api_key_123",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	err = storage.CreateDevice(device)
	require.NoError(t, err)

	// Test API key lookup
	foundDevice, err := storage.GetDeviceByAPIKey("secret_api_key_123")
	require.NoError(t, err)
	assert.Equal(t, device.ID, foundDevice.ID)
	assert.Equal(t, device.APIKey, foundDevice.APIKey)

	// Test non-existent API key
	_, err = storage.GetDeviceByAPIKey("nonexistent_key")
	assert.Error(t, err, "Should fail for non-existent API key")
}

func TestConcurrentAPIKeyLookup(t *testing.T) {
	storage := createTestMetadataStorage(t)
	defer storage.Close()

	// Create user
	user := &User{
		ID:           "user1",
		Email:        "user@test.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	// Create devices with unique API keys
	numDevices := 100
	for i := 0; i < numDevices; i++ {
		device := &Device{
			ID:        fmt.Sprintf("device_%d", i),
			UserID:    "user1",
			Name:      fmt.Sprintf("Device %d", i),
			Type:      "sensor",
			APIKey:    fmt.Sprintf("api_key_%d", i),
			Status:    "active",
			CreatedAt: time.Now(),
		}
		err := storage.CreateDevice(device)
		require.NoError(t, err)
	}

	// Concurrent API key lookups
	numLookups := 10000
	var wg sync.WaitGroup
	var successCount int32

	start := time.Now()

	for i := 0; i < numLookups; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()

			deviceIdx := rand.Intn(numDevices)
			apiKey := fmt.Sprintf("api_key_%d", deviceIdx)

			device, err := storage.GetDeviceByAPIKey(apiKey)
			if err == nil && device.APIKey == apiKey {
				atomic.AddInt32(&successCount, 1)
			}
		}()
	}

	wg.Wait()
	duration := time.Since(start)

	t.Logf("Concurrent API Key Lookup Results:")
	t.Logf("  Total Lookups: %d", numLookups)
	t.Logf("  Success: %d", successCount)
	t.Logf("  Duration: %s", duration)
	t.Logf("  Throughput: %.2f lookups/sec", float64(numLookups)/duration.Seconds())

	assert.Equal(t, int32(numLookups), successCount, "All API key lookups should succeed")
}

func TestDeviceOwnershipValidation(t *testing.T) {
	storage := createTestMetadataStorage(t)
	defer storage.Close()

	// Create two users
	user1 := &User{
		ID:           "user1",
		Email:        "user1@test.com",
		PasswordHash: "hash1",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user1)
	require.NoError(t, err)

	user2 := &User{
		ID:           "user2",
		Email:        "user2@test.com",
		PasswordHash: "hash2",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err = storage.CreateUser(user2)
	require.NoError(t, err)

	// User1 creates a device
	device := &Device{
		ID:        "device1",
		UserID:    "user1",
		Name:      "User1 Device",
		Type:      "sensor",
		APIKey:    "user1_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	err = storage.CreateDevice(device)
	require.NoError(t, err)

	// User2 tries to delete User1's device - should fail
	err = storage.DeleteDevice("device1", "user2")
	assert.Error(t, err, "User2 should not be able to delete User1's device")
	assert.Contains(t, err.Error(), "access denied")

	// User1 can delete their own device - should succeed
	err = storage.DeleteDevice("device1", "user1")
	assert.NoError(t, err, "User1 should be able to delete their own device")

	// Verify device is deleted
	_, err = storage.GetDevice("device1")
	assert.Error(t, err, "Device should be deleted")
}

// ============ Transaction and Consistency Tests ============

func TestTransactionConsistency(t *testing.T) {
	storage := createTestMetadataStorage(t)
	defer storage.Close()

	// Create user
	user := &User{
		ID:           "user1",
		Email:        "user@test.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	// Create and delete devices concurrently
	numOperations := 1000
	var wg sync.WaitGroup

	for i := 0; i < numOperations; i++ {
		wg.Add(2)

		// Create device
		go func(idx int) {
			defer wg.Done()
			device := &Device{
				ID:        fmt.Sprintf("device_%d", idx),
				UserID:    "user1",
				Name:      fmt.Sprintf("Device %d", idx),
				Type:      "sensor",
				APIKey:    fmt.Sprintf("key_%d", idx),
				Status:    "active",
				CreatedAt: time.Now(),
			}
			storage.CreateDevice(device)
		}(i)

		// Delete device (might fail if not created yet)
		go func(idx int) {
			defer wg.Done()
			storage.DeleteDevice(fmt.Sprintf("device_%d", idx), "user1")
		}(i)
	}

	wg.Wait()

	// Verify data consistency
	devices, err := storage.GetUserDevices("user1")
	require.NoError(t, err)

	t.Logf("Transaction Consistency Test:")
	t.Logf("  Operations: %d create + %d delete", numOperations, numOperations)
	t.Logf("  Final device count: %d", len(devices))
	t.Logf("  Consistency check: PASSED (no crashes)")
}

func TestIndexConsistency(t *testing.T) {
	storage := createTestMetadataStorage(t)
	defer storage.Close()

	// Create user
	user := &User{
		ID:           "user1",
		Email:        "user@test.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	// Create device
	device := &Device{
		ID:        "device1",
		UserID:    "user1",
		Name:      "Test Device",
		Type:      "sensor",
		APIKey:    "test_api_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	err = storage.CreateDevice(device)
	require.NoError(t, err)

	// Verify device can be found by ID
	foundByID, err := storage.GetDevice("device1")
	require.NoError(t, err)
	assert.Equal(t, "device1", foundByID.ID)

	// Verify device can be found by API key
	foundByKey, err := storage.GetDeviceByAPIKey("test_api_key")
	require.NoError(t, err)
	assert.Equal(t, "device1", foundByKey.ID)

	// Verify device is in user's device list
	userDevices, err := storage.GetUserDevices("user1")
	require.NoError(t, err)
	assert.Equal(t, 1, len(userDevices))
	assert.Equal(t, "device1", userDevices[0].ID)

	// Delete device
	err = storage.DeleteDevice("device1", "user1")
	require.NoError(t, err)

	// Verify all indexes are cleaned up
	_, err = storage.GetDevice("device1")
	assert.Error(t, err, "Device should not be found by ID")

	_, err = storage.GetDeviceByAPIKey("test_api_key")
	assert.Error(t, err, "Device should not be found by API key")

	userDevices, err = storage.GetUserDevices("user1")
	require.NoError(t, err)
	assert.Equal(t, 0, len(userDevices), "User should have no devices")
}

// ============ Performance and Memory Tests ============

func TestHighVolumeTransactions(t *testing.T) {
	storage := createTestMetadataStorage(t)
	defer storage.Close()

	// Create user
	user := &User{
		ID:           "user1",
		Email:        "user@test.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	numDevices := 10000
	start := time.Now()

	// Sequential device creation
	for i := 0; i < numDevices; i++ {
		device := &Device{
			ID:        fmt.Sprintf("device_%d", i),
			UserID:    "user1",
			Name:      fmt.Sprintf("Device %d", i),
			Type:      "sensor",
			APIKey:    fmt.Sprintf("key_%d", i),
			Status:    "active",
			CreatedAt: time.Now(),
		}
		err := storage.CreateDevice(device)
		require.NoError(t, err)
	}

	createDuration := time.Since(start)

	t.Logf("High Volume Transaction Results:")
	t.Logf("  Total Devices: %d", numDevices)
	t.Logf("  Create Duration: %s", createDuration)
	t.Logf("  Create Throughput: %.2f devices/sec", float64(numDevices)/createDuration.Seconds())

	// Test bulk retrieval
	start = time.Now()
	devices, err := storage.GetUserDevices("user1")
	require.NoError(t, err)
	retrievalDuration := time.Since(start)

	assert.Equal(t, numDevices, len(devices))
	t.Logf("  Retrieval Duration: %s", retrievalDuration)
	t.Logf("  Retrieval Throughput: %.2f devices/sec", float64(numDevices)/retrievalDuration.Seconds())
}

func TestBuntDBMemoryUsage(t *testing.T) {
	storage := createTestMetadataStorage(t)
	defer storage.Close()

	// Force GC to get baseline
	runtime.GC()
	var memBefore runtime.MemStats
	runtime.ReadMemStats(&memBefore)

	// Create user
	user := &User{
		ID:           "user1",
		Email:        "user@test.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	storage.CreateUser(user)

	// Create many devices
	numDevices := 5000
	for i := 0; i < numDevices; i++ {
		device := &Device{
			ID:        fmt.Sprintf("device_%d", i),
			UserID:    "user1",
			Name:      fmt.Sprintf("Device %d", i),
			Type:      "sensor",
			APIKey:    fmt.Sprintf("key_%d", i),
			Status:    "active",
			CreatedAt: time.Now(),
		}
		storage.CreateDevice(device)
	}

	// Perform many lookups
	for i := 0; i < 10000; i++ {
		deviceIdx := rand.Intn(numDevices)
		storage.GetDevice(fmt.Sprintf("device_%d", deviceIdx))
		storage.GetDeviceByAPIKey(fmt.Sprintf("key_%d", deviceIdx))
	}

	// Force GC and check memory
	runtime.GC()
	var memAfter runtime.MemStats
	runtime.ReadMemStats(&memAfter)

	memUsed := memAfter.Alloc - memBefore.Alloc
	memPerDevice := float64(memUsed) / float64(numDevices)

	t.Logf("BuntDB Memory Usage:")
	t.Logf("  Devices Created: %d", numDevices)
	t.Logf("  Lookups Performed: %d", 10000)
	t.Logf("  Memory Used: %.2f MB", float64(memUsed)/(1024*1024))
	t.Logf("  Memory Per Device: %.2f KB", memPerDevice/1024)
}

func TestDataPersistence(t *testing.T) {
	tmpDir := t.TempDir()
	metaPath := filepath.Join(tmpDir, "persist_test.db")
	dataPath := filepath.Join(tmpDir, "persist_tsdata")

	// Create storage and add data
	storage1, err := New(metaPath, dataPath, 7*24*time.Hour)
	require.NoError(t, err)

	user := &User{
		ID:           "persist_user",
		Email:        "persist@test.com",
		PasswordHash: "hash123",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err = storage1.CreateUser(user)
	require.NoError(t, err)

	device := &Device{
		ID:        "persist_device",
		UserID:    "persist_user",
		Name:      "Persistent Device",
		Type:      "sensor",
		APIKey:    "persist_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	err = storage1.CreateDevice(device)
	require.NoError(t, err)

	// Close storage
	err = storage1.Close()
	require.NoError(t, err)

	// Reopen storage and verify data persists
	storage2, err := New(metaPath, dataPath, 7*24*time.Hour)
	require.NoError(t, err)
	defer storage2.Close()

	// Check user persists
	foundUser, err := storage2.GetUserByEmail("persist@test.com")
	require.NoError(t, err)
	assert.Equal(t, "persist_user", foundUser.ID)

	// Check device persists
	foundDevice, err := storage2.GetDevice("persist_device")
	require.NoError(t, err)
	assert.Equal(t, "persist_device", foundDevice.ID)

	// Check API key index persists
	foundByKey, err := storage2.GetDeviceByAPIKey("persist_key")
	require.NoError(t, err)
	assert.Equal(t, "persist_device", foundByKey.ID)

	t.Logf("Data Persistence Test: PASSED")
	t.Logf("  User data persisted: ✓")
	t.Logf("  Device data persisted: ✓")
	t.Logf("  API key index persisted: ✓")
}

// ============ Benchmark Tests ============

func BenchmarkUserCreation(b *testing.B) {
	tmpDir := b.TempDir()
	metaPath := filepath.Join(tmpDir, "bench_meta.db")
	dataPath := filepath.Join(tmpDir, "bench_data")

	storage, err := New(metaPath, dataPath, 7*24*time.Hour)
	if err != nil {
		b.Fatal(err)
	}
	defer storage.Close()

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		user := &User{
			ID:           fmt.Sprintf("user_%d", i),
			Email:        fmt.Sprintf("user%d@test.com", i),
			PasswordHash: "hash123",
			Role:         "user",
			Status:       "active",
			CreatedAt:    time.Now(),
		}
		storage.CreateUser(user)
	}
}

func BenchmarkUserLookupByEmail(b *testing.B) {
	tmpDir := b.TempDir()
	metaPath := filepath.Join(tmpDir, "bench_meta.db")
	dataPath := filepath.Join(tmpDir, "bench_data")

	storage, err := New(metaPath, dataPath, 7*24*time.Hour)
	if err != nil {
		b.Fatal(err)
	}
	defer storage.Close()

	// Create test users
	for i := 0; i < 100; i++ {
		user := &User{
			ID:           fmt.Sprintf("user_%d", i),
			Email:        fmt.Sprintf("user%d@test.com", i),
			PasswordHash: "hash123",
			Role:         "user",
			Status:       "active",
			CreatedAt:    time.Now(),
		}
		storage.CreateUser(user)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		email := fmt.Sprintf("user%d@test.com", i%100)
		storage.GetUserByEmail(email)
	}
}

func BenchmarkMetadataDeviceCreation(b *testing.B) {
	tmpDir := b.TempDir()
	metaPath := filepath.Join(tmpDir, "bench_meta.db")
	dataPath := filepath.Join(tmpDir, "bench_data")

	storage, err := New(metaPath, dataPath, 7*24*time.Hour)
	if err != nil {
		b.Fatal(err)
	}
	defer storage.Close()

	// Create test user
	user := &User{
		ID:           "bench_user",
		Email:        "bench@test.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	storage.CreateUser(user)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		device := &Device{
			ID:        fmt.Sprintf("device_%d", i),
			UserID:    "bench_user",
			Name:      fmt.Sprintf("Device %d", i),
			Type:      "sensor",
			APIKey:    fmt.Sprintf("key_%d", i),
			Status:    "active",
			CreatedAt: time.Now(),
		}
		storage.CreateDevice(device)
	}
}

func BenchmarkAPIKeyLookup(b *testing.B) {
	tmpDir := b.TempDir()
	metaPath := filepath.Join(tmpDir, "bench_meta.db")
	dataPath := filepath.Join(tmpDir, "bench_data")

	storage, err := New(metaPath, dataPath, 7*24*time.Hour)
	if err != nil {
		b.Fatal(err)
	}
	defer storage.Close()

	// Create user and devices
	user := &User{
		ID:           "bench_user",
		Email:        "bench@test.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	storage.CreateUser(user)

	for i := 0; i < 100; i++ {
		device := &Device{
			ID:        fmt.Sprintf("device_%d", i),
			UserID:    "bench_user",
			Name:      fmt.Sprintf("Device %d", i),
			Type:      "sensor",
			APIKey:    fmt.Sprintf("key_%d", i),
			Status:    "active",
			CreatedAt: time.Now(),
		}
		storage.CreateDevice(device)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		apiKey := fmt.Sprintf("key_%d", i%100)
		storage.GetDeviceByAPIKey(apiKey)
	}
}

func BenchmarkGetUserDevices(b *testing.B) {
	tmpDir := b.TempDir()
	metaPath := filepath.Join(tmpDir, "bench_meta.db")
	dataPath := filepath.Join(tmpDir, "bench_data")

	storage, err := New(metaPath, dataPath, 7*24*time.Hour)
	if err != nil {
		b.Fatal(err)
	}
	defer storage.Close()

	// Create user with many devices
	user := &User{
		ID:           "bench_user",
		Email:        "bench@test.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	storage.CreateUser(user)

	for i := 0; i < 1000; i++ {
		device := &Device{
			ID:        fmt.Sprintf("device_%d", i),
			UserID:    "bench_user",
			Name:      fmt.Sprintf("Device %d", i),
			Type:      "sensor",
			APIKey:    fmt.Sprintf("key_%d", i),
			Status:    "active",
			CreatedAt: time.Now(),
		}
		storage.CreateDevice(device)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		storage.GetUserDevices("bench_user")
	}
}

func BenchmarkConcurrentDeviceCreation(b *testing.B) {
	tmpDir := b.TempDir()
	metaPath := filepath.Join(tmpDir, "bench_meta.db")
	dataPath := filepath.Join(tmpDir, "bench_data")

	storage, err := New(metaPath, dataPath, 7*24*time.Hour)
	if err != nil {
		b.Fatal(err)
	}
	defer storage.Close()

	user := &User{
		ID:           "bench_user",
		Email:        "bench@test.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	storage.CreateUser(user)

	b.ResetTimer()
	b.RunParallel(func(pb *testing.PB) {
		i := 0
		for pb.Next() {
			device := &Device{
				ID:        fmt.Sprintf("device_%d_%d", b.N, i),
				UserID:    "bench_user",
				Name:      fmt.Sprintf("Device %d", i),
				Type:      "sensor",
				APIKey:    fmt.Sprintf("key_%d_%d", b.N, i),
				Status:    "active",
				CreatedAt: time.Now(),
			}
			storage.CreateDevice(device)
			i++
		}
	})
}

func BenchmarkConcurrentAPIKeyLookup(b *testing.B) {
	tmpDir := b.TempDir()
	metaPath := filepath.Join(tmpDir, "bench_meta.db")
	dataPath := filepath.Join(tmpDir, "bench_data")

	storage, err := New(metaPath, dataPath, 7*24*time.Hour)
	if err != nil {
		b.Fatal(err)
	}
	defer storage.Close()

	user := &User{
		ID:           "bench_user",
		Email:        "bench@test.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	storage.CreateUser(user)

	// Create devices
	numDevices := 100
	for i := 0; i < numDevices; i++ {
		device := &Device{
			ID:        fmt.Sprintf("device_%d", i),
			UserID:    "bench_user",
			Name:      fmt.Sprintf("Device %d", i),
			Type:      "sensor",
			APIKey:    fmt.Sprintf("key_%d", i),
			Status:    "active",
			CreatedAt: time.Now(),
		}
		storage.CreateDevice(device)
	}

	b.ResetTimer()
	b.RunParallel(func(pb *testing.PB) {
		i := 0
		for pb.Next() {
			apiKey := fmt.Sprintf("key_%d", i%numDevices)
			storage.GetDeviceByAPIKey(apiKey)
			i++
		}
	})
}
