package storage

import (
	"os"
	"testing"
	"time"
)

func TestGetRetentionConfigFromEnvDefaults(t *testing.T) {
	os.Unsetenv("RETENTION_MAX_DAYS")
	os.Unsetenv("PUBLIC_DATA_RETENTION_DAYS")
	os.Unsetenv("RETENTION_CHECK_HOURS")

	cfg := GetRetentionConfigFromEnv()

	if cfg.MaxAge != 7*24*time.Hour {
		t.Errorf("expected 7 days, got %v", cfg.MaxAge)
	}
	if cfg.PublicMaxAge != 24*time.Hour {
		t.Errorf("expected 1 day, got %v", cfg.PublicMaxAge)
	}
	if cfg.CleanupEvery != time.Hour {
		t.Errorf("expected 1 hour, got %v", cfg.CleanupEvery)
	}
}

func TestGetRetentionConfigFromEnvOverride(t *testing.T) {
	os.Setenv("RETENTION_MAX_DAYS", "30")
	os.Setenv("PUBLIC_DATA_RETENTION_DAYS", "3")
	os.Setenv("RETENTION_CHECK_HOURS", "6")
	defer func() {
		os.Unsetenv("RETENTION_MAX_DAYS")
		os.Unsetenv("PUBLIC_DATA_RETENTION_DAYS")
		os.Unsetenv("RETENTION_CHECK_HOURS")
	}()

	cfg := GetRetentionConfigFromEnv()

	if cfg.MaxAge != 30*24*time.Hour {
		t.Errorf("expected 30 days, got %v", cfg.MaxAge)
	}
	if cfg.PublicMaxAge != 3*24*time.Hour {
		t.Errorf("expected 3 days, got %v", cfg.PublicMaxAge)
	}
	if cfg.CleanupEvery != 6*time.Hour {
		t.Errorf("expected 6 hours, got %v", cfg.CleanupEvery)
	}
}

func TestGetRetentionZeroDays(t *testing.T) {
	os.Setenv("RETENTION_MAX_DAYS", "0")
	defer os.Unsetenv("RETENTION_MAX_DAYS")

	cfg := GetRetentionConfigFromEnv()
	// 0 means infinite retention (100 years)
	if cfg.MaxAge < 365*24*time.Hour {
		t.Error("0 days should mean very long retention")
	}
}

func TestPurgeOldDataPointsBuntDB(t *testing.T) {
	s, err := New(":memory:", t.TempDir()+"/ts", 24*time.Hour)
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()

	// BuntDB+tstorage: PurgeOldDataPoints is a no-op
	purged, err := s.PurgeOldDataPoints(24 * time.Hour)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if purged != 0 {
		t.Fatalf("expected 0 purged (no-op), got %d", purged)
	}
}
