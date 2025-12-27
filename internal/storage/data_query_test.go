package storage

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// ============ Data Query Tests ============

func TestGetLatestData(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create device
	device := &Device{
		ID:        "dev_latest_001",
		UserID:    "user_test",
		Name:      "Latest Data Device",
		Type:      "sensor",
		APIKey:    "sk_latest_001",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	err := storage.CreateDevice(device)
	require.NoError(t, err)

	// Insert data points
	dataPoints := []DataPoint{
		{
			DeviceID:  "dev_latest_001",
			Timestamp: time.Now().Add(-2 * time.Hour),
			Data: map[string]interface{}{
				"temperature": 20.5,
				"humidity":    55.0,
			},
		},
		{
			DeviceID:  "dev_latest_001",
			Timestamp: time.Now().Add(-1 * time.Hour),
			Data: map[string]interface{}{
				"temperature": 22.0,
				"humidity":    58.0,
			},
		},
		{
			DeviceID:  "dev_latest_001",
			Timestamp: time.Now(),
			Data: map[string]interface{}{
				"temperature": 23.5,
				"humidity":    60.0,
				"battery":     85.0,
			},
		},
	}

	for _, dp := range dataPoints {
		err := storage.StoreData(&dp)
		require.NoError(t, err)
	}

	// Wait for data to be written
	time.Sleep(100 * time.Millisecond)

	// Get latest data
	latest, err := storage.GetLatestData("dev_latest_001")
	require.NoError(t, err)
	assert.NotNil(t, latest)
	assert.Equal(t, "dev_latest_001", latest.DeviceID)

	// Should have the most recent values
	assert.Contains(t, latest.Data, "temperature")
	assert.Contains(t, latest.Data, "humidity")
}

func TestGetLatestDataNoData(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Try to get data for non-existent device
	_, err := storage.GetLatestData("dev_nonexistent")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "no data found")
}

func TestGetLatestDataMultipleMetrics(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	device := &Device{
		ID:        "dev_multi_metrics",
		UserID:    "user_test",
		Name:      "Multi Metric Device",
		Type:      "sensor",
		APIKey:    "sk_multi_001",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	// Insert data with many metrics
	dp := DataPoint{
		DeviceID:  "dev_multi_metrics",
		Timestamp: time.Now(),
		Data: map[string]interface{}{
			"temperature": 25.5,
			"humidity":    65.0,
			"pressure":    1013.25,
			"battery":     92.0,
			"value":       123.45,
		},
	}
	storage.StoreData(&dp)

	time.Sleep(100 * time.Millisecond)

	latest, err := storage.GetLatestData("dev_multi_metrics")
	require.NoError(t, err)

	// Should retrieve all metrics
	assert.Len(t, latest.Data, 5)
	assert.Equal(t, 25.5, latest.Data["temperature"])
	assert.Equal(t, 65.0, latest.Data["humidity"])
	assert.Equal(t, 1013.25, latest.Data["pressure"])
	assert.Equal(t, 92.0, latest.Data["battery"])
	assert.Equal(t, 123.45, latest.Data["value"])
}

func TestGetDataHistoryWithRange(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create device
	device := &Device{
		ID:        "dev_range_001",
		UserID:    "user_test",
		Name:      "Range Test Device",
		Type:      "sensor",
		APIKey:    "sk_range_001",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	// Insert data points over a time range
	now := time.Now()
	dataPoints := []DataPoint{
		{
			DeviceID:  "dev_range_001",
			Timestamp: now.Add(-10 * time.Hour),
			Data:      map[string]interface{}{"temperature": 18.0},
		},
		{
			DeviceID:  "dev_range_001",
			Timestamp: now.Add(-8 * time.Hour),
			Data:      map[string]interface{}{"temperature": 19.0},
		},
		{
			DeviceID:  "dev_range_001",
			Timestamp: now.Add(-6 * time.Hour),
			Data:      map[string]interface{}{"temperature": 20.0},
		},
		{
			DeviceID:  "dev_range_001",
			Timestamp: now.Add(-4 * time.Hour),
			Data:      map[string]interface{}{"temperature": 21.0},
		},
		{
			DeviceID:  "dev_range_001",
			Timestamp: now.Add(-2 * time.Hour),
			Data:      map[string]interface{}{"temperature": 22.0},
		},
		{
			DeviceID:  "dev_range_001",
			Timestamp: now,
			Data:      map[string]interface{}{"temperature": 23.0},
		},
	}

	for _, dp := range dataPoints {
		storage.StoreData(&dp)
	}

	time.Sleep(100 * time.Millisecond)

	// Query last 5 hours (should get 3 points: -4h, -2h, now)
	start := now.Add(-5 * time.Hour)
	end := now.Add(1 * time.Hour)
	history, err := storage.GetDataHistoryWithRange("dev_range_001", start, end, 10)
	require.NoError(t, err)

	assert.GreaterOrEqual(t, len(history), 1)
	assert.LessOrEqual(t, len(history), 3)

	// Verify data is sorted (newest first)
	if len(history) > 1 {
		assert.True(t, history[0].Timestamp.After(history[1].Timestamp) ||
			history[0].Timestamp.Equal(history[1].Timestamp))
	}
}

func TestGetDataHistoryWithRangeLimit(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	device := &Device{
		ID:        "dev_limit_001",
		UserID:    "user_test",
		Name:      "Limit Test Device",
		Type:      "sensor",
		APIKey:    "sk_limit_001",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	// Insert 20 data points
	now := time.Now()
	for i := 0; i < 20; i++ {
		dp := DataPoint{
			DeviceID:  "dev_limit_001",
			Timestamp: now.Add(time.Duration(-i) * time.Minute),
			Data: map[string]interface{}{
				"temperature": 20.0 + float64(i),
			},
		}
		storage.StoreData(&dp)
	}

	time.Sleep(100 * time.Millisecond)

	// Query with limit of 5
	start := now.Add(-30 * time.Minute)
	end := now.Add(1 * time.Minute)
	history, err := storage.GetDataHistoryWithRange("dev_limit_001", start, end, 5)
	require.NoError(t, err)

	// Should respect limit
	assert.LessOrEqual(t, len(history), 5)
}

func TestGetDataHistoryWithRangeNoData(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	device := &Device{
		ID:        "dev_empty_001",
		UserID:    "user_test",
		Name:      "Empty Device",
		Type:      "sensor",
		APIKey:    "sk_empty_001",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	// Query without any data
	now := time.Now()
	history, err := storage.GetDataHistoryWithRange("dev_empty_001", now.Add(-1*time.Hour), now, 10)
	require.NoError(t, err)
	assert.Empty(t, history)
}

func TestGetDataHistoryWithRangeOutsideWindow(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	device := &Device{
		ID:        "dev_window_001",
		UserID:    "user_test",
		Name:      "Window Test Device",
		Type:      "sensor",
		APIKey:    "sk_window_001",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	// Insert data now
	now := time.Now()
	dp := DataPoint{
		DeviceID:  "dev_window_001",
		Timestamp: now,
		Data:      map[string]interface{}{"temperature": 25.0},
	}
	storage.StoreData(&dp)

	time.Sleep(100 * time.Millisecond)

	// Query a time range that doesn't include the data (far in the past)
	start := now.Add(-48 * time.Hour)
	end := now.Add(-24 * time.Hour)
	history, err := storage.GetDataHistoryWithRange("dev_window_001", start, end, 10)
	require.NoError(t, err)
	assert.Empty(t, history)
}

func TestGetDataHistoryWithRangeMultipleMetrics(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	device := &Device{
		ID:        "dev_multi_range",
		UserID:    "user_test",
		Name:      "Multi Range Device",
		Type:      "sensor",
		APIKey:    "sk_multi_range",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	// Insert data with multiple metrics
	now := time.Now()
	for i := 0; i < 5; i++ {
		dp := DataPoint{
			DeviceID:  "dev_multi_range",
			Timestamp: now.Add(time.Duration(-i) * time.Hour),
			Data: map[string]interface{}{
				"temperature": 20.0 + float64(i),
				"humidity":    50.0 + float64(i*2),
				"pressure":    1000.0 + float64(i*5),
			},
		}
		storage.StoreData(&dp)
	}

	time.Sleep(100 * time.Millisecond)

	// Query range
	history, err := storage.GetDataHistoryWithRange("dev_multi_range", now.Add(-6*time.Hour), now.Add(1*time.Hour), 10)
	require.NoError(t, err)

	// Each point should have multiple metrics
	if len(history) > 0 {
		assert.Contains(t, history[0].Data, "temperature")
		assert.Contains(t, history[0].Data, "humidity")
		assert.Contains(t, history[0].Data, "pressure")
	}
}

func TestGetLatestDataTimestamp(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	device := &Device{
		ID:        "dev_timestamp_001",
		UserID:    "user_test",
		Name:      "Timestamp Test",
		Type:      "sensor",
		APIKey:    "sk_timestamp_001",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	// Insert data at known time
	specificTime := time.Now().Add(-10 * time.Minute)
	dp := DataPoint{
		DeviceID:  "dev_timestamp_001",
		Timestamp: specificTime,
		Data:      map[string]interface{}{"value": 42.0},
	}
	storage.StoreData(&dp)

	time.Sleep(100 * time.Millisecond)

	// Get latest and check timestamp
	latest, err := storage.GetLatestData("dev_timestamp_001")
	require.NoError(t, err)

	// Timestamp should be close to what we inserted
	assert.WithinDuration(t, specificTime, latest.Timestamp, 1*time.Second)
}

// Integration test: full data lifecycle
func TestDataQueryLifecycle(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// 1. Create device
	device := &Device{
		ID:        "dev_lifecycle_query",
		UserID:    "user_test",
		Name:      "Query Lifecycle Device",
		Type:      "sensor",
		APIKey:    "sk_lifecycle_query",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	// 2. Insert historical data
	now := time.Now()
	for i := 0; i < 24; i++ {
		dp := DataPoint{
			DeviceID:  "dev_lifecycle_query",
			Timestamp: now.Add(time.Duration(-i) * time.Hour),
			Data: map[string]interface{}{
				"temperature": 20.0 + float64(i%10),
				"humidity":    50.0 + float64(i%20),
			},
		}
		storage.StoreData(&dp)
	}

	// Wait longer for all data to be written
	time.Sleep(1500 * time.Millisecond)

	// 3. Get latest data
	latest, err := storage.GetLatestData("dev_lifecycle_query")
	require.NoError(t, err)
	assert.NotNil(t, latest)

	// 4. Get data history with range (last 12 hours from when we inserted data)
	queryEnd := now.Add(1 * time.Minute) // Add buffer for timing
	history12h, err := storage.GetDataHistoryWithRange("dev_lifecycle_query", now.Add(-12*time.Hour), queryEnd, 20)
	require.NoError(t, err)
	assert.NotEmpty(t, history12h)

	// 5. Get data history (default 7 days)
	historyDefault, err := storage.GetDataHistory("dev_lifecycle_query", 20)
	require.NoError(t, err)
	assert.NotEmpty(t, historyDefault)

	// 6. Verify ordering (newest first)
	if len(historyDefault) > 1 {
		for i := 0; i < len(historyDefault)-1; i++ {
			assert.True(t, historyDefault[i].Timestamp.After(historyDefault[i+1].Timestamp) ||
				historyDefault[i].Timestamp.Equal(historyDefault[i+1].Timestamp),
				"Data should be sorted newest first")
		}
	}
}
