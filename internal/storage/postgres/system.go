package postgres

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"time"

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
	config := &storage.SystemConfig{
		Initialized:   true,
		SetupAt:       time.Now(),
		PlatformName:  platformName,
		AllowRegister: allowRegister,
		DataRetention: retention,
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
