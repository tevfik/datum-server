package storage

import (
	"log"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"
)

// GetRetentionConfigFromEnv reads retention settings from environment variables
// RETENTION_MAX_DAYS - days to keep data (default: 7)
// RETENTION_CHECK_HOURS - hours between cleanup runs (default: 1)
func GetRetentionConfigFromEnv() RetentionConfig {
	config := DefaultRetention

	if maxDays := os.Getenv("RETENTION_MAX_DAYS"); maxDays != "" {
		if days, err := strconv.Atoi(maxDays); err == nil && days > 0 {
			config.MaxAge = time.Duration(days) * 24 * time.Hour
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
	CleanupEvery time.Duration // Run cleanup every N duration
}

// DefaultRetention - 7 days, cleanup every hour
var DefaultRetention = RetentionConfig{
	MaxAge:       7 * 24 * time.Hour,
	CleanupEvery: 1 * time.Hour,
}

// StartRetentionWorker runs a background goroutine that cleans old data
func (s *Storage) StartRetentionWorker(config RetentionConfig, dataPath string) {
	go func() {
		log.Printf("🧹 Retention worker started: delete data older than %v, check every %v",
			config.MaxAge, config.CleanupEvery)

		ticker := time.NewTicker(config.CleanupEvery)
		defer ticker.Stop()

		// Run once immediately
		s.cleanupOldPartitions(dataPath, config.MaxAge)

		for range ticker.C {
			s.cleanupOldPartitions(dataPath, config.MaxAge)
		}
	}()
}

// cleanupOldPartitions removes tstorage partitions older than maxAge
func (s *Storage) cleanupOldPartitions(dataPath string, maxAge time.Duration) {
	cutoff := time.Now().Add(-maxAge).Unix()
	deletedCount := 0

	entries, err := os.ReadDir(dataPath)
	if err != nil {
		return
	}

	for _, entry := range entries {
		if !entry.IsDir() || !strings.HasPrefix(entry.Name(), "p-") {
			continue
		}

		// Parse partition name: p-{startTs}-{endTs}
		parts := strings.Split(entry.Name(), "-")
		if len(parts) != 3 {
			continue
		}

		endTs, err := strconv.ParseInt(parts[2], 10, 64)
		if err != nil {
			continue
		}

		// If partition's end timestamp is before cutoff, delete it
		if endTs < cutoff {
			partitionPath := filepath.Join(dataPath, entry.Name())
			if err := os.RemoveAll(partitionPath); err != nil {
				log.Printf("⚠️ Failed to delete partition %s: %v", entry.Name(), err)
			} else {
				deletedCount++
			}
		}
	}

	if deletedCount > 0 {
		log.Printf("🧹 Retention: deleted %d old partition(s)", deletedCount)
	}
}

// CleanupNow forces an immediate cleanup (for testing)
func (s *Storage) CleanupNow(dataPath string, maxAge time.Duration) int {
	cutoff := time.Now().Add(-maxAge).Unix()
	deletedCount := 0

	entries, err := os.ReadDir(dataPath)
	if err != nil {
		return 0
	}

	for _, entry := range entries {
		if !entry.IsDir() || !strings.HasPrefix(entry.Name(), "p-") {
			continue
		}

		parts := strings.Split(entry.Name(), "-")
		if len(parts) != 3 {
			continue
		}

		endTs, err := strconv.ParseInt(parts[2], 10, 64)
		if err != nil {
			continue
		}

		if endTs < cutoff {
			partitionPath := filepath.Join(dataPath, entry.Name())
			if err := os.RemoveAll(partitionPath); err == nil {
				deletedCount++
			}
		}
	}

	return deletedCount
}
