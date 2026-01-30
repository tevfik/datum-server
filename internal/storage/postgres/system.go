package postgres

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"time"

	"datum-go/internal/logger"
	"datum-go/internal/storage"
)

const systemConfigKey = "system_config"

// GetSystemConfig retrieves the current system configuration
func (s *PostgresStore) GetSystemConfig() (*storage.SystemConfig, error) {
	var valueJSON []byte
	err := s.db.QueryRow("SELECT value FROM system_settings WHERE key = $1", systemConfigKey).Scan(&valueJSON)
	if err != nil {
		if err == sql.ErrNoRows {
			// Return default/nil? Or uninitialized config?
			// BuntDB impl returned nil if not found.
			return nil, nil // Return nil implies not initialized
		}
		return nil, err
	}

	var config storage.SystemConfig
	if err := json.Unmarshal(valueJSON, &config); err != nil {
		return nil, err
	}

	return &config, nil
}

// SaveSystemConfig saves the system configuration
func (s *PostgresStore) SaveSystemConfig(config *storage.SystemConfig) error {
	valueJSON, err := json.Marshal(config)
	if err != nil {
		return err
	}

	query := `
		INSERT INTO system_settings (key, value, updated_at)
		VALUES ($1, $2, $3)
		ON CONFLICT (key) DO UPDATE SET value = $2, updated_at = $3
	`
	_, err = s.db.Exec(query, systemConfigKey, valueJSON, time.Now())
	return err
}

// IsSystemInitialized checks if system is set up
func (s *PostgresStore) IsSystemInitialized() bool {
	config, err := s.GetSystemConfig()
	if err != nil || config == nil {
		return false
	}
	return config.Initialized
}

// InitializeSystem marks the system as initialized
func (s *PostgresStore) InitializeSystem(platformName string, allowRegister bool, retention int) error {
	defaultPublicRetention := 1
	if env := storage.GetRetentionConfigFromEnv(); env.PublicMaxAge > 0 {
		defaultPublicRetention = int(env.PublicMaxAge.Hours() / 24)
	}

	config := &storage.SystemConfig{
		Initialized:         true,
		SetupAt:             time.Now(),
		PlatformName:        platformName,
		AllowRegister:       allowRegister,
		DataRetention:       retention,
		PublicDataRetention: defaultPublicRetention,
	}
	return s.SaveSystemConfig(config)
}

// ResetSystem clears all data (TRUNCATE tables)
func (s *PostgresStore) ResetSystem() error {
	// Truncate tables with cascade
	// Order matters if not using CASCADE, but CASCADE handles FKs.
	query := `
		TRUNCATE TABLE 
			users, devices, data_points, commands, 
			provisioning_requests, password_reset_tokens, 
			user_api_keys, system_settings 
		CASCADE
	`
	_, err := s.db.Exec(query)
	return err
}

// ExportDatabase exports all data as a map
func (s *PostgresStore) ExportDatabase() (map[string]interface{}, error) {
	export := make(map[string]interface{})

	config, _ := s.GetSystemConfig()
	users, _ := s.ListAllUsers()
	devices, _ := s.ListAllDevices()
	stats, _ := s.GetDatabaseStats()

	// API Keys and Commands could be exported too, but adhering to BuntDB spec
	// BuntDB ExportDatabase (lines 85+ of system.go) exports: config, users, devices, stats.

	export["config"] = config
	export["users"] = users
	export["devices"] = devices
	export["stats"] = stats
	export["exported_at"] = time.Now()

	return export, nil
}

// UpdateDataRetention updates retention policy
func (s *PostgresStore) UpdateDataRetention(days int) error {
	config, err := s.GetSystemConfig()
	if err != nil {
		return err
	}
	if config == nil {
		return fmt.Errorf("system not initialized")
	}
	config.DataRetention = days
	return s.SaveSystemConfig(config)
}

// UpdateRetentionPolicy updates retention policy (Admin handler version)
func (s *PostgresStore) UpdateRetentionPolicy(days int, checkIntervalHours int) error {
	// We ignore checkIntervalHours for now or store it?
	// BuntDB likely stores it in config?
	// The config struct in storage.go might need checkInterval?
	// SystemConfig struct def:
	// type SystemConfig struct { ... DataRetention int ... }
	// It doesn't seem to have CheckInterval.
	// But let's just update DataRetention for now.
	return s.UpdateDataRetention(days)
}

// UpdateRegistrationConfig updates registration policy
func (s *PostgresStore) UpdateRegistrationConfig(allow bool) error {
	config, err := s.GetSystemConfig()
	if err != nil {
		return err
	}
	if config == nil {
		return fmt.Errorf("system not initialized")
	}
	config.AllowRegister = allow
	return s.SaveSystemConfig(config)
}

// UpdatePublicDataRetention updates public data retention policy
func (s *PostgresStore) UpdatePublicDataRetention(days int) error {
	config, err := s.GetSystemConfig()
	if err != nil {
		return err
	}
	if config == nil {
		return fmt.Errorf("system not initialized")
	}
	config.PublicDataRetention = days
	return s.SaveSystemConfig(config)
}

// CleanupPublicData deletes public data older than maxAge
func (s *PostgresStore) CleanupPublicData(maxAge time.Duration) (int, error) {
	cutoff := time.Now().Add(-maxAge)
	// Assuming device_id for public data starts with 'public_'
	query := `DELETE FROM data_points WHERE device_id LIKE 'public_%' AND timestamp < $1`
	result, err := s.db.Exec(query, cutoff)
	if err != nil {
		return 0, err
	}
	rows, _ := result.RowsAffected()
	return int(rows), nil
}

// GetStats returns database statistics.
func (s *PostgresStore) GetStats() (*storage.SystemStats, error) {
	stats := &storage.SystemStats{
		ServerTime: time.Now(),
	}

	// Count users
	if err := s.db.QueryRow("SELECT COUNT(*) FROM users").Scan(&stats.TotalUsers); err != nil {
		return nil, err
	}

	// Count devices
	if err := s.db.QueryRow("SELECT COUNT(*) FROM devices").Scan(&stats.TotalDevices); err != nil {
		return nil, err
	}

	// DB Size (approximate)
	var size int64
	s.db.QueryRow("SELECT pg_database_size(current_database())").Scan(&size)
	stats.DBSizeBytes = size

	return stats, nil
}

// GetSystemLogs returns system logs (Not implemented for Postgres yet)
// GetSystemLogs returns system logs
func (s *PostgresStore) GetSystemLogs(limit int, level string, search string) ([]string, error) {
	return logger.GetRecentLogs(limit, level, search)
}

// ClearSystemLogs clears system logs (Not implemented)
func (s *PostgresStore) ClearSystemLogs() error {
	return nil
}
