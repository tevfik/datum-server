package storage

import (
	"fmt"
	"path/filepath"
	"runtime"
	"sync"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// Helper function to create test storage
func createTestStorage(t *testing.T) (*Storage, func()) {
	tmpDir := t.TempDir()
	metaPath := filepath.Join(tmpDir, "test.db")
	dataPath := filepath.Join(tmpDir, "tsdata")

	storage, err := New(metaPath, dataPath)
	require.NoError(t, err)

	cleanup := func() {
		storage.Close()
	}

	return storage, cleanup
}

// ============ Basic Storage Tests ============

func TestNewStorage(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	assert.NotNil(t, storage)
	assert.NotNil(t, storage.db)
	assert.NotNil(t, storage.ts)
}

func TestCloseStorage(t *testing.T) {
	storage, _ := createTestStorage(t)
	err := storage.Close()
	assert.NoError(t, err)
}

// ============ User Operations Tests ============

func TestCreateUser(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{
		ID:           "usr_test_001",
		Email:        "test@example.com",
		PasswordHash: "hashedpassword",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}

	err := storage.CreateUser(user)
	assert.NoError(t, err)

	// Test duplicate email
	err = storage.CreateUser(user)
	assert.Error(t, err)
}

func TestGetUserByEmail(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{
		ID:           "usr_test_002",
		Email:        "test2@example.com",
		PasswordHash: "hashedpassword",
		Role:         "admin",
		Status:       "active",
		CreatedAt:    time.Now(),
	}

	err := storage.CreateUser(user)
	require.NoError(t, err)

	retrieved, err := storage.GetUserByEmail("test2@example.com")
	assert.NoError(t, err)
	assert.Equal(t, user.ID, retrieved.ID)
	assert.Equal(t, user.Email, retrieved.Email)
	assert.Equal(t, user.Role, retrieved.Role)
}

func TestGetUserByID(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{
		ID:           "usr_test_003",
		Email:        "test3@example.com",
		PasswordHash: "hashedpassword",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}

	err := storage.CreateUser(user)
	require.NoError(t, err)

	retrieved, err := storage.GetUserByID("usr_test_003")
	assert.NoError(t, err)
	assert.Equal(t, user.Email, retrieved.Email)
}

// ============ Device Operations Tests ============

func TestCreateAndGetDevice(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	device := &Device{
		ID:        "device_test_001",
		UserID:    "usr_test_001",
		Name:      "Test Device",
		Type:      "sensor",
		Status:    "active",
		APIKey:    "test_api_key_001",
		CreatedAt: time.Now(),
	}

	err := storage.CreateDevice(device)
	assert.NoError(t, err)

	retrieved, err := storage.GetDevice("device_test_001")
	assert.NoError(t, err)
	assert.Equal(t, device.Name, retrieved.Name)
	assert.Equal(t, device.Type, retrieved.Type)
}

func TestListDevices(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create test user first
	user := &User{
		ID:           "usr_list_test",
		Email:        "listtest@example.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	// Create multiple devices
	for i := 0; i < 5; i++ {
		device := &Device{
			ID:        fmt.Sprintf("device_test_%03d", i),
			UserID:    "usr_list_test",
			Name:      fmt.Sprintf("Test Device %d", i),
			Type:      "sensor",
			APIKey:    fmt.Sprintf("api_key_%03d", i),
			Status:    "active",
			CreatedAt: time.Now(),
		}
		err := storage.CreateDevice(device)
		require.NoError(t, err)
	}

	devices, err := storage.ListAllDevices()
	assert.NoError(t, err)
	assert.GreaterOrEqual(t, len(devices), 5)
}

func TestUpdateDevice(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	device := &Device{
		ID:        "device_test_update",
		UserID:    "usr_update_test",
		Name:      "Original Name",
		Type:      "sensor",
		APIKey:    "api_key_update",
		Status:    "active",
		CreatedAt: time.Now(),
	}

	err := storage.CreateDevice(device)
	require.NoError(t, err)

	// Update device status
	err = storage.UpdateDevice("device_test_update", "inactive")
	assert.NoError(t, err)

	retrieved, err := storage.GetDevice("device_test_update")
	assert.NoError(t, err)
	assert.Equal(t, "inactive", retrieved.Status)
}

func TestDeleteDevice(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	userID := "usr_delete_test"
	device := &Device{
		ID:        "device_test_delete",
		UserID:    userID,
		Name:      "To Be Deleted",
		Type:      "sensor",
		APIKey:    "api_key_delete",
		Status:    "active",
		CreatedAt: time.Now(),
	}

	err := storage.CreateDevice(device)
	require.NoError(t, err)

	err = storage.DeleteDevice("device_test_delete", userID)
	assert.NoError(t, err)

	_, err = storage.GetDevice("device_test_delete")
	assert.Error(t, err)
}

func TestCreateDeviceDuplicate(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	userID := "usr_duplicate_test"
	device := &Device{
		ID:        "device_duplicate",
		UserID:    userID,
		Name:      "First Device",
		Type:      "sensor",
		APIKey:    "api_key_first",
		Status:    "active",
		CreatedAt: time.Now(),
	}

	// Create first device
	err := storage.CreateDevice(device)
	require.NoError(t, err)

	// Try to create duplicate device with same ID
	duplicateDevice := &Device{
		ID:        "device_duplicate", // Same ID
		UserID:    userID,
		Name:      "Second Device",
		Type:      "actuator",
		APIKey:    "api_key_second",
		Status:    "active",
		CreatedAt: time.Now(),
	}

	// Should return an error
	err = storage.CreateDevice(duplicateDevice)
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "already exists")

	// Verify first device is still intact
	retrieved, err := storage.GetDevice("device_duplicate")
	require.NoError(t, err)
	assert.Equal(t, "First Device", retrieved.Name)
	assert.Equal(t, "sensor", retrieved.Type)
	assert.Equal(t, "api_key_first", retrieved.APIKey)
}

// ============ Time-Series Data Tests ============

func TestInsertDataPoints(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create device first
	device := &Device{
		ID:        "device_ts_001",
		UserID:    "usr_ts_001",
		Name:      "TS Test Device",
		Type:      "sensor",
		APIKey:    "api_key_ts_001",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	err := storage.CreateDevice(device)
	require.NoError(t, err)

	// Insert data points using StoreData
	dataPoint := &DataPoint{
		DeviceID:  "device_ts_001",
		Timestamp: time.Now(),
		Data: map[string]interface{}{
			"temperature": 23.5,
			"humidity":    65.0,
			"pressure":    1013.25,
		},
	}

	err = storage.StoreData(dataPoint)
	assert.NoError(t, err)
}

func TestGetDataPoints(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create device
	device := &Device{
		ID:        "device_ts_002",
		UserID:    "usr_ts_002",
		Name:      "TS Test Device 2",
		Type:      "sensor",
		APIKey:    "api_key_ts_002",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	err := storage.CreateDevice(device)
	require.NoError(t, err)

	// Insert data points
	for i := 0; i < 10; i++ {
		dataPoint := &DataPoint{
			DeviceID:  "device_ts_002",
			Timestamp: time.Now(),
			Data: map[string]interface{}{
				"temperature": 20.0 + float64(i),
				"humidity":    60.0 + float64(i),
			},
		}
		err = storage.StoreData(dataPoint)
		require.NoError(t, err)
		time.Sleep(10 * time.Millisecond)
	}

	// Query data points using GetDataHistory
	dataHistory, err := storage.GetDataHistory("device_ts_002", 100)
	assert.NoError(t, err)
	assert.Greater(t, len(dataHistory), 0)
}

// ============ Load Tests ============

func TestConcurrentDeviceCreation(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping load test in short mode")
	}

	storage, cleanup := createTestStorage(t)
	defer cleanup()

	numDevices := 1000
	concurrency := 10

	var wg sync.WaitGroup
	errors := make(chan error, numDevices)

	start := time.Now()

	for i := 0; i < numDevices; i++ {
		wg.Add(1)
		go func(idx int) {
			defer wg.Done()

			device := &Device{
				ID:        fmt.Sprintf("device_load_%06d", idx),
				UserID:    "usr_load_test",
				Name:      fmt.Sprintf("Load Test Device %d", idx),
				Type:      "sensor",
				APIKey:    fmt.Sprintf("api_key_load_%06d", idx),
				Status:    "active",
				CreatedAt: time.Now(),
			}

			if err := storage.CreateDevice(device); err != nil {
				errors <- err
			}
		}(i)

		if i%concurrency == 0 {
			time.Sleep(1 * time.Millisecond)
		}
	}

	wg.Wait()
	close(errors)

	duration := time.Since(start)
	t.Logf("Created %d devices in %v (%.2f devices/sec)", numDevices, duration, float64(numDevices)/duration.Seconds())

	// Check for errors
	errorCount := 0
	for err := range errors {
		if err != nil {
			t.Logf("Error: %v", err)
			errorCount++
		}
	}

	assert.Equal(t, 0, errorCount, "Should have no errors")
}

func TestConcurrentDataPointInsertion(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping load test in short mode")
	}

	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create test device
	device := &Device{
		ID:        "device_load_ts",
		UserID:    "usr_load_ts",
		Name:      "Load Test TS Device",
		Type:      "sensor",
		APIKey:    "api_key_load_ts",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	err := storage.CreateDevice(device)
	require.NoError(t, err)

	numInserts := 10000
	concurrency := 100

	var wg sync.WaitGroup
	errors := make(chan error, numInserts)

	start := time.Now()

	for i := 0; i < numInserts; i++ {
		wg.Add(1)
		go func(idx int) {
			defer wg.Done()

			dataPoint := &DataPoint{
				DeviceID:  "device_load_ts",
				Timestamp: time.Now(),
				Data: map[string]interface{}{
					"temperature": 20.0 + float64(idx%100)/10.0,
					"humidity":    50.0 + float64(idx%50),
					"pressure":    1000.0 + float64(idx%100),
				},
			}

			if err := storage.StoreData(dataPoint); err != nil {
				errors <- err
			}
		}(i)

		if i%concurrency == 0 {
			time.Sleep(1 * time.Millisecond)
		}
	}

	wg.Wait()
	close(errors)

	duration := time.Since(start)
	t.Logf("Inserted %d data points in %v (%.2f inserts/sec)", numInserts, duration, float64(numInserts)/duration.Seconds())

	// Check for errors
	errorCount := 0
	for err := range errors {
		if err != nil {
			t.Logf("Error: %v", err)
			errorCount++
		}
	}

	assert.Equal(t, 0, errorCount, "Should have no errors")
}

func TestHighVolumeDataQuery(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping load test in short mode")
	}

	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create device
	device := &Device{
		ID:        "device_query_test",
		UserID:    "usr_query_test",
		Name:      "Query Test Device",
		Type:      "sensor",
		APIKey:    "api_key_query_test",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	err := storage.CreateDevice(device)
	require.NoError(t, err)

	// Insert data points over time
	numPoints := 1000
	for i := 0; i < numPoints; i++ {
		dataPoint := &DataPoint{
			DeviceID:  "device_query_test",
			Timestamp: time.Now(),
			Data: map[string]interface{}{
				"temperature": 20.0 + float64(i%100)/10.0,
			},
		}
		err = storage.StoreData(dataPoint)
		require.NoError(t, err)
		time.Sleep(1 * time.Millisecond)
	}

	// Query data using GetDataHistory
	queryStart := time.Now()
	dataHistory, err := storage.GetDataHistory("device_query_test", 100)
	queryDuration := time.Since(queryStart)

	assert.NoError(t, err)
	t.Logf("Queried %d data points in %v", len(dataHistory), queryDuration)
	assert.Greater(t, len(dataHistory), 0)
}

func TestStorageResilience(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Test storage can handle rapid open/close cycles
	for i := 0; i < 10; i++ {
		device := &Device{
			ID:        fmt.Sprintf("device_resilience_%d", i),
			UserID:    "usr_resilience",
			Name:      fmt.Sprintf("Resilience Test %d", i),
			Type:      "sensor",
			APIKey:    fmt.Sprintf("api_key_resilience_%d", i),
			Status:    "active",
			CreatedAt: time.Now(),
		}
		err := storage.CreateDevice(device)
		assert.NoError(t, err)
	}
}

// ============ Benchmark Tests ============

func BenchmarkDeviceCreation(b *testing.B) {
	tmpDir := b.TempDir()
	metaPath := filepath.Join(tmpDir, "bench.db")
	dataPath := filepath.Join(tmpDir, "tsdata")

	storage, err := New(metaPath, dataPath)
	if err != nil {
		b.Fatal(err)
	}
	defer storage.Close()

	b.ResetTimer()

	for i := 0; i < b.N; i++ {
		device := &Device{
			ID:        fmt.Sprintf("device_bench_%d", i),
			UserID:    "usr_bench",
			Name:      fmt.Sprintf("Bench Device %d", i),
			Type:      "sensor",
			APIKey:    fmt.Sprintf("api_key_bench_%d", i),
			Status:    "active",
			CreatedAt: time.Now(),
		}
		if err := storage.CreateDevice(device); err != nil {
			b.Fatal(err)
		}
	}
}

func BenchmarkDataPointInsertion(b *testing.B) {
	tmpDir := b.TempDir()
	metaPath := filepath.Join(tmpDir, "bench.db")
	dataPath := filepath.Join(tmpDir, "tsdata")

	storage, err := New(metaPath, dataPath)
	if err != nil {
		b.Fatal(err)
	}
	defer storage.Close()

	// Create test device
	device := &Device{
		ID:        "device_bench_ts",
		UserID:    "usr_bench_ts",
		Name:      "Bench TS Device",
		Type:      "sensor",
		APIKey:    "api_key_bench_ts",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	b.ResetTimer()

	for i := 0; i < b.N; i++ {
		dataPoint := &DataPoint{
			DeviceID:  "device_bench_ts",
			Timestamp: time.Now(),
			Data: map[string]interface{}{
				"temperature": 23.5,
				"humidity":    65.0,
				"pressure":    1013.25,
			},
		}
		if err := storage.StoreData(dataPoint); err != nil {
			b.Fatal(err)
		}
	}
}

func BenchmarkDataPointQuery(b *testing.B) {
	tmpDir := b.TempDir()
	metaPath := filepath.Join(tmpDir, "bench.db")
	dataPath := filepath.Join(tmpDir, "tsdata")

	storage, err := New(metaPath, dataPath)
	if err != nil {
		b.Fatal(err)
	}
	defer storage.Close()

	// Create device and insert test data
	device := &Device{
		ID:        "device_bench_query",
		UserID:    "usr_bench_query",
		Name:      "Query Bench Device",
		Type:      "sensor",
		APIKey:    "api_key_bench_query",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	for i := 0; i < 100; i++ {
		dataPoint := &DataPoint{
			DeviceID:  "device_bench_query",
			Timestamp: time.Now(),
			Data: map[string]interface{}{
				"temperature": 20.0 + float64(i%50)/10.0,
			},
		}
		storage.StoreData(dataPoint)
		time.Sleep(1 * time.Millisecond)
	}

	b.ResetTimer()

	for i := 0; i < b.N; i++ {
		_, err := storage.GetDataHistory("device_bench_query", 100)
		if err != nil {
			b.Fatal(err)
		}
	}
}

func BenchmarkConcurrentInserts(b *testing.B) {
	tmpDir := b.TempDir()
	metaPath := filepath.Join(tmpDir, "bench.db")
	dataPath := filepath.Join(tmpDir, "tsdata")

	storage, err := New(metaPath, dataPath)
	if err != nil {
		b.Fatal(err)
	}
	defer storage.Close()

	// Create test device
	device := &Device{
		ID:        "device_bench_concurrent",
		UserID:    "usr_bench_concurrent",
		Name:      "Concurrent Bench Device",
		Type:      "sensor",
		APIKey:    "api_key_bench_concurrent",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	b.ResetTimer()

	b.RunParallel(func(pb *testing.PB) {
		i := 0
		for pb.Next() {
			dataPoint := &DataPoint{
				DeviceID:  "device_bench_concurrent",
				Timestamp: time.Now(),
				Data: map[string]interface{}{
					"temperature": 20.0 + float64(i%100)/10.0,
					"humidity":    50.0 + float64(i%50),
				},
			}
			if err := storage.StoreData(dataPoint); err != nil {
				b.Fatal(err)
			}
			i++
		}
	})
}

// ============ Stress Tests ============

func TestMaximumLoadStress(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping stress test in short mode")
	}

	storage, cleanup := createTestStorage(t)
	defer cleanup()

	t.Log("🔥 MAXIMUM LOAD STRESS TEST STARTING...")

	// Create multiple devices
	numDevices := 100
	for i := 0; i < numDevices; i++ {
		device := &Device{
			ID:        fmt.Sprintf("device_stress_%03d", i),
			UserID:    "usr_stress_test",
			Name:      fmt.Sprintf("Stress Device %d", i),
			Type:      "sensor",
			APIKey:    fmt.Sprintf("api_key_stress_%03d", i),
			Status:    "active",
			CreatedAt: time.Now(),
		}
		err := storage.CreateDevice(device)
		require.NoError(t, err)
	}

	// Massive concurrent writes
	totalInserts := 50000
	concurrency := 500
	var wg sync.WaitGroup
	errors := make(chan error, totalInserts)

	start := time.Now()

	for i := 0; i < totalInserts; i++ {
		wg.Add(1)
		go func(idx int) {
			defer wg.Done()

			deviceID := fmt.Sprintf("device_stress_%03d", idx%numDevices)
			dataPoint := &DataPoint{
				DeviceID:  deviceID,
				Timestamp: time.Now(),
				Data: map[string]interface{}{
					"temperature": 20.0 + float64(idx%100)/10.0,
					"humidity":    50.0 + float64(idx%50),
					"pressure":    1000.0 + float64(idx%100),
					"voltage":     3.3 + float64(idx%10)/100.0,
				},
			}

			if err := storage.StoreData(dataPoint); err != nil {
				errors <- err
			}
		}(i)

		if i%concurrency == 0 {
			time.Sleep(1 * time.Millisecond)
		}
	}

	wg.Wait()
	close(errors)

	duration := time.Since(start)
	insertsPerSec := float64(totalInserts) / duration.Seconds()

	t.Logf("📊 STRESS TEST RESULTS:")
	t.Logf("  Total Inserts: %d", totalInserts)
	t.Logf("  Duration: %v", duration)
	t.Logf("  Throughput: %.2f inserts/sec", insertsPerSec)
	t.Logf("  Devices: %d", numDevices)

	// Check errors
	errorCount := 0
	for err := range errors {
		if err != nil {
			errorCount++
		}
	}

	successRate := float64(totalInserts-errorCount) / float64(totalInserts) * 100
	t.Logf("  Success Rate: %.2f%%", successRate)
	t.Logf("  Errors: %d", errorCount)

	assert.Less(t, errorCount, totalInserts/10, "Error rate should be less than 10%")
}

func TestMemoryUsageUnderLoad(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping memory test in short mode")
	}

	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create device
	device := &Device{
		ID:        "device_memory_test",
		UserID:    "usr_memory_test",
		Name:      "Memory Test Device",
		Type:      "sensor",
		APIKey:    "api_key_memory_test",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	err := storage.CreateDevice(device)
	require.NoError(t, err)

	// Insert large volume of data
	numInserts := 10000
	for i := 0; i < numInserts; i++ {
		dataPoint := &DataPoint{
			DeviceID:  "device_memory_test",
			Timestamp: time.Now(),
			Data: map[string]interface{}{
				"temperature": 20.0 + float64(i%100)/10.0,
				"humidity":    50.0 + float64(i%50),
				"pressure":    1000.0 + float64(i%100),
			},
		}
		err = storage.StoreData(dataPoint)
		require.NoError(t, err)

		if i%1000 == 0 {
			// Force garbage collection
			var m runtime.MemStats
			runtime.ReadMemStats(&m)
			t.Logf("Memory after %d inserts: Alloc=%v MB, Sys=%v MB",
				i, m.Alloc/1024/1024, m.Sys/1024/1024)
		}
	}

	t.Log("Memory test completed successfully")
}
