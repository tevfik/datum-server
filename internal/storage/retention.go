package storage

import (
	"os"
	"strconv"
	"time"
)

// GetRetentionConfigFromEnv reads retention settings from environment variables
// RETENTION_MAX_DAYS - days to keep data (default: 7)
// PUBLIC_DATA_RETENTION_DAYS - days to keep public data (default: 1)
// RETENTION_CHECK_HOURS - hours between cleanup runs (default: 1)
func GetRetentionConfigFromEnv() RetentionConfig {
	config := DefaultRetention

	if maxDays := os.Getenv("RETENTION_MAX_DAYS"); maxDays != "" {
		if days, err := strconv.Atoi(maxDays); err == nil {
			if days <= 0 {
				// 0 or negative means infinite retention (set to 100 years)
				// tstorage might interpret 0 as "expire immediately", so we use a large value
				config.MaxAge = 100 * 365 * 24 * time.Hour
			} else {
				config.MaxAge = time.Duration(days) * 24 * time.Hour
			}
		}
	}

	if publicDays := os.Getenv("PUBLIC_DATA_RETENTION_DAYS"); publicDays != "" {
		if days, err := strconv.Atoi(publicDays); err == nil && days > 0 {
			config.PublicMaxAge = time.Duration(days) * 24 * time.Hour
		}
	}

	if checkHours := os.Getenv("RETENTION_CHECK_HOURS"); checkHours != "" {
		if hours, err := strconv.Atoi(checkHours); err == nil && hours > 0 {
			config.CleanupEvery = time.Duration(hours) * time.Hour
		}
	}

	return config
}

// RetentionConfig defines data retention settings
type RetentionConfig struct {
	MaxAge       time.Duration // Delete data older than this
	PublicMaxAge time.Duration // Delete public data older than this
	CleanupEvery time.Duration // Run cleanup every N duration
}

// DefaultRetention - 7 days, 1 day for public, cleanup every hour
var DefaultRetention = RetentionConfig{
	MaxAge:       7 * 24 * time.Hour,
	PublicMaxAge: 24 * time.Hour,
	CleanupEvery: 1 * time.Hour,
}
