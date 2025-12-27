package storage

import (
	"path/filepath"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// Helper to create test storage for system tests
func createTestStorageForSystem(t *testing.T) *Storage {
	tmpDir := t.TempDir()
	metaPath := filepath.Join(tmpDir, "system_test.db")
	dataPath := filepath.Join(tmpDir, "system_data")

	storage, err := New(metaPath, dataPath)
	require.NoError(t, err, "Failed to create test storage")

	return storage
}

// ============ System Config Tests ============

func TestGetSystemConfigDefault(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	config, err := storage.GetSystemConfig()
	require.NoError(t, err)

	// Default config should be uninitialized
	assert.False(t, config.Initialized, "Default config should not be initialized")
	assert.Empty(t, config.PlatformName, "Default platform name should be empty")
	assert.False(t, config.AllowRegister, "Default allow register should be false")
	assert.Equal(t, 0, config.DataRetention, "Default retention should be 0")
}

func TestSaveAndGetSystemConfig(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	// Create and save config
	now := time.Now()
	config := &SystemConfig{
		Initialized:   true,
		SetupAt:       now,
		PlatformName:  "Test IoT Platform",
		AllowRegister: true,
		DataRetention: 30,
	}

	err := storage.SaveSystemConfig(config)
	require.NoError(t, err)

	// Retrieve and verify
	retrieved, err := storage.GetSystemConfig()
	require.NoError(t, err)

	assert.True(t, retrieved.Initialized)
	assert.Equal(t, "Test IoT Platform", retrieved.PlatformName)
	assert.True(t, retrieved.AllowRegister)
	assert.Equal(t, 30, retrieved.DataRetention)
	assert.WithinDuration(t, now, retrieved.SetupAt, time.Second)
}

func TestUpdateSystemConfig(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	// Initial config
	config := &SystemConfig{
		Initialized:   true,
		PlatformName:  "Initial Name",
		AllowRegister: false,
		DataRetention: 7,
	}
	storage.SaveSystemConfig(config)

	// Update config
	config.PlatformName = "Updated Name"
	config.AllowRegister = true
	config.DataRetention = 14
	err := storage.SaveSystemConfig(config)
	require.NoError(t, err)

	// Verify updates
	retrieved, err := storage.GetSystemConfig()
	require.NoError(t, err)

	assert.Equal(t, "Updated Name", retrieved.PlatformName)
	assert.True(t, retrieved.AllowRegister)
	assert.Equal(t, 14, retrieved.DataRetention)
}

// ============ System Initialization Tests ============

func TestIsSystemInitialized(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	// Initially should not be initialized
	assert.False(t, storage.IsSystemInitialized(), "System should not be initialized initially")

	// Initialize system
	err := storage.InitializeSystem("Test Platform", true, 7)
	require.NoError(t, err)

	// Now should be initialized
	assert.True(t, storage.IsSystemInitialized(), "System should be initialized after setup")
}

func TestInitializeSystem(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	platformName := "DatumPy IoT Platform"
	allowRegister := true
	retention := 30

	err := storage.InitializeSystem(platformName, allowRegister, retention)
	require.NoError(t, err)

	// Verify initialization
	config, err := storage.GetSystemConfig()
	require.NoError(t, err)

	assert.True(t, config.Initialized)
	assert.Equal(t, platformName, config.PlatformName)
	assert.Equal(t, allowRegister, config.AllowRegister)
	assert.Equal(t, retention, config.DataRetention)
	assert.WithinDuration(t, time.Now(), config.SetupAt, 2*time.Second)
}

func TestInitializeSystemMultipleTimes(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	// First initialization
	err := storage.InitializeSystem("Platform V1", false, 7)
	require.NoError(t, err)

	// Second initialization (should overwrite)
	err = storage.InitializeSystem("Platform V2", true, 14)
	require.NoError(t, err)

	// Verify latest config
	config, err := storage.GetSystemConfig()
	require.NoError(t, err)

	assert.Equal(t, "Platform V2", config.PlatformName)
	assert.True(t, config.AllowRegister)
	assert.Equal(t, 14, config.DataRetention)
}

// ============ Data Retention Update Tests ============

func TestUpdateDataRetention(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	// Initialize system first
	storage.InitializeSystem("Test Platform", true, 7)

	// Update retention
	err := storage.UpdateDataRetention(30)
	require.NoError(t, err)

	// Verify update
	config, err := storage.GetSystemConfig()
	require.NoError(t, err)
	assert.Equal(t, 30, config.DataRetention)

	// Other fields should remain unchanged
	assert.True(t, config.Initialized)
	assert.Equal(t, "Test Platform", config.PlatformName)
	assert.True(t, config.AllowRegister)
}

func TestUpdateDataRetentionBeforeInit(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	// Try to update retention before initialization
	// Should work but create default config
	err := storage.UpdateDataRetention(14)
	require.NoError(t, err)

	config, err := storage.GetSystemConfig()
	require.NoError(t, err)
	assert.Equal(t, 14, config.DataRetention)
}

func TestUpdateDataRetentionMultipleTimes(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	storage.InitializeSystem("Test Platform", true, 7)

	// Update multiple times
	retentionValues := []int{14, 30, 60, 90}
	for _, retention := range retentionValues {
		err := storage.UpdateDataRetention(retention)
		require.NoError(t, err)

		config, err := storage.GetSystemConfig()
		require.NoError(t, err)
		assert.Equal(t, retention, config.DataRetention)
	}
}

// ============ System Reset Tests ============

func TestResetSystem(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	// Initialize system and add data
	storage.InitializeSystem("Test Platform", true, 7)

	// Add users
	user := &User{
		ID:           "user1",
		Email:        "user@test.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	storage.CreateUser(user)

	// Add devices
	device := &Device{
		ID:        "device1",
		UserID:    "user1",
		Name:      "Test Device",
		Type:      "sensor",
		APIKey:    "test_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	// Verify data exists
	assert.True(t, storage.IsSystemInitialized())
	users, _ := storage.ListAllUsers()
	assert.Equal(t, 1, len(users))
	devices, _ := storage.ListAllDevices()
	assert.Equal(t, 1, len(devices))

	// Reset system
	err := storage.ResetSystem()
	require.NoError(t, err)

	// Verify everything is gone
	assert.False(t, storage.IsSystemInitialized(), "System should not be initialized after reset")

	users, _ = storage.ListAllUsers()
	assert.Equal(t, 0, len(users), "All users should be deleted")

	devices, _ = storage.ListAllDevices()
	assert.Equal(t, 0, len(devices), "All devices should be deleted")
}

func TestResetEmptySystem(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	// Reset empty system (should not error)
	err := storage.ResetSystem()
	require.NoError(t, err)

	// Verify system is still uninitialized
	assert.False(t, storage.IsSystemInitialized())
}

func TestResetAndReinitialize(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	// Initialize
	storage.InitializeSystem("Platform V1", false, 7)
	assert.True(t, storage.IsSystemInitialized())

	// Reset
	storage.ResetSystem()
	assert.False(t, storage.IsSystemInitialized())

	// Re-initialize with different settings
	err := storage.InitializeSystem("Platform V2", true, 30)
	require.NoError(t, err)

	config, err := storage.GetSystemConfig()
	require.NoError(t, err)
	assert.True(t, config.Initialized)
	assert.Equal(t, "Platform V2", config.PlatformName)
	assert.True(t, config.AllowRegister)
	assert.Equal(t, 30, config.DataRetention)
}

// ============ Database Export Tests ============

func TestExportDatabaseEmpty(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	export, err := storage.ExportDatabase()
	require.NoError(t, err)

	assert.NotNil(t, export)
	assert.Contains(t, export, "config")
	assert.Contains(t, export, "users")
	assert.Contains(t, export, "devices")
	assert.Contains(t, export, "exported_at")

	// Verify empty collections
	users := export["users"].([]User)
	assert.Equal(t, 0, len(users))

	devices := export["devices"].([]Device)
	assert.Equal(t, 0, len(devices))
}

func TestExportDatabaseWithData(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	// Initialize system
	storage.InitializeSystem("Export Test Platform", true, 14)

	// Add users
	for i := 0; i < 3; i++ {
		user := &User{
			ID:           "user" + string(rune('1'+i)),
			Email:        "user" + string(rune('1'+i)) + "@test.com",
			PasswordHash: "hash",
			Role:         "user",
			Status:       "active",
			CreatedAt:    time.Now(),
		}
		storage.CreateUser(user)
	}

	// Add devices
	for i := 0; i < 5; i++ {
		device := &Device{
			ID:        "device" + string(rune('1'+i)),
			UserID:    "user1",
			Name:      "Device " + string(rune('1'+i)),
			Type:      "sensor",
			APIKey:    "key" + string(rune('1'+i)),
			Status:    "active",
			CreatedAt: time.Now(),
		}
		storage.CreateDevice(device)
	}

	// Export
	export, err := storage.ExportDatabase()
	require.NoError(t, err)

	// Verify config
	config := export["config"].(*SystemConfig)
	assert.True(t, config.Initialized)
	assert.Equal(t, "Export Test Platform", config.PlatformName)

	// Verify users
	users := export["users"].([]User)
	assert.Equal(t, 3, len(users))

	// Verify devices
	devices := export["devices"].([]Device)
	assert.Equal(t, 5, len(devices))

	// Verify timestamp
	exportedAt := export["exported_at"].(time.Time)
	assert.WithinDuration(t, time.Now(), exportedAt, 2*time.Second)
}

func TestSystemLifecycle(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	// 1. Fresh system (uninitialized)
	assert.False(t, storage.IsSystemInitialized())

	// 2. Initialize system
	err := storage.InitializeSystem("Production Platform", false, 30)
	require.NoError(t, err)
	assert.True(t, storage.IsSystemInitialized())

	// 3. Update retention policy
	err = storage.UpdateDataRetention(60)
	require.NoError(t, err)

	config, _ := storage.GetSystemConfig()
	assert.Equal(t, 60, config.DataRetention)

	// 4. Add some data
	user := &User{
		ID:           "admin",
		Email:        "admin@platform.com",
		PasswordHash: "hash",
		Role:         "admin",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	storage.CreateUser(user)

	device := &Device{
		ID:        "sensor1",
		UserID:    "admin",
		Name:      "Temperature Sensor",
		Type:      "sensor",
		APIKey:    "secret_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	// 5. Verify data exists
	users, _ := storage.ListAllUsers()
	assert.Equal(t, 1, len(users))
	assert.Equal(t, "admin", users[0].ID)

	devices, _ := storage.ListAllDevices()
	assert.Equal(t, 1, len(devices))
	assert.Equal(t, "sensor1", devices[0].ID)

	// 6. Reset system
	err = storage.ResetSystem()
	require.NoError(t, err)
	assert.False(t, storage.IsSystemInitialized())

	// 7. Verify cleanup
	users, _ = storage.ListAllUsers()
	assert.Equal(t, 0, len(users))
	devices, _ = storage.ListAllDevices()
	assert.Equal(t, 0, len(devices))
}

func TestConcurrentConfigReads(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	// Initialize
	storage.InitializeSystem("Concurrent Test", true, 7)

	// Multiple concurrent reads
	done := make(chan bool)
	for i := 0; i < 100; i++ {
		go func() {
			config, err := storage.GetSystemConfig()
			assert.NoError(t, err)
			assert.True(t, config.Initialized)
			assert.Equal(t, "Concurrent Test", config.PlatformName)
			done <- true
		}()
	}

	// Wait for all goroutines
	for i := 0; i < 100; i++ {
		<-done
	}
}

func TestConcurrentConfigUpdates(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	storage.InitializeSystem("Update Test", true, 7)

	// Multiple concurrent retention updates
	done := make(chan bool)
	for i := 0; i < 50; i++ {
		go func(retention int) {
			err := storage.UpdateDataRetention(retention)
			assert.NoError(t, err)
			done <- true
		}(i + 1)
	}

	// Wait for all updates
	for i := 0; i < 50; i++ {
		<-done
	}

	// Verify final state (should be one of the values)
	config, err := storage.GetSystemConfig()
	require.NoError(t, err)
	assert.Greater(t, config.DataRetention, 0)
	assert.LessOrEqual(t, config.DataRetention, 50)
}

// ============ Edge Case Tests ============

func TestSystemConfigWithEmptyValues(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	// Initialize with empty/zero values
	err := storage.InitializeSystem("", false, 0)
	require.NoError(t, err)

	config, err := storage.GetSystemConfig()
	require.NoError(t, err)

	assert.True(t, config.Initialized)
	assert.Empty(t, config.PlatformName)
	assert.False(t, config.AllowRegister)
	assert.Equal(t, 0, config.DataRetention)
}

func TestSystemConfigWithExtremeValues(t *testing.T) {
	storage := createTestStorageForSystem(t)
	defer storage.Close()

	// Very long platform name
	longName := string(make([]byte, 1000))
	for i := range longName {
		longName = longName[:i] + "A" + longName[i+1:]
	}

	err := storage.InitializeSystem(longName, true, 36500) // 100 years retention
	require.NoError(t, err)

	config, err := storage.GetSystemConfig()
	require.NoError(t, err)
	assert.Equal(t, longName, config.PlatformName)
	assert.Equal(t, 36500, config.DataRetention)
}

func TestSystemConfigPersistence(t *testing.T) {
	tmpDir := t.TempDir()
	metaPath := filepath.Join(tmpDir, "persist_test.db")
	dataPath := filepath.Join(tmpDir, "persist_data")

	// Create storage and initialize
	storage1, err := New(metaPath, dataPath)
	require.NoError(t, err)

	err = storage1.InitializeSystem("Persistent Platform", true, 30)
	require.NoError(t, err)

	setupTime := time.Now()
	storage1.Close()

	// Reopen storage
	storage2, err := New(metaPath, dataPath)
	require.NoError(t, err)
	defer storage2.Close()

	// Verify config persisted
	assert.True(t, storage2.IsSystemInitialized())

	config, err := storage2.GetSystemConfig()
	require.NoError(t, err)
	assert.Equal(t, "Persistent Platform", config.PlatformName)
	assert.True(t, config.AllowRegister)
	assert.Equal(t, 30, config.DataRetention)
	assert.WithinDuration(t, setupTime, config.SetupAt, 2*time.Second)
}

// ============ Benchmark Tests ============

func BenchmarkGetSystemConfig(b *testing.B) {
	tmpDir := b.TempDir()
	metaPath := filepath.Join(tmpDir, "bench_meta.db")
	dataPath := filepath.Join(tmpDir, "bench_data")

	storage, err := New(metaPath, dataPath)
	if err != nil {
		b.Fatal(err)
	}
	defer storage.Close()

	storage.InitializeSystem("Bench Platform", true, 7)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		storage.GetSystemConfig()
	}
}

func BenchmarkSaveSystemConfig(b *testing.B) {
	tmpDir := b.TempDir()
	metaPath := filepath.Join(tmpDir, "bench_meta.db")
	dataPath := filepath.Join(tmpDir, "bench_data")

	storage, err := New(metaPath, dataPath)
	if err != nil {
		b.Fatal(err)
	}
	defer storage.Close()

	config := &SystemConfig{
		Initialized:   true,
		SetupAt:       time.Now(),
		PlatformName:  "Benchmark Platform",
		AllowRegister: true,
		DataRetention: 30,
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		storage.SaveSystemConfig(config)
	}
}

func BenchmarkUpdateDataRetention(b *testing.B) {
	tmpDir := b.TempDir()
	metaPath := filepath.Join(tmpDir, "bench_meta.db")
	dataPath := filepath.Join(tmpDir, "bench_data")

	storage, err := New(metaPath, dataPath)
	if err != nil {
		b.Fatal(err)
	}
	defer storage.Close()

	storage.InitializeSystem("Bench Platform", true, 7)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		storage.UpdateDataRetention(i % 365)
	}
}

func BenchmarkExportDatabase(b *testing.B) {
	tmpDir := b.TempDir()
	metaPath := filepath.Join(tmpDir, "bench_meta.db")
	dataPath := filepath.Join(tmpDir, "bench_data")

	storage, err := New(metaPath, dataPath)
	if err != nil {
		b.Fatal(err)
	}
	defer storage.Close()

	storage.InitializeSystem("Bench Platform", true, 7)

	// Add some test data
	for i := 0; i < 100; i++ {
		user := &User{
			ID:           "user" + string(rune(i)),
			Email:        "user" + string(rune(i)) + "@test.com",
			PasswordHash: "hash",
			Role:         "user",
			Status:       "active",
			CreatedAt:    time.Now(),
		}
		storage.CreateUser(user)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		storage.ExportDatabase()
	}
}
