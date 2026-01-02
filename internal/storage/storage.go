package storage

import (
	"encoding/json"
	"fmt"
	"time"

	"github.com/nakabonne/tstorage"
	"github.com/tidwall/buntdb"
)

// Storage handles both metadata (BuntDB) and time-series data (tstorage)
type Storage struct {
	db *buntdb.DB       // For users, devices, commands
	ts tstorage.Storage // For time-series data points
}

func New(metaPath, dataPath string, retention time.Duration) (*Storage, error) {
	// Open BuntDB for metadata
	db, err := buntdb.Open(metaPath)
	if err != nil {
		return nil, fmt.Errorf("buntdb: %w", err)
	}

	// Open tstorage for time-series data
	ts, err := tstorage.NewStorage(
		tstorage.WithDataPath(dataPath),
		tstorage.WithPartitionDuration(time.Hour), // 1 hour partitions
		tstorage.WithRetention(retention),
		tstorage.WithWriteTimeout(time.Second),
	)
	if err != nil {
		db.Close()
		return nil, fmt.Errorf("tstorage: %w", err)
	}

	return &Storage{db: db, ts: ts}, nil
}

func (s *Storage) Close() error {
	if err := s.ts.Close(); err != nil {
		return err
	}
	return s.db.Close()
}

// ============ User operations (BuntDB) ============

type User struct {
	ID           string    `json:"id"`
	Email        string    `json:"email"`
	PasswordHash string    `json:"password_hash"`
	Role         string    `json:"role"`   // "admin", "user"
	Status       string    `json:"status"` // "active", "suspended", "pending"
	CreatedAt    time.Time `json:"created_at"`
	UpdatedAt    time.Time `json:"updated_at,omitempty"`
	LastLoginAt  time.Time `json:"last_login_at,omitempty"`
}

func (s *Storage) CreateUser(user *User) error {
	return s.db.Update(func(tx *buntdb.Tx) error {
		emailKey := fmt.Sprintf("user:email:%s", user.Email)
		if _, err := tx.Get(emailKey); err == nil {
			return fmt.Errorf("user already exists")
		}

		userKey := fmt.Sprintf("user:%s", user.ID)
		userData, _ := json.Marshal(user)
		tx.Set(userKey, string(userData), nil)
		tx.Set(emailKey, user.ID, nil)
		return nil
	})
}

func (s *Storage) GetUserByEmail(email string) (*User, error) {
	var user User
	err := s.db.View(func(tx *buntdb.Tx) error {
		emailKey := fmt.Sprintf("user:email:%s", email)
		userID, err := tx.Get(emailKey)
		if err != nil {
			return err
		}

		userKey := fmt.Sprintf("user:%s", userID)
		userData, err := tx.Get(userKey)
		if err != nil {
			return err
		}
		return json.Unmarshal([]byte(userData), &user)
	})
	return &user, err
}

func (s *Storage) GetUserByID(userID string) (*User, error) {
	var user User
	err := s.db.View(func(tx *buntdb.Tx) error {
		userKey := fmt.Sprintf("user:%s", userID)
		userData, err := tx.Get(userKey)
		if err != nil {
			return err
		}
		return json.Unmarshal([]byte(userData), &user)
	})
	return &user, err
}

// ============ Device operations (BuntDB) ============

type Device struct {
	ID        string    `json:"id"`
	UserID    string    `json:"user_id"`
	Name      string    `json:"name"`
	Type      string    `json:"type"`
	DeviceUID string    `json:"device_uid,omitempty"` // Hardware UID
	APIKey    string    `json:"api_key"`              // Legacy API key (for backward compatibility)
	Status    string    `json:"status"`               // "active", "banned", "suspended", "revoked"
	LastSeen  time.Time `json:"last_seen"`
	CreatedAt time.Time `json:"created_at"`
	UpdatedAt time.Time `json:"updated_at,omitempty"`

	// Token-based authentication (Hybrid SAS system)
	MasterSecret   string    `json:"master_secret,omitempty"`    // Device's master secret for token generation
	CurrentToken   string    `json:"current_token,omitempty"`    // Current active token
	PreviousToken  string    `json:"previous_token,omitempty"`   // Previous token (valid during grace period)
	TokenIssuedAt  time.Time `json:"token_issued_at,omitempty"`  // When current token was issued
	TokenExpiresAt time.Time `json:"token_expires_at,omitempty"` // When current token expires
	GracePeriodEnd time.Time `json:"grace_period_end,omitempty"` // When previous token becomes invalid
	KeyRevokedAt   time.Time `json:"key_revoked_at,omitempty"`   // When keys were revoked (if revoked)
}

func (s *Storage) CreateDevice(device *Device) error {
	return s.db.Update(func(tx *buntdb.Tx) error {
		deviceKey := fmt.Sprintf("device:%s", device.ID)

		// Check if device already exists
		_, err := tx.Get(deviceKey)
		if err == nil {
			// Device exists
			return fmt.Errorf("device with ID '%s' already exists", device.ID)
		}
		if err != buntdb.ErrNotFound {
			// Some other error occurred
			return err
		}

		deviceData, _ := json.Marshal(device)
		tx.Set(deviceKey, string(deviceData), nil)

		// Add to user's device list
		userDevicesKey := fmt.Sprintf("user:%s:devices", device.UserID)
		devicesJSON, err := tx.Get(userDevicesKey)
		var devices []string
		if err == nil {
			json.Unmarshal([]byte(devicesJSON), &devices)
		}
		devices = append(devices, device.ID)
		devicesData, _ := json.Marshal(devices)
		tx.Set(userDevicesKey, string(devicesData), nil)

		// Index by API key
		apiKeyIndex := fmt.Sprintf("apikey:%s", device.APIKey)
		tx.Set(apiKeyIndex, device.ID, nil)
		return nil
	})
}

func (s *Storage) GetDevice(deviceID string) (*Device, error) {
	var device Device
	err := s.db.View(func(tx *buntdb.Tx) error {
		deviceKey := fmt.Sprintf("device:%s", deviceID)
		deviceData, err := tx.Get(deviceKey)
		if err != nil {
			return err
		}
		return json.Unmarshal([]byte(deviceData), &device)
	})
	return &device, err
}

func (s *Storage) GetDeviceByAPIKey(apiKey string) (*Device, error) {
	var device Device
	err := s.db.View(func(tx *buntdb.Tx) error {
		apiKeyIndex := fmt.Sprintf("apikey:%s", apiKey)
		deviceID, err := tx.Get(apiKeyIndex)
		if err != nil {
			return err
		}

		deviceKey := fmt.Sprintf("device:%s", deviceID)
		deviceData, err := tx.Get(deviceKey)
		if err != nil {
			return err
		}
		return json.Unmarshal([]byte(deviceData), &device)
	})
	return &device, err
}

func (s *Storage) GetUserDevices(userID string) ([]Device, error) {
	var devices []Device
	err := s.db.View(func(tx *buntdb.Tx) error {
		userDevicesKey := fmt.Sprintf("user:%s:devices", userID)
		devicesJSON, err := tx.Get(userDevicesKey)
		if err != nil {
			return nil
		}

		var deviceIDs []string
		json.Unmarshal([]byte(devicesJSON), &deviceIDs)

		for _, deviceID := range deviceIDs {
			deviceKey := fmt.Sprintf("device:%s", deviceID)
			deviceData, err := tx.Get(deviceKey)
			if err != nil {
				continue
			}
			var device Device
			json.Unmarshal([]byte(deviceData), &device)
			devices = append(devices, device)
		}
		return nil
	})
	return devices, err
}

func (s *Storage) DeleteDevice(deviceID, userID string) error {
	return s.db.Update(func(tx *buntdb.Tx) error {
		deviceKey := fmt.Sprintf("device:%s", deviceID)
		deviceData, err := tx.Get(deviceKey)
		if err != nil {
			return fmt.Errorf("device not found")
		}

		var device Device
		json.Unmarshal([]byte(deviceData), &device)
		if device.UserID != userID {
			return fmt.Errorf("access denied")
		}

		tx.Delete(deviceKey)
		tx.Delete(fmt.Sprintf("apikey:%s", device.APIKey))

		// Remove from user's device list
		userDevicesKey := fmt.Sprintf("user:%s:devices", userID)
		devicesJSON, _ := tx.Get(userDevicesKey)
		var deviceIDs []string
		json.Unmarshal([]byte(devicesJSON), &deviceIDs)

		var newDevices []string
		for _, id := range deviceIDs {
			if id != deviceID {
				newDevices = append(newDevices, id)
			}
		}
		devicesData, _ := json.Marshal(newDevices)
		tx.Set(userDevicesKey, string(devicesData), nil)
		return nil
	})
}

// ============ Time-series data operations (tstorage) ============

type DataPoint struct {
	DeviceID  string                 `json:"device_id"`
	Timestamp time.Time              `json:"timestamp"`
	Data      map[string]interface{} `json:"data"`
}

// StoreData stores each numeric field as a separate metric
func (s *Storage) StoreData(point *DataPoint) error {
	ts := point.Timestamp.UnixNano()

	var rows []tstorage.Row
	for key, val := range point.Data {
		var floatVal float64
		switch v := val.(type) {
		case float64:
			floatVal = v
		case float32:
			floatVal = float64(v)
		case int:
			floatVal = float64(v)
		case int64:
			floatVal = float64(v)
		default:
			continue // Skip non-numeric values
		}

		metricName := fmt.Sprintf("%s.%s", point.DeviceID, key)
		rows = append(rows, tstorage.Row{
			Metric:    metricName,
			DataPoint: tstorage.DataPoint{Timestamp: ts, Value: floatVal},
		})
	}

	if len(rows) == 0 {
		return nil
	}

	return s.ts.InsertRows(rows)
}

// GetLatestData retrieves the most recent data point for a device
func (s *Storage) GetLatestData(deviceID string) (*DataPoint, error) {
	end := time.Now()
	start := end.Add(-24 * time.Hour) // Last 24 hours

	// Get all metrics for this device
	data := make(map[string]interface{})
	var latestTs int64

	// We need to query each metric separately
	// This is a limitation - we'll query known metrics or scan
	metrics := []string{"temperature", "humidity", "pressure", "battery", "value"}

	for _, metric := range metrics {
		metricName := fmt.Sprintf("%s.%s", deviceID, metric)
		points, err := s.ts.Select(metricName, nil, start.UnixNano(), end.UnixNano())
		if err != nil || len(points) == 0 {
			continue
		}

		// Get most recent
		latest := points[len(points)-1]
		data[metric] = latest.Value
		if latest.Timestamp > latestTs {
			latestTs = latest.Timestamp
		}
	}

	if len(data) == 0 {
		return nil, fmt.Errorf("no data found")
	}

	return &DataPoint{
		DeviceID:  deviceID,
		Timestamp: time.Unix(0, latestTs),
		Data:      data,
	}, nil
}

// GetDataHistoryWithRange retrieves historical data with time range filtering
func (s *Storage) GetDataHistoryWithRange(deviceID string, start, end time.Time, limit int) ([]DataPoint, error) {
	// Collect all timestamps and their values
	tsMap := make(map[int64]map[string]float64)
	metrics := []string{"temperature", "humidity", "pressure", "battery", "value"}

	for _, metric := range metrics {
		metricName := fmt.Sprintf("%s.%s", deviceID, metric)
		points, err := s.ts.Select(metricName, nil, start.UnixNano(), end.UnixNano())
		if err != nil || len(points) == 0 {
			continue
		}

		for _, p := range points {
			if tsMap[p.Timestamp] == nil {
				tsMap[p.Timestamp] = make(map[string]float64)
			}
			tsMap[p.Timestamp][metric] = p.Value
		}
	}

	// Convert to DataPoints, sorted by timestamp descending
	var timestamps []int64
	for ts := range tsMap {
		timestamps = append(timestamps, ts)
	}

	// Sort descending (newest first)
	for i := 0; i < len(timestamps)-1; i++ {
		for j := i + 1; j < len(timestamps); j++ {
			if timestamps[i] < timestamps[j] {
				timestamps[i], timestamps[j] = timestamps[j], timestamps[i]
			}
		}
	}

	var points []DataPoint
	for i, ts := range timestamps {
		if i >= limit {
			break
		}
		data := make(map[string]interface{})
		for k, v := range tsMap[ts] {
			data[k] = v
		}
		points = append(points, DataPoint{
			DeviceID:  deviceID,
			Timestamp: time.Unix(0, ts),
			Data:      data,
		})
	}

	return points, nil
}

// GetDataHistory retrieves historical data points for a device
func (s *Storage) GetDataHistory(deviceID string, limit int) ([]DataPoint, error) {
	end := time.Now()
	start := end.Add(-7 * 24 * time.Hour) // Last 7 days

	// Collect all timestamps and their values
	type tsValue struct {
		metric string
		value  float64
	}

	tsMap := make(map[int64]map[string]float64)
	metrics := []string{"temperature", "humidity", "pressure", "battery", "value"}

	for _, metric := range metrics {
		metricName := fmt.Sprintf("%s.%s", deviceID, metric)
		points, err := s.ts.Select(metricName, nil, start.UnixNano(), end.UnixNano())
		if err != nil || len(points) == 0 {
			continue
		}

		for _, p := range points {
			if tsMap[p.Timestamp] == nil {
				tsMap[p.Timestamp] = make(map[string]float64)
			}
			tsMap[p.Timestamp][metric] = p.Value
		}
	}

	// Convert to DataPoints, sorted by timestamp descending
	var timestamps []int64
	for ts := range tsMap {
		timestamps = append(timestamps, ts)
	}

	// Sort descending (newest first)
	for i := 0; i < len(timestamps)-1; i++ {
		for j := i + 1; j < len(timestamps); j++ {
			if timestamps[i] < timestamps[j] {
				timestamps[i], timestamps[j] = timestamps[j], timestamps[i]
			}
		}
	}

	var points []DataPoint
	for i, ts := range timestamps {
		if i >= limit {
			break
		}
		data := make(map[string]interface{})
		for k, v := range tsMap[ts] {
			data[k] = v
		}
		points = append(points, DataPoint{
			DeviceID:  deviceID,
			Timestamp: time.Unix(0, ts),
			Data:      data,
		})
	}

	return points, nil
}

// ============ Command operations (BuntDB) ============

type Command struct {
	ID        string                 `json:"id"`
	DeviceID  string                 `json:"device_id"`
	Action    string                 `json:"action"`
	Params    map[string]interface{} `json:"params"`
	Status    string                 `json:"status"`
	CreatedAt time.Time              `json:"created_at"`
	ExpiresAt time.Time              `json:"expires_at,omitempty"`
	AckedAt   time.Time              `json:"acked_at,omitempty"`
	Result    map[string]interface{} `json:"result,omitempty"`
}

func (s *Storage) CreateCommand(cmd *Command) error {
	if cmd.ExpiresAt.IsZero() {
		cmd.ExpiresAt = time.Now().Add(24 * time.Hour) // Default 24h TTL
	}

	return s.db.Update(func(tx *buntdb.Tx) error {
		key := fmt.Sprintf("command:%s", cmd.ID)
		data, _ := json.Marshal(cmd)
		tx.Set(key, string(data), nil)

		pendingKey := fmt.Sprintf("device:%s:pending_commands", cmd.DeviceID)
		pendingJSON, err := tx.Get(pendingKey)
		var pending []string
		if err == nil {
			json.Unmarshal([]byte(pendingJSON), &pending)
		}
		pending = append(pending, cmd.ID)
		pendingData, _ := json.Marshal(pending)
		tx.Set(pendingKey, string(pendingData), nil)
		return nil
	})
}

func (s *Storage) GetPendingCommands(deviceID string) ([]Command, error) {
	var commands []Command
	err := s.db.View(func(tx *buntdb.Tx) error {
		pendingKey := fmt.Sprintf("device:%s:pending_commands", deviceID)
		pendingJSON, err := tx.Get(pendingKey)
		if err != nil {
			return nil
		}

		var pending []string
		json.Unmarshal([]byte(pendingJSON), &pending)

		for _, cmdID := range pending {
			cmdKey := fmt.Sprintf("command:%s", cmdID)
			cmdData, err := tx.Get(cmdKey)
			if err != nil {
				continue
			}
			var cmd Command
			json.Unmarshal([]byte(cmdData), &cmd)

			// Check for expiration
			if cmd.Status == "pending" && !cmd.ExpiresAt.IsZero() && time.Now().After(cmd.ExpiresAt) {
				continue // Skip expired commands
			}

			if cmd.Status == "pending" {
				commands = append(commands, cmd)
			}
		}
		return nil
	})
	return commands, err
}

func (s *Storage) AcknowledgeCommand(cmdID string, result map[string]interface{}) error {
	return s.db.Update(func(tx *buntdb.Tx) error {
		cmdKey := fmt.Sprintf("command:%s", cmdID)
		cmdData, err := tx.Get(cmdKey)
		if err != nil {
			return fmt.Errorf("command not found")
		}

		var cmd Command
		json.Unmarshal([]byte(cmdData), &cmd)
		cmd.Status = "acknowledged"
		cmd.AckedAt = time.Now()
		cmd.Result = result

		data, _ := json.Marshal(cmd)
		tx.Set(cmdKey, string(data), nil)

		pendingKey := fmt.Sprintf("device:%s:pending_commands", cmd.DeviceID)
		pendingJSON, _ := tx.Get(pendingKey)
		var pending []string
		json.Unmarshal([]byte(pendingJSON), &pending)

		var newPending []string
		for _, id := range pending {
			if id != cmdID {
				newPending = append(newPending, id)
			}
		}
		pendingData, _ := json.Marshal(newPending)
		tx.Set(pendingKey, string(pendingData), nil)
		return nil
	})
}

func (s *Storage) GetPendingCommandCount(deviceID string) int {
	count := 0
	s.db.View(func(tx *buntdb.Tx) error {
		pendingKey := fmt.Sprintf("device:%s:pending_commands", deviceID)
		pendingJSON, err := tx.Get(pendingKey)
		if err != nil {
			return nil
		}
		var pending []string
		json.Unmarshal([]byte(pendingJSON), &pending)
		count = len(pending)
		return nil
	})
	return count
}

// ============ Admin Management operations ============

// GetUserCount returns total number of users (used to check if system is initialized)
func (s *Storage) GetUserCount() (int, error) {
	count := 0
	err := s.db.View(func(tx *buntdb.Tx) error {
		tx.Ascend("", func(key, value string) bool {
			// Match exactly "user:{id}" pattern (not user:email: or user:{id}:devices)
			if len(key) > 5 && key[:5] == "user:" {
				// Check that there are no more colons after "user:"
				remainingKey := key[5:]
				if !contains(remainingKey, ":") {
					count++
				}
			}
			return true
		})
		return nil
	})
	return count, err
}

// ListAllUsers returns all users in the system
func (s *Storage) ListAllUsers() ([]User, error) {
	var users []User
	err := s.db.View(func(tx *buntdb.Tx) error {
		tx.Ascend("", func(key, value string) bool {
			if len(key) > 5 && key[:5] == "user:" {
				// Skip email indexes and device lists
				if contains(key, ":email:") || contains(key, ":devices") {
					return true
				}
				var user User
				if err := json.Unmarshal([]byte(value), &user); err == nil {
					users = append(users, user)
				}
			}
			return true
		})
		return nil
	})
	return users, err
}

// UpdateUser updates user role and status
func (s *Storage) UpdateUser(userID string, role, status string) error {
	return s.db.Update(func(tx *buntdb.Tx) error {
		userKey := fmt.Sprintf("user:%s", userID)
		userData, err := tx.Get(userKey)
		if err != nil {
			return fmt.Errorf("user not found")
		}

		var user User
		json.Unmarshal([]byte(userData), &user)

		if role != "" {
			user.Role = role
		}
		if status != "" {
			user.Status = status
		}
		user.UpdatedAt = time.Now()

		data, _ := json.Marshal(user)
		tx.Set(userKey, string(data), nil)
		return nil
	})
}

// UpdateUserLastLogin updates the last login timestamp
func (s *Storage) UpdateUserLastLogin(userID string) error {
	return s.db.Update(func(tx *buntdb.Tx) error {
		userKey := fmt.Sprintf("user:%s", userID)
		userData, err := tx.Get(userKey)
		if err != nil {
			return err
		}

		var user User
		json.Unmarshal([]byte(userData), &user)
		user.LastLoginAt = time.Now()

		data, _ := json.Marshal(user)
		tx.Set(userKey, string(data), nil)
		return nil
	})
}

// UpdateUserPassword updates a user's password hash
func (s *Storage) UpdateUserPassword(userID string, passwordHash string) error {
	return s.db.Update(func(tx *buntdb.Tx) error {
		userKey := fmt.Sprintf("user:%s", userID)
		userData, err := tx.Get(userKey)
		if err != nil {
			return err
		}

		var user User
		json.Unmarshal([]byte(userData), &user)
		user.PasswordHash = passwordHash
		user.UpdatedAt = time.Now()

		data, _ := json.Marshal(user)
		tx.Set(userKey, string(data), nil)
		return nil
	})
}

// DeleteUser removes a user and all their devices
func (s *Storage) DeleteUser(userID string) error {
	return s.db.Update(func(tx *buntdb.Tx) error {
		// Get user's devices first
		userDevicesKey := fmt.Sprintf("user:%s:devices", userID)
		devicesJSON, _ := tx.Get(userDevicesKey)
		var deviceIDs []string
		json.Unmarshal([]byte(devicesJSON), &deviceIDs)

		// Delete each device
		for _, deviceID := range deviceIDs {
			deviceKey := fmt.Sprintf("device:%s", deviceID)
			deviceData, _ := tx.Get(deviceKey)
			var device Device
			json.Unmarshal([]byte(deviceData), &device)

			tx.Delete(deviceKey)
			tx.Delete(fmt.Sprintf("apikey:%s", device.APIKey))
		}

		// Delete user device list
		tx.Delete(userDevicesKey)

		// Delete email index
		userKey := fmt.Sprintf("user:%s", userID)
		userData, err := tx.Get(userKey)
		if err != nil {
			return fmt.Errorf("user not found")
		}
		var user User
		json.Unmarshal([]byte(userData), &user)
		tx.Delete(fmt.Sprintf("user:email:%s", user.Email))

		// Delete user
		tx.Delete(userKey)
		return nil
	})
}

// ListAllDevices returns all devices across all users
func (s *Storage) ListAllDevices() ([]Device, error) {
	var devices []Device
	err := s.db.View(func(tx *buntdb.Tx) error {
		tx.Ascend("", func(key, value string) bool {
			if len(key) > 7 && key[:7] == "device:" {
				// Skip command-related keys
				if contains(key, ":pending") || contains(key, ":commands") {
					return true
				}
				var device Device
				if err := json.Unmarshal([]byte(value), &device); err == nil {
					devices = append(devices, device)
				}
			}
			return true
		})
		return nil
	})
	return devices, err
}

// UpdateDevice updates device status (for ban/unban)
func (s *Storage) UpdateDevice(deviceID string, status string) error {
	return s.db.Update(func(tx *buntdb.Tx) error {
		deviceKey := fmt.Sprintf("device:%s", deviceID)
		deviceData, err := tx.Get(deviceKey)
		if err != nil {
			return fmt.Errorf("device not found")
		}

		var device Device
		json.Unmarshal([]byte(deviceData), &device)
		device.Status = status
		device.UpdatedAt = time.Now()

		data, _ := json.Marshal(device)
		tx.Set(deviceKey, string(data), nil)
		return nil
	})
}

// UpdateDeviceLastSeen updates the last seen timestamp of a device
func (s *Storage) UpdateDeviceLastSeen(deviceID string) error {
	return s.db.Update(func(tx *buntdb.Tx) error {
		deviceKey := fmt.Sprintf("device:%s", deviceID)
		deviceData, err := tx.Get(deviceKey)
		if err != nil {
			return fmt.Errorf("device not found")
		}

		var device Device
		json.Unmarshal([]byte(deviceData), &device)
		device.LastSeen = time.Now()
		// We don't update UpdatedAt here to keep it semantic to configuration changes

		data, _ := json.Marshal(device)
		tx.Set(deviceKey, string(data), nil)
		return nil
	})
}

// ForceDeleteDevice deletes a device by admin (bypasses user ownership check)
func (s *Storage) ForceDeleteDevice(deviceID string) error {
	return s.db.Update(func(tx *buntdb.Tx) error {
		deviceKey := fmt.Sprintf("device:%s", deviceID)
		deviceData, err := tx.Get(deviceKey)
		if err != nil {
			return fmt.Errorf("device not found")
		}

		var device Device
		json.Unmarshal([]byte(deviceData), &device)

		tx.Delete(deviceKey)
		tx.Delete(fmt.Sprintf("apikey:%s", device.APIKey))

		// Also delete token index if exists
		if device.CurrentToken != "" {
			tx.Delete(fmt.Sprintf("token:%s", device.CurrentToken))
		}

		// Remove from user's device list
		userDevicesKey := fmt.Sprintf("user:%s:devices", device.UserID)
		devicesJSON, _ := tx.Get(userDevicesKey)
		var deviceIDs []string
		json.Unmarshal([]byte(devicesJSON), &deviceIDs)

		var newDevices []string
		for _, id := range deviceIDs {
			if id != deviceID {
				newDevices = append(newDevices, id)
			}
		}
		devicesData, _ := json.Marshal(newDevices)
		tx.Set(userDevicesKey, string(devicesData), nil)
		return nil
	})
}

// ============ Token Management Operations ============

// RotateDeviceKey generates a new token for a device with grace period for transition
func (s *Storage) RotateDeviceKey(deviceID string, newToken string, tokenExpiresAt time.Time, gracePeriodDays int) (*Device, error) {
	var device *Device
	err := s.db.Update(func(tx *buntdb.Tx) error {
		deviceKey := fmt.Sprintf("device:%s", deviceID)
		deviceData, err := tx.Get(deviceKey)
		if err != nil {
			return fmt.Errorf("device not found")
		}

		var d Device
		json.Unmarshal([]byte(deviceData), &d)

		// Check if device is revoked
		if d.Status == "revoked" {
			return fmt.Errorf("device keys are revoked, cannot rotate")
		}

		// Delete old token index
		if d.CurrentToken != "" {
			tx.Delete(fmt.Sprintf("token:%s", d.CurrentToken))
		}

		// Move current token to previous
		d.PreviousToken = d.CurrentToken
		d.CurrentToken = newToken
		d.TokenIssuedAt = time.Now()
		d.TokenExpiresAt = tokenExpiresAt
		d.GracePeriodEnd = time.Now().Add(time.Duration(gracePeriodDays) * 24 * time.Hour)
		d.UpdatedAt = time.Now()

		// Create new token index
		tx.Set(fmt.Sprintf("token:%s", newToken), deviceID, nil)

		data, _ := json.Marshal(d)
		tx.Set(deviceKey, string(data), nil)
		device = &d
		return nil
	})
	return device, err
}

// RevokeDeviceKey immediately invalidates all tokens for a device
func (s *Storage) RevokeDeviceKey(deviceID string) (*Device, error) {
	var device *Device
	err := s.db.Update(func(tx *buntdb.Tx) error {
		deviceKey := fmt.Sprintf("device:%s", deviceID)
		deviceData, err := tx.Get(deviceKey)
		if err != nil {
			return fmt.Errorf("device not found")
		}

		var d Device
		json.Unmarshal([]byte(deviceData), &d)

		// Delete token indexes
		if d.CurrentToken != "" {
			tx.Delete(fmt.Sprintf("token:%s", d.CurrentToken))
		}
		if d.PreviousToken != "" {
			tx.Delete(fmt.Sprintf("token:%s", d.PreviousToken))
		}

		// Clear tokens and mark as revoked
		d.PreviousToken = ""
		d.CurrentToken = ""
		d.TokenExpiresAt = time.Time{}
		d.GracePeriodEnd = time.Time{}
		d.KeyRevokedAt = time.Now()
		d.Status = "revoked"
		d.UpdatedAt = time.Now()

		data, _ := json.Marshal(d)
		tx.Set(deviceKey, string(data), nil)
		device = &d
		return nil
	})
	return device, err
}

// GetDeviceByToken retrieves a device by its current or previous token
func (s *Storage) GetDeviceByToken(token string) (*Device, bool, error) {
	var device Device
	var isGracePeriod bool

	err := s.db.View(func(tx *buntdb.Tx) error {
		// First try to find by token index
		tokenKey := fmt.Sprintf("token:%s", token)
		deviceID, err := tx.Get(tokenKey)
		if err == nil {
			// Token found in index
			deviceKey := fmt.Sprintf("device:%s", deviceID)
			deviceData, err := tx.Get(deviceKey)
			if err != nil {
				return fmt.Errorf("device not found")
			}
			return json.Unmarshal([]byte(deviceData), &device)
		}

		// Token not in index, check if it's a previous token in grace period
		// We need to scan devices (this is slower but handles grace period)
		var found bool
		tx.Ascend("", func(key, value string) bool {
			if len(key) > 7 && key[:7] == "device:" {
				// Skip command-related keys
				if contains(key, ":pending") || contains(key, ":commands") {
					return true
				}
				var d Device
				if err := json.Unmarshal([]byte(value), &d); err == nil {
					if d.PreviousToken == token && time.Now().Before(d.GracePeriodEnd) {
						device = d
						isGracePeriod = true
						found = true
						return false // Stop iteration
					}
				}
			}
			return true
		})

		if !found {
			return fmt.Errorf("token not found or expired")
		}
		return nil
	})

	return &device, isGracePeriod, err
}

// GetDeviceTokenInfo returns token status information for a device
func (s *Storage) GetDeviceTokenInfo(deviceID string) (map[string]interface{}, error) {
	device, err := s.GetDevice(deviceID)
	if err != nil {
		return nil, err
	}

	info := make(map[string]interface{})
	info["device_id"] = device.ID
	info["has_token"] = device.CurrentToken != ""

	if device.CurrentToken != "" {
		info["token_expires_at"] = device.TokenExpiresAt
		info["token_issued_at"] = device.TokenIssuedAt
		info["needs_refresh"] = time.Until(device.TokenExpiresAt) < 7*24*time.Hour && time.Now().Before(device.TokenExpiresAt)
		info["in_grace_period"] = device.PreviousToken != "" && time.Now().Before(device.GracePeriodEnd)
		if info["in_grace_period"].(bool) {
			info["grace_period_end"] = device.GracePeriodEnd
		}
	}

	if !device.KeyRevokedAt.IsZero() {
		info["revoked"] = true
		info["revoked_at"] = device.KeyRevokedAt
	}

	return info, nil
}

// InitializeDeviceToken sets up initial token for a new device or migrates a legacy device
func (s *Storage) InitializeDeviceToken(deviceID, masterSecret, token string, tokenExpiresAt time.Time) (*Device, error) {
	var device *Device
	err := s.db.Update(func(tx *buntdb.Tx) error {
		deviceKey := fmt.Sprintf("device:%s", deviceID)
		deviceData, err := tx.Get(deviceKey)
		if err != nil {
			return fmt.Errorf("device not found")
		}

		var d Device
		json.Unmarshal([]byte(deviceData), &d)

		// Set up token system
		d.MasterSecret = masterSecret
		d.CurrentToken = token
		d.TokenIssuedAt = time.Now()
		d.TokenExpiresAt = tokenExpiresAt
		d.UpdatedAt = time.Now()

		// Create token index
		tx.Set(fmt.Sprintf("token:%s", token), deviceID, nil)

		data, _ := json.Marshal(d)
		tx.Set(deviceKey, string(data), nil)
		device = &d
		return nil
	})
	return device, err
}

// CleanupExpiredGracePeriods removes previous tokens for devices past grace period
func (s *Storage) CleanupExpiredGracePeriods() (int, error) {
	count := 0
	err := s.db.Update(func(tx *buntdb.Tx) error {
		var devicesToUpdate []Device
		now := time.Now()

		tx.Ascend("", func(key, value string) bool {
			if len(key) > 7 && key[:7] == "device:" {
				if contains(key, ":pending") || contains(key, ":commands") {
					return true
				}
				var d Device
				if err := json.Unmarshal([]byte(value), &d); err == nil {
					// Check if grace period has expired and previous token exists
					if d.PreviousToken != "" && now.After(d.GracePeriodEnd) {
						devicesToUpdate = append(devicesToUpdate, d)
					}
				}
			}
			return true
		})

		for _, d := range devicesToUpdate {
			// Clear previous token
			if d.PreviousToken != "" {
				tx.Delete(fmt.Sprintf("token:%s", d.PreviousToken))
			}
			d.PreviousToken = ""
			d.GracePeriodEnd = time.Time{}
			d.UpdatedAt = now

			deviceKey := fmt.Sprintf("device:%s", d.ID)
			data, _ := json.Marshal(d)
			tx.Set(deviceKey, string(data), nil)
			count++
		}

		return nil
	})
	return count, err
}

// GetDatabaseStats returns storage statistics
func (s *Storage) GetDatabaseStats() (map[string]interface{}, error) {
	stats := make(map[string]interface{})

	userCount, _ := s.GetUserCount()
	users, _ := s.ListAllUsers()
	devices, _ := s.ListAllDevices()

	// Count by status
	activeUsers := 0
	suspendedUsers := 0
	adminUsers := 0
	for _, u := range users {
		if u.Status == "active" || u.Status == "" {
			activeUsers++
		} else if u.Status == "suspended" {
			suspendedUsers++
		}
		if u.Role == "admin" {
			adminUsers++
		}
	}

	activeDevices := 0
	bannedDevices := 0
	for _, d := range devices {
		if d.Status == "active" || d.Status == "" {
			activeDevices++
		} else if d.Status == "banned" {
			bannedDevices++
		}
	}

	stats["total_users"] = userCount
	stats["active_users"] = activeUsers
	stats["suspended_users"] = suspendedUsers
	stats["admin_users"] = adminUsers
	stats["total_devices"] = len(devices)
	stats["active_devices"] = activeDevices
	stats["banned_devices"] = bannedDevices

	return stats, nil
}

// Helper function to check if string contains substring
func contains(s, substr string) bool {
	return len(s) >= len(substr) && (s == substr || len(s) > 0 && containsHelper(s, substr))
}

func containsHelper(s, substr string) bool {
	for i := 0; i <= len(s)-len(substr); i++ {
		if s[i:i+len(substr)] == substr {
			return true
		}
	}
	return false
}

// ============ Provisioning operations (BuntDB) ============

// ProvisioningRequest represents a pending device registration request
type ProvisioningRequest struct {
	ID          string    `json:"id"`          // Unique request ID
	DeviceUID   string    `json:"device_uid"`  // Hardware unique ID (MAC, chip ID)
	UserID      string    `json:"user_id"`     // User who initiated the request
	DeviceName  string    `json:"device_name"` // User-provided device name
	DeviceType  string    `json:"device_type"` // Device type/model
	Status      string    `json:"status"`      // pending, completed, expired, cancelled
	DeviceID    string    `json:"device_id"`   // Assigned device ID (after completion)
	APIKey      string    `json:"api_key"`     // Generated API key (after completion)
	ServerURL   string    `json:"server_url"`  // Server URL for device
	WiFiSSID    string    `json:"wifi_ssid"`   // WiFi credentials (optional)
	WiFiPass    string    `json:"wifi_pass"`   // WiFi password (optional)
	ExpiresAt   time.Time `json:"expires_at"`  // Request expiration time
	CreatedAt   time.Time `json:"created_at"`
	CompletedAt time.Time `json:"completed_at,omitempty"`
}

// CreateProvisioningRequest creates a new provisioning request
func (s *Storage) CreateProvisioningRequest(req *ProvisioningRequest) error {
	return s.db.Update(func(tx *buntdb.Tx) error {
		// Check if UID is already registered as a device
		uidDeviceKey := fmt.Sprintf("device:uid:%s", req.DeviceUID)
		if existingDeviceID, err := tx.Get(uidDeviceKey); err == nil {
			return fmt.Errorf("device with UID '%s' is already registered as device '%s'", req.DeviceUID, existingDeviceID)
		}

		// Check if there's already a pending request for this UID
		uidPendingKey := fmt.Sprintf("provision:uid:%s", req.DeviceUID)
		if existingReqID, err := tx.Get(uidPendingKey); err == nil {
			// Check if the existing request is still pending
			existingReqKey := fmt.Sprintf("provision:%s", existingReqID)
			if existingReqData, err := tx.Get(existingReqKey); err == nil {
				var existingReq ProvisioningRequest
				if json.Unmarshal([]byte(existingReqData), &existingReq) == nil {
					if existingReq.Status == "pending" && time.Now().Before(existingReq.ExpiresAt) {
						return fmt.Errorf("a pending provisioning request already exists for UID '%s'", req.DeviceUID)
					}
				}
			}
		}

		// Store the provisioning request
		reqKey := fmt.Sprintf("provision:%s", req.ID)
		reqData, _ := json.Marshal(req)
		tx.Set(reqKey, string(reqData), nil)

		// Index by UID for device lookup
		tx.Set(uidPendingKey, req.ID, nil)

		// Index by user for listing
		userProvKey := fmt.Sprintf("user:%s:provisions", req.UserID)
		provsJSON, _ := tx.Get(userProvKey)
		var provIDs []string
		if provsJSON != "" {
			json.Unmarshal([]byte(provsJSON), &provIDs)
		}
		provIDs = append(provIDs, req.ID)
		provsData, _ := json.Marshal(provIDs)
		tx.Set(userProvKey, string(provsData), nil)

		return nil
	})
}

// GetProvisioningRequestByUID retrieves a pending provisioning request by device UID
func (s *Storage) GetProvisioningRequestByUID(deviceUID string) (*ProvisioningRequest, error) {
	var req ProvisioningRequest
	err := s.db.View(func(tx *buntdb.Tx) error {
		uidPendingKey := fmt.Sprintf("provision:uid:%s", deviceUID)
		reqID, err := tx.Get(uidPendingKey)
		if err != nil {
			return fmt.Errorf("no provisioning request found for UID '%s'", deviceUID)
		}

		reqKey := fmt.Sprintf("provision:%s", reqID)
		reqData, err := tx.Get(reqKey)
		if err != nil {
			return err
		}
		return json.Unmarshal([]byte(reqData), &req)
	})
	return &req, err
}

// GetProvisioningRequest retrieves a provisioning request by ID
func (s *Storage) GetProvisioningRequest(reqID string) (*ProvisioningRequest, error) {
	var req ProvisioningRequest
	err := s.db.View(func(tx *buntdb.Tx) error {
		reqKey := fmt.Sprintf("provision:%s", reqID)
		reqData, err := tx.Get(reqKey)
		if err != nil {
			return err
		}
		return json.Unmarshal([]byte(reqData), &req)
	})
	return &req, err
}

// CompleteProvisioningRequest marks a provisioning request as completed and creates the device
func (s *Storage) CompleteProvisioningRequest(reqID string) (*Device, error) {
	var device *Device
	err := s.db.Update(func(tx *buntdb.Tx) error {
		// Get the request
		reqKey := fmt.Sprintf("provision:%s", reqID)
		reqData, err := tx.Get(reqKey)
		if err != nil {
			return fmt.Errorf("provisioning request not found")
		}

		var req ProvisioningRequest
		if err := json.Unmarshal([]byte(reqData), &req); err != nil {
			return err
		}

		// Check if request is valid
		if req.Status != "pending" {
			return fmt.Errorf("provisioning request is not pending (status: %s)", req.Status)
		}
		if time.Now().After(req.ExpiresAt) {
			req.Status = "expired"
			reqData, _ := json.Marshal(req)
			tx.Set(reqKey, string(reqData), nil)
			return fmt.Errorf("provisioning request has expired")
		}

		// Update request status
		req.Status = "completed"
		req.CompletedAt = time.Now()
		updatedReqData, _ := json.Marshal(req)
		tx.Set(reqKey, string(updatedReqData), nil)

		// Create the device
		device = &Device{
			ID:        req.DeviceID,
			UserID:    req.UserID,
			Name:      req.DeviceName,
			Type:      req.DeviceType,
			DeviceUID: req.DeviceUID,
			APIKey:    req.APIKey,
			Status:    "active",
			CreatedAt: time.Now(),
		}

		deviceKey := fmt.Sprintf("device:%s", device.ID)
		deviceData, _ := json.Marshal(device)
		tx.Set(deviceKey, string(deviceData), nil)

		// Add to user's device list
		userDevicesKey := fmt.Sprintf("user:%s:devices", device.UserID)
		devicesJSON, _ := tx.Get(userDevicesKey)
		var devices []string
		if devicesJSON != "" {
			json.Unmarshal([]byte(devicesJSON), &devices)
		}
		devices = append(devices, device.ID)
		devicesData, _ := json.Marshal(devices)
		tx.Set(userDevicesKey, string(devicesData), nil)

		// Index by API key
		apiKeyIndex := fmt.Sprintf("apikey:%s", device.APIKey)
		tx.Set(apiKeyIndex, device.ID, nil)

		// Index by UID (for preventing duplicate registrations)
		uidDeviceKey := fmt.Sprintf("device:uid:%s", req.DeviceUID)
		tx.Set(uidDeviceKey, device.ID, nil)

		// Remove pending UID index
		uidPendingKey := fmt.Sprintf("provision:uid:%s", req.DeviceUID)
		tx.Delete(uidPendingKey)

		return nil
	})
	return device, err
}

// CancelProvisioningRequest cancels a pending provisioning request
func (s *Storage) CancelProvisioningRequest(reqID string) error {
	return s.db.Update(func(tx *buntdb.Tx) error {
		reqKey := fmt.Sprintf("provision:%s", reqID)
		reqData, err := tx.Get(reqKey)
		if err != nil {
			return fmt.Errorf("provisioning request not found")
		}

		var req ProvisioningRequest
		if err := json.Unmarshal([]byte(reqData), &req); err != nil {
			return err
		}

		if req.Status != "pending" {
			return fmt.Errorf("can only cancel pending requests")
		}

		req.Status = "cancelled"
		cancelledData, _ := json.Marshal(req)
		tx.Set(reqKey, string(cancelledData), nil)

		// Remove pending UID index
		uidPendingKey := fmt.Sprintf("provision:uid:%s", req.DeviceUID)
		tx.Delete(uidPendingKey)

		return nil
	})
}

// GetUserProvisioningRequests returns all provisioning requests for a user
func (s *Storage) GetUserProvisioningRequests(userID string) ([]ProvisioningRequest, error) {
	var requests []ProvisioningRequest
	err := s.db.View(func(tx *buntdb.Tx) error {
		userProvKey := fmt.Sprintf("user:%s:provisions", userID)
		provsJSON, err := tx.Get(userProvKey)
		if err != nil {
			return nil // No requests
		}

		var provIDs []string
		json.Unmarshal([]byte(provsJSON), &provIDs)

		for _, provID := range provIDs {
			reqKey := fmt.Sprintf("provision:%s", provID)
			reqData, err := tx.Get(reqKey)
			if err != nil {
				continue
			}
			var req ProvisioningRequest
			if json.Unmarshal([]byte(reqData), &req) == nil {
				requests = append(requests, req)
			}
		}
		return nil
	})
	return requests, err
}

// IsDeviceUIDRegistered checks if a device UID is already registered
func (s *Storage) IsDeviceUIDRegistered(deviceUID string) (bool, string, error) {
	var deviceID string
	var registered bool
	err := s.db.View(func(tx *buntdb.Tx) error {
		uidDeviceKey := fmt.Sprintf("device:uid:%s", deviceUID)
		id, err := tx.Get(uidDeviceKey)
		if err == nil {
			registered = true
			deviceID = id
		}
		return nil
	})
	return registered, deviceID, err
}

// CleanupExpiredProvisioningRequests removes expired provisioning requests
func (s *Storage) CleanupExpiredProvisioningRequests() (int, error) {
	var cleaned int
	err := s.db.Update(func(tx *buntdb.Tx) error {
		var expiredKeys []string
		tx.Ascend("", func(key, value string) bool {
			if len(key) > 10 && key[:10] == "provision:" && key[10:14] != "uid:" {
				var req ProvisioningRequest
				if json.Unmarshal([]byte(value), &req) == nil {
					if req.Status == "pending" && time.Now().After(req.ExpiresAt) {
						expiredKeys = append(expiredKeys, key)
					}
				}
			}
			return true
		})

		for _, key := range expiredKeys {
			reqData, _ := tx.Get(key)
			var req ProvisioningRequest
			if json.Unmarshal([]byte(reqData), &req) == nil {
				req.Status = "expired"
				newData, _ := json.Marshal(req)
				tx.Set(key, string(newData), nil)

				// Remove UID index
				uidPendingKey := fmt.Sprintf("provision:uid:%s", req.DeviceUID)
				tx.Delete(uidPendingKey)
				cleaned++
			}
		}
		return nil
	})
	return cleaned, err
}
