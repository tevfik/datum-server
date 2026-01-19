package storage

import (
	"encoding/json"
	"time"

	"github.com/tidwall/buntdb"
)

// SystemConfig stores platform-wide configuration
type SystemConfig struct {
	Initialized         bool      `json:"initialized"`
	SetupAt             time.Time `json:"setup_at,omitempty"`
	PlatformName        string    `json:"platform_name"`
	AllowRegister       bool      `json:"allow_register"`        // Public registration toggle
	DataRetention       int       `json:"data_retention"`        // Days to keep data
	PublicDataRetention int       `json:"public_data_retention"` // Days to keep public data
}

const systemConfigKey = "system:config"

// GetSystemConfig retrieves the current system configuration
func (s *Storage) GetSystemConfig() (*SystemConfig, error) {
	var config SystemConfig
	err := s.db.View(func(tx *buntdb.Tx) error {
		data, err := tx.Get(systemConfigKey)
		if err != nil {
			// Return default (uninitialized) config
			return nil
		}
		return json.Unmarshal([]byte(data), &config)
	})

	// Ensure defaults for new fields if not present in DB
	if err == nil {
		if config.PublicDataRetention == 0 {
			config.PublicDataRetention = 1 // Default to 1 day if not set
		}
	}

	return &config, err
}

// SaveSystemConfig saves the system configuration
func (s *Storage) SaveSystemConfig(config *SystemConfig) error {
	return s.db.Update(func(tx *buntdb.Tx) error {
		data, err := json.Marshal(config)
		if err != nil {
			return err
		}
		_, _, err = tx.Set(systemConfigKey, string(data), nil)
		return err
	})
}

// IsSystemInitialized checks if the system has been set up
func (s *Storage) IsSystemInitialized() bool {
	config, err := s.GetSystemConfig()
	if err != nil {
		return false
	}
	return config.Initialized
}

// InitializeSystem marks the system as initialized with the given settings
func (s *Storage) InitializeSystem(platformName string, allowRegister bool, retention int) error {
	defaultPublicRetention := 1
	if env := GetRetentionConfigFromEnv(); env.PublicMaxAge > 0 {
		defaultPublicRetention = int(env.PublicMaxAge.Hours() / 24)
	}

	config := &SystemConfig{
		Initialized:         true,
		SetupAt:             time.Now(),
		PlatformName:        platformName,
		AllowRegister:       allowRegister,
		DataRetention:       retention,
		PublicDataRetention: defaultPublicRetention,
	}
	return s.SaveSystemConfig(config)
}

// ResetSystem clears all data and returns to uninitialized state
func (s *Storage) ResetSystem() error {
	return s.db.Update(func(tx *buntdb.Tx) error {
		// Delete all keys
		var keysToDelete []string
		tx.Ascend("", func(key, value string) bool {
			keysToDelete = append(keysToDelete, key)
			return true
		})

		for _, key := range keysToDelete {
			tx.Delete(key)
		}
		return nil
	})
}

// ExportDatabase exports all data as a map for backup
func (s *Storage) ExportDatabase() (map[string]interface{}, error) {
	export := make(map[string]interface{})

	config, _ := s.GetSystemConfig()
	users, _ := s.ListAllUsers()
	devices, _ := s.ListAllDevices()
	stats, _ := s.GetDatabaseStats()

	export["config"] = config
	export["users"] = users
	export["devices"] = devices
	export["stats"] = stats
	export["exported_at"] = time.Now()

	return export, nil
}

// UpdateDataRetention updates the data retention policy
func (s *Storage) UpdateDataRetention(days int) error {
	config, err := s.GetSystemConfig()
	if err != nil {
		return err
	}
	config.DataRetention = days
	return s.SaveSystemConfig(config)
}

// UpdatePublicDataRetention updates the public data retention policy
func (s *Storage) UpdatePublicDataRetention(days int) error {
	config, err := s.GetSystemConfig()
	if err != nil {
		return err
	}
	config.PublicDataRetention = days
	return s.SaveSystemConfig(config)
}

// UpdateRegistrationConfig updates the registration policy
func (s *Storage) UpdateRegistrationConfig(allow bool) error {
	config, err := s.GetSystemConfig()
	if err != nil {
		return err
	}
	config.AllowRegister = allow
	return s.SaveSystemConfig(config)
}
