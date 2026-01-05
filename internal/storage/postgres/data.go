package postgres

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"time"

	"datum-go/internal/storage"
)

// StoreData stores data points and updates device shadow
func (s *PostgresStore) StoreData(point *storage.DataPoint) error {
	tx, err := s.db.Begin()
	if err != nil {
		return err
	}
	defer tx.Rollback()

	// 1. Insert into Time-Series (data_points)
	jsonData, err := json.Marshal(point.Data)
	if err != nil {
		return err
	}

	_, err = tx.Exec(`
		INSERT INTO data_points (time, device_id, data)
		VALUES ($1, $2, $3)
	`, point.Timestamp, point.DeviceID, jsonData)
	if err != nil {
		return fmt.Errorf("failed to insert data point: %w", err)
	}

	// 2. Update Device Shadow (Latest State) using JSONB merge
	// shadow_state = shadow_state || new_data
	// COALESCE(shadow_state, '{}'::jsonb) handles nulls
	_, err = tx.Exec(`
		UPDATE devices 
		SET 
			shadow_state = COALESCE(shadow_state, '{}'::jsonb) || $1,
			last_seen = $2,
			updated_at = $2
		WHERE id = $3
	`, jsonData, time.Now(), point.DeviceID)
	if err != nil {
		return fmt.Errorf("failed to update device shadow: %w", err)
	}

	return tx.Commit()
}

// GetLatestData retrieves the merged shadow state
func (s *PostgresStore) GetLatestData(deviceID string) (*storage.DataPoint, error) {
	var shadowJSON []byte
	var lastSeen time.Time

	err := s.db.QueryRow(`
		SELECT shadow_state, last_seen FROM devices WHERE id = $1
	`, deviceID).Scan(&shadowJSON, &lastSeen)

	if err != nil {
		if err == sql.ErrNoRows {
			return nil, fmt.Errorf("no data found")
		}
		return nil, err
	}

	if shadowJSON == nil {
		return nil, fmt.Errorf("no shadow data found")
	}

	var data map[string]interface{}
	if err := json.Unmarshal(shadowJSON, &data); err != nil {
		return nil, err
	}

	return &storage.DataPoint{
		DeviceID:  deviceID,
		Timestamp: lastSeen, // Approximation if 'timestamp' key missing in JSON
		Data:      data,
	}, nil
}

// GetDataHistory retrieving history
func (s *PostgresStore) GetDataHistory(deviceID string, limit int) ([]storage.DataPoint, error) {
	// Default to last 7 days if no range specified logic needed?
	// Interface definition: GetLatestData(deviceID string) vs GetDataHistory(deviceID string, limit int)

	query := `
		SELECT time, data 
		FROM data_points 
		WHERE device_id = $1 
		ORDER BY time DESC 
		LIMIT $2
	`
	rows, err := s.db.Query(query, deviceID, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var points []storage.DataPoint
	for rows.Next() {
		var t time.Time
		var dataJSON []byte
		if err := rows.Scan(&t, &dataJSON); err != nil {
			return nil, err
		}

		var data map[string]interface{}
		json.Unmarshal(dataJSON, &data)

		points = append(points, storage.DataPoint{
			DeviceID:  deviceID,
			Timestamp: t,
			Data:      data,
		})
	}
	return points, nil
}

// GetDataHistoryWithRange retrieves historical data with time range filtering
func (s *PostgresStore) GetDataHistoryWithRange(deviceID string, start, end time.Time, limit int) ([]storage.DataPoint, error) {
	query := `
		SELECT time, data 
		FROM data_points 
		WHERE device_id = $1 AND time >= $2 AND time <= $3
		ORDER BY time DESC 
		LIMIT $4
	`
	rows, err := s.db.Query(query, deviceID, start, end, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var points []storage.DataPoint
	for rows.Next() {
		var t time.Time
		var dataJSON []byte
		if err := rows.Scan(&t, &dataJSON); err != nil {
			return nil, err
		}

		var data map[string]interface{}
		json.Unmarshal(dataJSON, &data)

		points = append(points, storage.DataPoint{
			DeviceID:  deviceID,
			Timestamp: t,
			Data:      data,
		})
	}
	return points, nil
}

// GetDatabaseStats returns storage statistics
func (s *PostgresStore) GetDatabaseStats() (map[string]interface{}, error) {
	stats := make(map[string]interface{})
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	// Parallel queries or simple sequence
	var totalUsers, activeUsers, suspendedUsers, adminUsers int
	s.db.QueryRowContext(ctx, "SELECT COUNT(*) FROM users").Scan(&totalUsers)
	s.db.QueryRowContext(ctx, "SELECT COUNT(*) FROM users WHERE status = 'active' OR status = ''").Scan(&activeUsers)
	s.db.QueryRowContext(ctx, "SELECT COUNT(*) FROM users WHERE status = 'suspended'").Scan(&suspendedUsers)
	s.db.QueryRowContext(ctx, "SELECT COUNT(*) FROM users WHERE role = 'admin'").Scan(&adminUsers)

	var totalDevices, activeDevices, bannedDevices int
	s.db.QueryRowContext(ctx, "SELECT COUNT(*) FROM devices").Scan(&totalDevices)
	s.db.QueryRowContext(ctx, "SELECT COUNT(*) FROM devices WHERE status = 'active' OR status = ''").Scan(&activeDevices)
	s.db.QueryRowContext(ctx, "SELECT COUNT(*) FROM devices WHERE status = 'banned'").Scan(&bannedDevices)

	stats["total_users"] = totalUsers
	stats["active_users"] = activeUsers
	stats["suspended_users"] = suspendedUsers
	stats["admin_users"] = adminUsers
	stats["total_devices"] = totalDevices
	stats["active_devices"] = activeDevices
	stats["banned_devices"] = bannedDevices

	return stats, nil
}
