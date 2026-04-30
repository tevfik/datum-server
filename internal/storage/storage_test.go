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

	storage, err := New(metaPath, dataPath, 7*24*time.Hour)
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

		// time.Sleep removed for max throughput
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

		// time.Sleep removed for max throughput
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

	storage, err := New(metaPath, dataPath, 7*24*time.Hour)
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

	storage, err := New(metaPath, dataPath, 7*24*time.Hour)
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

	storage, err := New(metaPath, dataPath, 7*24*time.Hour)
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

	storage, err := New(metaPath, dataPath, 7*24*time.Hour)
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

		// time.Sleep removed for max throughput
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

// ============ Provisioning Tests ============

func TestCreateProvisioningRequest(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create user first
	user := &User{
		ID:           "usr_prov_001",
		Email:        "prov@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	// Create provisioning request
	req := &ProvisioningRequest{
		ID:         "prov_test_001",
		DeviceUID:  "AABBCCDDEEFF",
		UserID:     "usr_prov_001",
		DeviceName: "Test Device",
		DeviceType: "ESP32",
		Status:     "pending",
		DeviceID:   "dev_test_001",
		APIKey:     "dk_test_key",
		ServerURL:  "http://localhost:8000",
		WiFiSSID:   "TestWiFi",
		WiFiPass:   "password123",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}

	err = storage.CreateProvisioningRequest(req)
	assert.NoError(t, err)
}

func TestCreateProvisioningRequestDuplicateUID(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{
		ID:           "usr_prov_002",
		Email:        "prov2@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	// Create first request
	req1 := &ProvisioningRequest{
		ID:         "prov_test_002",
		DeviceUID:  "AABBCCDDEEFF",
		UserID:     "usr_prov_002",
		DeviceName: "Device 1",
		Status:     "pending",
		DeviceID:   "dev_test_002",
		APIKey:     "dk_test_key_2",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err = storage.CreateProvisioningRequest(req1)
	require.NoError(t, err)

	// Try to create another request with same UID
	req2 := &ProvisioningRequest{
		ID:         "prov_test_003",
		DeviceUID:  "AABBCCDDEEFF", // Same UID
		UserID:     "usr_prov_002",
		DeviceName: "Device 2",
		Status:     "pending",
		DeviceID:   "dev_test_003",
		APIKey:     "dk_test_key_3",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err = storage.CreateProvisioningRequest(req2)
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "already exists")
}

func TestCreateProvisioningRequestAlreadyRegisteredDevice(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{
		ID:           "usr_prov_003",
		Email:        "prov3@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	// Create and complete a provisioning request
	req := &ProvisioningRequest{
		ID:         "prov_test_004",
		DeviceUID:  "112233445566",
		UserID:     "usr_prov_003",
		DeviceName: "Existing Device",
		Status:     "pending",
		DeviceID:   "dev_test_004",
		APIKey:     "dk_test_key_4",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err = storage.CreateProvisioningRequest(req)
	require.NoError(t, err)

	// Complete provisioning
	_, err = storage.CompleteProvisioningRequest("prov_test_004")
	require.NoError(t, err)

	// Try to create new request with same UID
	req2 := &ProvisioningRequest{
		ID:         "prov_test_005",
		DeviceUID:  "112233445566", // Same UID as registered device
		UserID:     "usr_prov_003",
		DeviceName: "Another Device",
		Status:     "pending",
		DeviceID:   "dev_test_005",
		APIKey:     "dk_test_key_5",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err = storage.CreateProvisioningRequest(req2)
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "already registered")
}

func TestGetProvisioningRequestByUID(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{
		ID:           "usr_prov_004",
		Email:        "prov4@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	req := &ProvisioningRequest{
		ID:         "prov_test_006",
		DeviceUID:  "FFEEDDCCBBAA",
		UserID:     "usr_prov_004",
		DeviceName: "UID Test Device",
		Status:     "pending",
		DeviceID:   "dev_test_006",
		APIKey:     "dk_test_key_6",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err = storage.CreateProvisioningRequest(req)
	require.NoError(t, err)

	// Retrieve by UID
	retrieved, err := storage.GetProvisioningRequestByUID("FFEEDDCCBBAA")
	assert.NoError(t, err)
	assert.Equal(t, req.ID, retrieved.ID)
	assert.Equal(t, req.DeviceUID, retrieved.DeviceUID)
	assert.Equal(t, req.DeviceName, retrieved.DeviceName)
}

func TestGetProvisioningRequestByUIDNotFound(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	_, err := storage.GetProvisioningRequestByUID("NONEXISTENT")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "no provisioning request found")
}

func TestGetProvisioningRequest(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{
		ID:           "usr_prov_005",
		Email:        "prov5@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	req := &ProvisioningRequest{
		ID:         "prov_test_007",
		DeviceUID:  "123456789ABC",
		UserID:     "usr_prov_005",
		DeviceName: "ID Test Device",
		Status:     "pending",
		DeviceID:   "dev_test_007",
		APIKey:     "dk_test_key_7",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err = storage.CreateProvisioningRequest(req)
	require.NoError(t, err)

	// Retrieve by ID
	retrieved, err := storage.GetProvisioningRequest("prov_test_007")
	assert.NoError(t, err)
	assert.Equal(t, req.DeviceUID, retrieved.DeviceUID)
	assert.Equal(t, req.UserID, retrieved.UserID)
}

func TestCompleteProvisioningRequest(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{
		ID:           "usr_prov_006",
		Email:        "prov6@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	req := &ProvisioningRequest{
		ID:         "prov_test_008",
		DeviceUID:  "ABCDEF123456",
		UserID:     "usr_prov_006",
		DeviceName: "Complete Test Device",
		DeviceType: "ESP32",
		Status:     "pending",
		DeviceID:   "dev_test_008",
		APIKey:     "dk_test_key_8",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err = storage.CreateProvisioningRequest(req)
	require.NoError(t, err)

	// Complete the request
	device, err := storage.CompleteProvisioningRequest("prov_test_008")
	assert.NoError(t, err)
	assert.NotNil(t, device)
	assert.Equal(t, "dev_test_008", device.ID)
	assert.Equal(t, "usr_prov_006", device.UserID)
	assert.Equal(t, "Complete Test Device", device.Name)
	assert.Equal(t, "ESP32", device.Type)
	assert.Equal(t, "dk_test_key_8", device.APIKey)
	assert.Equal(t, "active", device.Status)

	// Verify request status updated
	updated, err := storage.GetProvisioningRequest("prov_test_008")
	assert.NoError(t, err)
	assert.Equal(t, "completed", updated.Status)

	// Verify device created
	createdDevice, err := storage.GetDevice("dev_test_008")
	assert.NoError(t, err)
	assert.Equal(t, device.Name, createdDevice.Name)

	// Verify UID is now registered
	registered, deviceID, err := storage.IsDeviceUIDRegistered("ABCDEF123456")
	assert.NoError(t, err)
	assert.True(t, registered)
	assert.Equal(t, "dev_test_008", deviceID)
}

func TestCompleteProvisioningRequestExpired(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{
		ID:           "usr_prov_007",
		Email:        "prov7@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	// Create expired request
	req := &ProvisioningRequest{
		ID:         "prov_test_009",
		DeviceUID:  "EXPIRED123456",
		UserID:     "usr_prov_007",
		DeviceName: "Expired Device",
		Status:     "pending",
		DeviceID:   "dev_test_009",
		APIKey:     "dk_test_key_9",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(-1 * time.Hour), // Expired
		CreatedAt:  time.Now().Add(-2 * time.Hour),
	}
	err = storage.CreateProvisioningRequest(req)
	require.NoError(t, err)

	// Try to complete expired request
	_, err = storage.CompleteProvisioningRequest("prov_test_009")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "expired")
}

func TestCompleteProvisioningRequestAlreadyCompleted(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{
		ID:           "usr_prov_008",
		Email:        "prov8@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	req := &ProvisioningRequest{
		ID:         "prov_test_010",
		DeviceUID:  "COMPLETED123",
		UserID:     "usr_prov_008",
		DeviceName: "Already Completed",
		Status:     "pending",
		DeviceID:   "dev_test_010",
		APIKey:     "dk_test_key_10",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err = storage.CreateProvisioningRequest(req)
	require.NoError(t, err)

	// Complete once
	_, err = storage.CompleteProvisioningRequest("prov_test_010")
	assert.NoError(t, err)

	// Try to complete again
	_, err = storage.CompleteProvisioningRequest("prov_test_010")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "not pending")
}

func TestCancelProvisioningRequest(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{
		ID:           "usr_prov_009",
		Email:        "prov9@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	req := &ProvisioningRequest{
		ID:         "prov_test_011",
		DeviceUID:  "CANCEL123456",
		UserID:     "usr_prov_009",
		DeviceName: "Cancel Test",
		Status:     "pending",
		DeviceID:   "dev_test_011",
		APIKey:     "dk_test_key_11",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err = storage.CreateProvisioningRequest(req)
	require.NoError(t, err)

	// Cancel the request
	err = storage.CancelProvisioningRequest("prov_test_011")
	assert.NoError(t, err)

	// Verify status updated
	updated, err := storage.GetProvisioningRequest("prov_test_011")
	assert.NoError(t, err)
	assert.Equal(t, "cancelled", updated.Status)

	// Verify UID is not registered
	registered, _, _ := storage.IsDeviceUIDRegistered("CANCEL123456")
	assert.False(t, registered)
}

func TestCancelProvisioningRequestAlreadyCompleted(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{
		ID:           "usr_prov_010",
		Email:        "prov10@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	req := &ProvisioningRequest{
		ID:         "prov_test_012",
		DeviceUID:  "CANCELCOMP123",
		UserID:     "usr_prov_010",
		DeviceName: "Cancel Completed Test",
		Status:     "pending",
		DeviceID:   "dev_test_012",
		APIKey:     "dk_test_key_12",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err = storage.CreateProvisioningRequest(req)
	require.NoError(t, err)

	// Complete the request
	_, err = storage.CompleteProvisioningRequest("prov_test_012")
	assert.NoError(t, err)

	// Try to cancel completed request
	err = storage.CancelProvisioningRequest("prov_test_012")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "can only cancel pending")
}

func TestGetUserProvisioningRequests(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{
		ID:           "usr_prov_011",
		Email:        "prov11@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	// Create multiple requests
	for i := 1; i <= 3; i++ {
		req := &ProvisioningRequest{
			ID:         fmt.Sprintf("prov_test_01%d", i+2),
			DeviceUID:  fmt.Sprintf("UID%d", i),
			UserID:     "usr_prov_011",
			DeviceName: fmt.Sprintf("Device %d", i),
			Status:     "pending",
			DeviceID:   fmt.Sprintf("dev_test_01%d", i+2),
			APIKey:     fmt.Sprintf("dk_test_key_1%d", i+2),
			ServerURL:  "http://localhost:8000",
			ExpiresAt:  time.Now().Add(15 * time.Minute),
			CreatedAt:  time.Now(),
		}
		err = storage.CreateProvisioningRequest(req)
		require.NoError(t, err)
	}

	// Get all user requests
	requests, err := storage.GetUserProvisioningRequests("usr_prov_011")
	assert.NoError(t, err)
	assert.Len(t, requests, 3)
}

func TestIsDeviceUIDRegistered(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Check unregistered UID
	registered, deviceID, err := storage.IsDeviceUIDRegistered("NOTREGISTERED")
	assert.NoError(t, err)
	assert.False(t, registered)
	assert.Empty(t, deviceID)

	// Create and complete provisioning
	user := &User{
		ID:           "usr_prov_012",
		Email:        "prov12@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err = storage.CreateUser(user)
	require.NoError(t, err)

	req := &ProvisioningRequest{
		ID:         "prov_test_016",
		DeviceUID:  "REGISTERED123",
		UserID:     "usr_prov_012",
		DeviceName: "Registered Device",
		Status:     "pending",
		DeviceID:   "dev_test_016",
		APIKey:     "dk_test_key_16",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err = storage.CreateProvisioningRequest(req)
	require.NoError(t, err)

	_, err = storage.CompleteProvisioningRequest("prov_test_016")
	assert.NoError(t, err)

	// Check registered UID
	registered, deviceID, err = storage.IsDeviceUIDRegistered("REGISTERED123")
	assert.NoError(t, err)
	assert.True(t, registered)
	assert.Equal(t, "dev_test_016", deviceID)
}

func TestCleanupExpiredProvisioningRequests(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{
		ID:           "usr_prov_013",
		Email:        "prov13@example.com",
		PasswordHash: "hashed",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	err := storage.CreateUser(user)
	require.NoError(t, err)

	// Create expired request
	expiredReq := &ProvisioningRequest{
		ID:         "prov_expired_001",
		DeviceUID:  "EXPIRED001",
		UserID:     "usr_prov_013",
		DeviceName: "Expired 1",
		Status:     "pending",
		DeviceID:   "dev_expired_001",
		APIKey:     "dk_expired_001",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(-1 * time.Hour),
		CreatedAt:  time.Now().Add(-2 * time.Hour),
	}
	err = storage.CreateProvisioningRequest(expiredReq)
	require.NoError(t, err)

	// Create valid request
	validReq := &ProvisioningRequest{
		ID:         "prov_valid_001",
		DeviceUID:  "VALID001",
		UserID:     "usr_prov_013",
		DeviceName: "Valid 1",
		Status:     "pending",
		DeviceID:   "dev_valid_001",
		APIKey:     "dk_valid_001",
		ServerURL:  "http://localhost:8000",
		ExpiresAt:  time.Now().Add(15 * time.Minute),
		CreatedAt:  time.Now(),
	}
	err = storage.CreateProvisioningRequest(validReq)
	require.NoError(t, err)

	// Cleanup expired
	cleaned, err := storage.CleanupExpiredProvisioningRequests()
	assert.NoError(t, err)
	assert.Equal(t, 1, cleaned)

	// Verify expired is marked expired
	expired, err := storage.GetProvisioningRequest("prov_expired_001")
	assert.NoError(t, err)
	assert.Equal(t, "expired", expired.Status)

	// Verify valid is still pending
	valid, err := storage.GetProvisioningRequest("prov_valid_001")
	assert.NoError(t, err)
	assert.Equal(t, "pending", valid.Status)
}
