package storage

import (
	"fmt"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// ============ GetStats Tests ============

func TestGetStats(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	// Empty storage stats
	stats, err := s.GetStats()
	require.NoError(t, err)
	assert.NotNil(t, stats)
	assert.Equal(t, int64(0), stats.TotalUsers)
	assert.Equal(t, int64(0), stats.TotalDevices)
	assert.False(t, stats.ServerTime.IsZero())

	// Add a user
	user := &User{
		ID:        "usr_stats_001",
		Email:     "stats_user@example.com",
		Role:      "user",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	require.NoError(t, s.CreateUser(user))

	// Add a device (which also adds an apikey: index entry)
	device := &Device{
		ID:        "dev_stats_001",
		UserID:    user.ID,
		Name:      "Stats Device",
		APIKey:    "sk_stats_device_001",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	require.NoError(t, s.CreateDevice(device))

	stats, err = s.GetStats()
	require.NoError(t, err)
	assert.Equal(t, int64(1), stats.TotalUsers)
	assert.Equal(t, int64(1), stats.TotalDevices)
}

// ============ StoreDataBatch Tests ============

func TestStoreDataBatch_Empty(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	err := s.StoreDataBatch([]*DataPoint{})
	assert.NoError(t, err)
}

func TestStoreDataBatch_ExceedsMaxSize(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	// Build a batch larger than maxBatchSize (50000)
	points := make([]*DataPoint, 50001)
	for i := range points {
		points[i] = &DataPoint{DeviceID: "dev_x", Timestamp: time.Now(), Data: map[string]interface{}{"v": float64(i)}}
	}

	err := s.StoreDataBatch(points)
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "exceeds maximum")
}

func TestStoreDataBatch_Success(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	// Create a user and device first
	user := &User{ID: "usr_batch_001", Email: "batch@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	device := &Device{ID: "dev_batch_001", UserID: user.ID, Name: "Batch Device", APIKey: "sk_batch_001", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(device))

	now := time.Now()
	points := []*DataPoint{
		{DeviceID: device.ID, Timestamp: now.Add(-2 * time.Minute), Data: map[string]interface{}{"temperature": 21.5, "humidity": 60.0}},
		{DeviceID: device.ID, Timestamp: now.Add(-1 * time.Minute), Data: map[string]interface{}{"temperature": 22.0, "humidity": 61.0}},
	}

	err := s.StoreDataBatch(points)
	assert.NoError(t, err)

	// Verify shadow state was updated
	latest, err := s.GetLatestData(device.ID)
	require.NoError(t, err)
	assert.NotNil(t, latest)
}

func TestStoreDataBatch_MultipleDevices(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_mb_001", Email: "mb@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	for i := 1; i <= 3; i++ {
		d := &Device{
			ID:        fmt.Sprintf("dev_mb_%03d", i),
			UserID:    user.ID,
			Name:      fmt.Sprintf("Device %d", i),
			APIKey:    fmt.Sprintf("sk_mb_%03d", i),
			Status:    "active",
			CreatedAt: time.Now(),
		}
		require.NoError(t, s.CreateDevice(d))
	}

	points := []*DataPoint{
		{DeviceID: "dev_mb_001", Timestamp: time.Now(), Data: map[string]interface{}{"v": 1.0}},
		{DeviceID: "dev_mb_002", Timestamp: time.Now(), Data: map[string]interface{}{"v": 2.0}},
		{DeviceID: "dev_mb_003", Timestamp: time.Now(), Data: map[string]interface{}{"v": 3.0}},
	}

	err := s.StoreDataBatch(points)
	assert.NoError(t, err)
}

func TestStoreDataBatch_NonNumericValues(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_nn_001", Email: "nn@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))
	device := &Device{ID: "dev_nn_001", UserID: user.ID, Name: "NN Device", APIKey: "sk_nn_001", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(device))

	// Non-numeric data should not cause errors (strings, bools are skipped for tsdb)
	points := []*DataPoint{
		{DeviceID: device.ID, Timestamp: time.Now(), Data: map[string]interface{}{"status": "ok", "active": true, "count": 5}},
	}
	err := s.StoreDataBatch(points)
	assert.NoError(t, err)
}

// ============ GetCommand Tests ============

func TestGetCommand(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	cmd := &Command{
		ID:       "cmd_test_001",
		DeviceID: "dev_test_001",
		Action:   "reboot",
		Status:   "pending",
		Params:   map[string]interface{}{"delay": 5},
	}

	require.NoError(t, s.CreateCommand(cmd))

	retrieved, err := s.GetCommand(cmd.ID)
	require.NoError(t, err)
	assert.Equal(t, cmd.ID, retrieved.ID)
	assert.Equal(t, cmd.Action, retrieved.Action)
	assert.Equal(t, "pending", retrieved.Status)
}

func TestGetCommand_NotFound(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	_, err := s.GetCommand("cmd_nonexistent")
	assert.Error(t, err)
}

// ============ UpdateUserRefreshToken Tests ============

func TestUpdateUserRefreshToken(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_rt_001", Email: "rt@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	err := s.UpdateUserRefreshToken(user.ID, "new_refresh_token_xyz")
	require.NoError(t, err)

	retrieved, err := s.GetUserByID(user.ID)
	require.NoError(t, err)
	assert.Equal(t, "new_refresh_token_xyz", retrieved.RefreshToken)
}

func TestUpdateUserRefreshToken_UserNotFound(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	err := s.UpdateUserRefreshToken("usr_nonexistent", "token")
	assert.Error(t, err)
}

// ============ UpdateDeviceThingDescription Tests ============

func TestUpdateDeviceThingDescription(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_td_001", Email: "td@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	device := &Device{ID: "dev_td_001", UserID: user.ID, Name: "TD Device", APIKey: "sk_td_001", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(device))

	td := map[string]interface{}{
		"@context": "https://www.w3.org/2019/wot/td/v1",
		"@type":    "Thing",
		"title":    "My Temperature Sensor",
		"properties": map[string]interface{}{
			"temperature": map[string]interface{}{
				"type":     "number",
				"unit":     "celsius",
				"readOnly": true,
			},
		},
	}

	err := s.UpdateDeviceThingDescription(device.ID, td)
	require.NoError(t, err)

	retrieved, err := s.GetDevice(device.ID)
	require.NoError(t, err)
	assert.Equal(t, "My Temperature Sensor", retrieved.ThingDescription["title"])
}

func TestUpdateDeviceThingDescription_DeviceNotFound(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	err := s.UpdateDeviceThingDescription("dev_nonexistent", map[string]interface{}{"title": "x"})
	assert.Error(t, err)
}

// ============ UpdateDeviceConfig Tests ============

func TestUpdateDeviceConfig(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_cfg_001", Email: "cfg@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	device := &Device{ID: "dev_cfg_001", UserID: user.ID, Name: "Config Device", APIKey: "sk_cfg_001", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(device))

	config := map[string]interface{}{
		"report_interval": 60,
		"led_enabled":     true,
		"mode":            "eco",
	}

	err := s.UpdateDeviceConfig(device.ID, config)
	require.NoError(t, err)

	retrieved, err := s.GetDevice(device.ID)
	require.NoError(t, err)
	assert.Equal(t, float64(60), retrieved.DesiredState["report_interval"])
	assert.Equal(t, "eco", retrieved.DesiredState["mode"])
}

func TestUpdateDeviceConfig_DeviceNotFound(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	err := s.UpdateDeviceConfig("dev_nonexistent", map[string]interface{}{"key": "val"})
	assert.Error(t, err)
}

// ============ UpdateDeviceLastSeen Tests ============

func TestUpdateDeviceLastSeen(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_ls_001", Email: "ls@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	device := &Device{ID: "dev_ls_001", UserID: user.ID, Name: "LS Device", APIKey: "sk_ls_001", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(device))

	before := time.Now()
	err := s.UpdateDeviceLastSeen(device.ID)
	require.NoError(t, err)

	retrieved, err := s.GetDevice(device.ID)
	require.NoError(t, err)
	assert.True(t, retrieved.LastSeen.After(before) || retrieved.LastSeen.Equal(before))
}

func TestUpdateDeviceLastSeen_DeviceNotFound(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	err := s.UpdateDeviceLastSeen("dev_nonexistent")
	assert.Error(t, err)
}

// ============ GetAllDevices Tests ============

func TestGetAllDevices_Empty(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	devices, err := s.GetAllDevices()
	require.NoError(t, err)
	assert.Empty(t, devices)
}

func TestGetAllDevices(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_all_001", Email: "all@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	user2 := &User{ID: "usr_all_002", Email: "all2@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user2))

	for i := 1; i <= 3; i++ {
		d := &Device{
			ID:        fmt.Sprintf("dev_all_%03d", i),
			UserID:    user.ID,
			Name:      fmt.Sprintf("Device %d", i),
			APIKey:    fmt.Sprintf("sk_all_%03d", i),
			Status:    "active",
			CreatedAt: time.Now(),
		}
		require.NoError(t, s.CreateDevice(d))
	}

	d4 := &Device{ID: "dev_all_004", UserID: user2.ID, Name: "Device 4", APIKey: "sk_all_004", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(d4))

	devices, err := s.GetAllDevices()
	require.NoError(t, err)
	assert.Len(t, devices, 4)
}

// ============ UpdateDeviceAPIKey Tests ============

func TestUpdateDeviceAPIKey(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_ak_001", Email: "ak@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	device := &Device{ID: "dev_ak_001", UserID: user.ID, Name: "AK Device", APIKey: "sk_old_key_001", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(device))

	err := s.UpdateDeviceAPIKey(device.ID, "sk_new_key_999")
	require.NoError(t, err)

	retrieved, err := s.GetDevice(device.ID)
	require.NoError(t, err)
	assert.Equal(t, "sk_new_key_999", retrieved.APIKey)
}

func TestUpdateDeviceAPIKey_DeviceNotFound(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	err := s.UpdateDeviceAPIKey("dev_nonexistent", "sk_new_key")
	assert.Error(t, err)
}

// ============ GetDeviceByUID Tests ============

func TestGetDeviceByUID(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_uid_001", Email: "uid@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	device := &Device{
		ID:        "dev_uid_001",
		UserID:    user.ID,
		Name:      "UID Device",
		APIKey:    "sk_uid_001",
		DeviceUID: "AA:BB:CC:DD:EE:FF",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	require.NoError(t, s.CreateDevice(device))

	retrieved, err := s.GetDeviceByUID("AA:BB:CC:DD:EE:FF")
	require.NoError(t, err)
	assert.Equal(t, device.ID, retrieved.ID)
	assert.Equal(t, device.DeviceUID, retrieved.DeviceUID)
}

func TestGetDeviceByUID_NotFound(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	_, err := s.GetDeviceByUID("11:22:33:44:55:66")
	assert.Error(t, err)
}

func TestGetDeviceByUID_NoUID(t *testing.T) {
	s, cleanup := createTestStorage(t)
	defer cleanup()

	user := &User{ID: "usr_uid_002", Email: "uid2@example.com", Role: "user", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateUser(user))

	// Device without UID - should not match
	device := &Device{ID: "dev_uid_002", UserID: user.ID, Name: "No UID Device", APIKey: "sk_uid_002", Status: "active", CreatedAt: time.Now()}
	require.NoError(t, s.CreateDevice(device))

	_, err := s.GetDeviceByUID("AA:BB:CC:DD:EE:FF")
	assert.Error(t, err)
}
