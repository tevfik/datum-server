package storage

import (
	"fmt"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// Helper to create test storage with temp directories
func createTestStorageForRetention(t *testing.T) (*Storage, string) {
	tmpDir := t.TempDir()
	metaPath := filepath.Join(tmpDir, "test_meta.db")
	dataPath := filepath.Join(tmpDir, "test_data")

	err := os.MkdirAll(dataPath, 0755)
	require.NoError(t, err)

	storage, err := New(metaPath, dataPath)
	require.NoError(t, err)

	return storage, dataPath
}

// Helper to create fake partition directories
func createFakePartition(t testing.TB, dataPath string, startTs, endTs int64) {
	partitionName := fmt.Sprintf("p-%d-%d", startTs, endTs)
	partitionPath := filepath.Join(dataPath, partitionName)

	err := os.MkdirAll(partitionPath, 0755)
	if err != nil {
		t.Fatal(err)
	}

	// Create a dummy file inside partition
	dummyFile := filepath.Join(partitionPath, "data.bin")
	err = os.WriteFile(dummyFile, []byte("test data"), 0644)
	if err != nil {
		t.Fatal(err)
	}
}

// ============ Retention Config Tests ============

func TestGetRetentionConfigFromEnv(t *testing.T) {
	// Save original env vars
	originalMaxDays := os.Getenv("RETENTION_MAX_DAYS")
	originalCheckHours := os.Getenv("RETENTION_CHECK_HOURS")
	defer func() {
		os.Setenv("RETENTION_MAX_DAYS", originalMaxDays)
		os.Setenv("RETENTION_CHECK_HOURS", originalCheckHours)
	}()

	// Test default values
	os.Unsetenv("RETENTION_MAX_DAYS")
	os.Unsetenv("RETENTION_CHECK_HOURS")
	config := GetRetentionConfigFromEnv()
	assert.Equal(t, 7*24*time.Hour, config.MaxAge, "Default max age should be 7 days")
	assert.Equal(t, 1*time.Hour, config.CleanupEvery, "Default cleanup should be 1 hour")

	// Test custom max days
	os.Setenv("RETENTION_MAX_DAYS", "30")
	config = GetRetentionConfigFromEnv()
	assert.Equal(t, 30*24*time.Hour, config.MaxAge, "Custom max age should be 30 days")

	// Test custom check hours
	os.Setenv("RETENTION_CHECK_HOURS", "6")
	config = GetRetentionConfigFromEnv()
	assert.Equal(t, 6*time.Hour, config.CleanupEvery, "Custom cleanup should be 6 hours")

	// Test invalid values (should use defaults)
	os.Setenv("RETENTION_MAX_DAYS", "invalid")
	os.Setenv("RETENTION_CHECK_HOURS", "-1")
	config = GetRetentionConfigFromEnv()
	assert.Equal(t, 7*24*time.Hour, config.MaxAge, "Invalid max days should use default")
	assert.Equal(t, 1*time.Hour, config.CleanupEvery, "Invalid check hours should use default")
}

func TestDefaultRetentionConfig(t *testing.T) {
	assert.Equal(t, 7*24*time.Hour, DefaultRetention.MaxAge)
	assert.Equal(t, 1*time.Hour, DefaultRetention.CleanupEvery)
}

// ============ Partition Cleanup Tests ============

func TestCleanupOldPartitions(t *testing.T) {
	storage, dataPath := createTestStorageForRetention(t)
	defer storage.Close()

	now := time.Now()

	// Create partitions with different ages
	// Old partition (should be deleted)
	oldStartTs := now.Add(-10 * 24 * time.Hour).Unix()
	oldEndTs := now.Add(-9 * 24 * time.Hour).Unix()
	createFakePartition(t, dataPath, oldStartTs, oldEndTs)

	// Recent partition (should be kept)
	recentStartTs := now.Add(-2 * time.Hour).Unix()
	recentEndTs := now.Add(-1 * time.Hour).Unix()
	createFakePartition(t, dataPath, recentStartTs, recentEndTs)

	// Cleanup with 7 day retention
	maxAge := 7 * 24 * time.Hour
	deletedCount := storage.CleanupNow(dataPath, maxAge)

	assert.Equal(t, 1, deletedCount, "Should delete 1 old partition")

	// Verify old partition is deleted
	oldPartitionName := fmt.Sprintf("p-%d-%d", oldStartTs, oldEndTs)
	oldPartitionPath := filepath.Join(dataPath, oldPartitionName)
	_, err := os.Stat(oldPartitionPath)
	assert.True(t, os.IsNotExist(err), "Old partition should be deleted")

	// Verify recent partition still exists
	recentPartitionName := fmt.Sprintf("p-%d-%d", recentStartTs, recentEndTs)
	recentPartitionPath := filepath.Join(dataPath, recentPartitionName)
	_, err = os.Stat(recentPartitionPath)
	assert.NoError(t, err, "Recent partition should still exist")
}

func TestCleanupMultiplePartitions(t *testing.T) {
	storage, dataPath := createTestStorageForRetention(t)
	defer storage.Close()

	now := time.Now()

	// Create 5 old partitions (should all be deleted)
	for i := 0; i < 5; i++ {
		startTs := now.Add(-time.Duration(20+i) * 24 * time.Hour).Unix()
		endTs := now.Add(-time.Duration(19+i) * 24 * time.Hour).Unix()
		createFakePartition(t, dataPath, startTs, endTs)
	}

	// Create 3 recent partitions (should be kept)
	for i := 0; i < 3; i++ {
		startTs := now.Add(-time.Duration(2+i) * time.Hour).Unix()
		endTs := now.Add(-time.Duration(1+i) * time.Hour).Unix()
		createFakePartition(t, dataPath, startTs, endTs)
	}

	maxAge := 7 * 24 * time.Hour
	deletedCount := storage.CleanupNow(dataPath, maxAge)

	assert.Equal(t, 5, deletedCount, "Should delete 5 old partitions")

	// Count remaining partitions
	entries, err := os.ReadDir(dataPath)
	require.NoError(t, err)

	partitionCount := 0
	for _, entry := range entries {
		if entry.IsDir() && len(entry.Name()) > 2 && entry.Name()[:2] == "p-" {
			partitionCount++
		}
	}

	assert.Equal(t, 3, partitionCount, "Should have 3 partitions remaining")
}

func TestCleanupEmptyDataPath(t *testing.T) {
	storage, dataPath := createTestStorageForRetention(t)
	defer storage.Close()

	// No partitions created
	maxAge := 7 * 24 * time.Hour
	deletedCount := storage.CleanupNow(dataPath, maxAge)

	assert.Equal(t, 0, deletedCount, "Should delete 0 partitions from empty directory")
}

func TestCleanupNonPartitionDirs(t *testing.T) {
	storage, dataPath := createTestStorageForRetention(t)
	defer storage.Close()

	// Create directories that don't match partition naming
	otherDir := filepath.Join(dataPath, "other-dir")
	err := os.MkdirAll(otherDir, 0755)
	require.NoError(t, err)

	notPartition := filepath.Join(dataPath, "not-a-partition")
	err = os.MkdirAll(notPartition, 0755)
	require.NoError(t, err)

	// Create a file (not directory)
	dummyFile := filepath.Join(dataPath, "p-123-456")
	err = os.WriteFile(dummyFile, []byte("test"), 0644)
	require.NoError(t, err)

	maxAge := 1 * time.Hour
	deletedCount := storage.CleanupNow(dataPath, maxAge)

	assert.Equal(t, 0, deletedCount, "Should not delete non-partition items")

	// Verify non-partition items still exist
	_, err = os.Stat(otherDir)
	assert.NoError(t, err)
	_, err = os.Stat(notPartition)
	assert.NoError(t, err)
	_, err = os.Stat(dummyFile)
	assert.NoError(t, err)
}

func TestCleanupInvalidPartitionNames(t *testing.T) {
	storage, dataPath := createTestStorageForRetention(t)
	defer storage.Close()

	now := time.Now()

	// Create partition directories with invalid names
	invalidNames := []string{
		"p-123",           // Missing end timestamp (len != 3)
		"p--123-456",      // Empty segment (valid len, but weird)
		"p-123-456-789",   // Too many segments (len != 3)
		"not-a-partition", // Doesn't start with p-
	}

	for _, name := range invalidNames {
		dirPath := filepath.Join(dataPath, name)
		err := os.MkdirAll(dirPath, 0755)
		require.NoError(t, err)
	}

	// Also create one valid old partition that WILL be deleted
	oldTs := now.Add(-30 * 24 * time.Hour).Unix()
	createFakePartition(t, dataPath, oldTs-3600, oldTs)

	maxAge := 7 * 24 * time.Hour
	deletedCount := storage.CleanupNow(dataPath, maxAge)

	// Should only delete the valid old partition
	assert.Equal(t, 1, deletedCount, "Should only delete valid old partition")

	// Verify invalid partitions still exist
	for _, name := range invalidNames {
		dirPath := filepath.Join(dataPath, name)
		_, err := os.Stat(dirPath)
		assert.NoError(t, err, fmt.Sprintf("Invalid partition %s should still exist", name))
	}
}

func TestCleanupNonExistentPath(t *testing.T) {
	storage, _ := createTestStorageForRetention(t)
	defer storage.Close()

	nonExistentPath := "/path/that/does/not/exist"
	maxAge := 7 * 24 * time.Hour

	deletedCount := storage.CleanupNow(nonExistentPath, maxAge)
	assert.Equal(t, 0, deletedCount, "Should return 0 for non-existent path")
}

func TestCleanupBoundaryConditions(t *testing.T) {
	storage, dataPath := createTestStorageForRetention(t)
	defer storage.Close()

	now := time.Now()
	maxAge := 7 * 24 * time.Hour

	// Partition exactly at boundary (endTs == cutoff, should be KEPT - logic is endTs < cutoff)
	boundaryTs := now.Add(-maxAge).Unix()
	createFakePartition(t, dataPath, boundaryTs-3600, boundaryTs)

	// Partition just before boundary (endTs < cutoff, should be DELETED)
	beforeBoundaryTs := now.Add(-maxAge - 1*time.Hour).Unix()
	createFakePartition(t, dataPath, beforeBoundaryTs-3600, beforeBoundaryTs)

	// Partition after boundary (should be kept)
	afterBoundaryTs := now.Add(-maxAge + 1*time.Hour).Unix()
	createFakePartition(t, dataPath, afterBoundaryTs-3600, afterBoundaryTs)

	deletedCount := storage.CleanupNow(dataPath, maxAge)

	assert.Equal(t, 1, deletedCount, "Should delete only partition before cutoff")

	entries, err := os.ReadDir(dataPath)
	require.NoError(t, err)

	partitionCount := 0
	for _, entry := range entries {
		if entry.IsDir() && len(entry.Name()) > 2 && entry.Name()[:2] == "p-" {
			partitionCount++
		}
	}

	assert.Equal(t, 2, partitionCount, "Should keep 2 partitions (at and after boundary)")
}

// ============ Retention Worker Tests ============

func TestStartRetentionWorker(t *testing.T) {
	storage, dataPath := createTestStorageForRetention(t)
	defer storage.Close()

	// Create old partition
	now := time.Now()
	oldStartTs := now.Add(-10 * 24 * time.Hour).Unix()
	oldEndTs := now.Add(-9 * 24 * time.Hour).Unix()
	createFakePartition(t, dataPath, oldStartTs, oldEndTs)

	// Start retention worker with very short intervals for testing
	config := RetentionConfig{
		MaxAge:       5 * 24 * time.Hour,
		CleanupEvery: 100 * time.Millisecond,
	}
	storage.StartRetentionWorker(config, dataPath)

	// Wait for cleanup to run
	time.Sleep(200 * time.Millisecond)

	// Verify partition was deleted
	oldPartitionName := fmt.Sprintf("p-%d-%d", oldStartTs, oldEndTs)
	oldPartitionPath := filepath.Join(dataPath, oldPartitionName)
	_, err := os.Stat(oldPartitionPath)
	assert.True(t, os.IsNotExist(err), "Retention worker should have deleted old partition")
}

func TestRetentionWorkerMultipleCycles(t *testing.T) {
	storage, dataPath := createTestStorageForRetention(t)
	defer storage.Close()

	now := time.Now()

	// Create partitions that will be deleted in different cycles
	for i := 1; i <= 3; i++ {
		startTs := now.Add(-time.Duration(10+i) * 24 * time.Hour).Unix()
		endTs := now.Add(-time.Duration(9+i) * 24 * time.Hour).Unix()
		createFakePartition(t, dataPath, startTs, endTs)
	}

	initialCount := countPartitions(t, dataPath)
	assert.Equal(t, 3, initialCount, "Should start with 3 partitions")

	config := RetentionConfig{
		MaxAge:       5 * 24 * time.Hour,
		CleanupEvery: 50 * time.Millisecond,
	}
	storage.StartRetentionWorker(config, dataPath)

	// Wait for multiple cleanup cycles
	time.Sleep(200 * time.Millisecond)

	finalCount := countPartitions(t, dataPath)
	assert.Equal(t, 0, finalCount, "All old partitions should be deleted")
}

// ============ Integration Tests ============

func TestRetentionWithRealDataPoints(t *testing.T) {
	t.Skip("Skipping - tstorage may not create partition files immediately")
	storage, dataPath := createTestStorageForRetention(t)
	defer storage.Close()

	// Create user and device
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

	device := &Device{
		ID:        "test_device",
		UserID:    "test_user",
		Name:      "Test Device",
		Type:      "sensor",
		APIKey:    "test_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	err = storage.CreateDevice(device)
	require.NoError(t, err)

	// Insert old data points
	oldTime := time.Now().Add(-10 * 24 * time.Hour)
	for i := 0; i < 100; i++ {
		dataPoint := &DataPoint{
			DeviceID:  "test_device",
			Timestamp: oldTime.Add(time.Duration(i) * time.Second),
			Data: map[string]interface{}{
				"temperature": 20.0 + float64(i)*0.1,
			},
		}
		err = storage.StoreData(dataPoint)
		require.NoError(t, err)
	}

	// Verify data was stored (partitions created)
	initialCount := countPartitions(t, dataPath)
	assert.Greater(t, initialCount, 0, "Should have created partitions")

	// Run cleanup with 7 day retention
	maxAge := 7 * 24 * time.Hour
	deletedCount := storage.CleanupNow(dataPath, maxAge)

	assert.Greater(t, deletedCount, 0, "Should delete old partitions")

	// Verify old data is gone
	dataPoints, err := storage.GetDataHistory("test_device", 100)
	require.NoError(t, err)
	assert.Equal(t, 0, len(dataPoints), "Old data should be deleted")
}

func TestRetentionPreservesRecentData(t *testing.T) {
	storage, dataPath := createTestStorageForRetention(t)
	defer storage.Close()

	// Create user and device
	user := &User{
		ID:           "test_user",
		Email:        "test@example.com",
		PasswordHash: "hash",
		Role:         "user",
		Status:       "active",
		CreatedAt:    time.Now(),
	}
	storage.CreateUser(user)

	device := &Device{
		ID:        "test_device",
		UserID:    "test_user",
		Name:      "Test Device",
		Type:      "sensor",
		APIKey:    "test_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	// Insert recent data points
	recentTime := time.Now().Add(-1 * time.Hour)
	for i := 0; i < 50; i++ {
		dataPoint := &DataPoint{
			DeviceID:  "test_device",
			Timestamp: recentTime.Add(time.Duration(i) * time.Second),
			Data: map[string]interface{}{
				"temperature": 25.0,
			},
		}
		storage.StoreData(dataPoint)
	}

	// Run cleanup
	maxAge := 7 * 24 * time.Hour
	storage.CleanupNow(dataPath, maxAge)

	// Verify recent data still exists
	dataPoints, err := storage.GetDataHistory("test_device", 100)
	require.NoError(t, err)
	assert.Equal(t, 50, len(dataPoints), "Recent data should be preserved")
}

// ============ Performance Tests ============

func TestCleanupPerformance(t *testing.T) {
	storage, dataPath := createTestStorageForRetention(t)
	defer storage.Close()

	now := time.Now()

	// Create 1000 old partitions
	numPartitions := 1000
	for i := 0; i < numPartitions; i++ {
		startTs := now.Add(-time.Duration(30+i) * 24 * time.Hour).Unix()
		endTs := now.Add(-time.Duration(29+i) * 24 * time.Hour).Unix()
		createFakePartition(t, dataPath, startTs, endTs)
	}

	maxAge := 7 * 24 * time.Hour

	start := time.Now()
	deletedCount := storage.CleanupNow(dataPath, maxAge)
	duration := time.Since(start)

	assert.Equal(t, numPartitions, deletedCount, "Should delete all old partitions")
	assert.Less(t, duration, 5*time.Second, "Cleanup should complete within 5 seconds")

	t.Logf("Cleanup Performance:")
	t.Logf("  Deleted: %d partitions", deletedCount)
	t.Logf("  Duration: %s", duration)
	t.Logf("  Throughput: %.2f partitions/sec", float64(numPartitions)/duration.Seconds())
}

// ============ Helper Functions ============

func countPartitions(t *testing.T, dataPath string) int {
	entries, err := os.ReadDir(dataPath)
	require.NoError(t, err)

	count := 0
	for _, entry := range entries {
		if entry.IsDir() && len(entry.Name()) > 2 && entry.Name()[:2] == "p-" {
			count++
		}
	}
	return count
}

// ============ Benchmark Tests ============

func BenchmarkCleanup100Partitions(b *testing.B) {
	tmpDir := b.TempDir()
	metaPath := filepath.Join(tmpDir, "bench_meta.db")
	dataPath := filepath.Join(tmpDir, "bench_data")
	os.MkdirAll(dataPath, 0755)

	storage, err := New(metaPath, dataPath)
	if err != nil {
		b.Fatal(err)
	}
	defer storage.Close()

	now := time.Now()
	maxAge := 7 * 24 * time.Hour

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		b.StopTimer()
		// Create 100 old partitions
		for j := 0; j < 100; j++ {
			startTs := now.Add(-time.Duration(30+j) * 24 * time.Hour).Unix()
			endTs := now.Add(-time.Duration(29+j) * 24 * time.Hour).Unix()
			createFakePartition(b, dataPath, startTs, endTs)
		}
		b.StartTimer()

		storage.CleanupNow(dataPath, maxAge)
	}
}

func BenchmarkPartitionFiltering(b *testing.B) {
	tmpDir := b.TempDir()
	metaPath := filepath.Join(tmpDir, "bench_meta.db")
	dataPath := filepath.Join(tmpDir, "bench_data")
	os.MkdirAll(dataPath, 0755)

	storage, err := New(metaPath, dataPath)
	if err != nil {
		b.Fatal(err)
	}
	defer storage.Close()

	// Create mix of old and new partitions
	now := time.Now()
	for i := 0; i < 500; i++ {
		var startTs, endTs int64
		if i%2 == 0 {
			// Old partition
			startTs = now.Add(-time.Duration(30+i) * 24 * time.Hour).Unix()
			endTs = now.Add(-time.Duration(29+i) * 24 * time.Hour).Unix()
		} else {
			// Recent partition
			startTs = now.Add(-time.Duration(i) * time.Hour).Unix()
			endTs = now.Add(-time.Duration(i-1) * time.Hour).Unix()
		}
		createFakePartition(b, dataPath, startTs, endTs)
	}

	maxAge := 7 * 24 * time.Hour

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		storage.CleanupNow(dataPath, maxAge)
	}
}
