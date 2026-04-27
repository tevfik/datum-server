package config

import (
	"os"
	"testing"
)

func TestLoadDefaults(t *testing.T) {
	// Clear any env vars that might interfere.
	os.Unsetenv("PORT")
	os.Unsetenv("DATABASE_URL")
	os.Unsetenv("JWT_SECRET")
	os.Unsetenv("DATUM_CONFIG")

	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load() error: %v", err)
	}

	if cfg.Server.Port != 8000 {
		t.Errorf("expected default port 8000, got %d", cfg.Server.Port)
	}
	if cfg.Server.DataDir != "./data" {
		t.Errorf("expected default data_dir ./data, got %s", cfg.Server.DataDir)
	}
	if cfg.Server.EnableHSTS != true {
		t.Error("expected HSTS enabled by default")
	}
	if cfg.Database.MaxOpenConns != 25 {
		t.Errorf("expected 25 max_open_conns, got %d", cfg.Database.MaxOpenConns)
	}
	if cfg.Auth.JWTRefreshExpiryDays != 30 {
		t.Errorf("expected 30 jwt_refresh_expiry_days, got %d", cfg.Auth.JWTRefreshExpiryDays)
	}
	if cfg.Data.RetentionMaxDays != 7 {
		t.Errorf("expected 7 retention_max_days, got %d", cfg.Data.RetentionMaxDays)
	}
	if cfg.Data.TelemetryBufferSize != 10000 {
		t.Errorf("expected 10000 telemetry_buffer_size, got %d", cfg.Data.TelemetryBufferSize)
	}
	if cfg.Logging.Level != "INFO" {
		t.Errorf("expected INFO log level, got %s", cfg.Logging.Level)
	}
}

func TestLoadEnvOverride(t *testing.T) {
	os.Setenv("PORT", "9999")
	os.Setenv("DATA_DIR", "/tmp/test-datum")
	os.Setenv("RETENTION_MAX_DAYS", "30")
	defer func() {
		os.Unsetenv("PORT")
		os.Unsetenv("DATA_DIR")
		os.Unsetenv("RETENTION_MAX_DAYS")
	}()

	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load() error: %v", err)
	}

	if cfg.Server.Port != 9999 {
		t.Errorf("expected port 9999 from env, got %d", cfg.Server.Port)
	}
	if cfg.Server.DataDir != "/tmp/test-datum" {
		t.Errorf("expected /tmp/test-datum, got %s", cfg.Server.DataDir)
	}
	if cfg.Data.RetentionMaxDays != 30 {
		t.Errorf("expected 30, got %d", cfg.Data.RetentionMaxDays)
	}
}

func TestLoadDatabaseURLFromEnv(t *testing.T) {
	os.Setenv("DATABASE_URL", "postgres://user:pass@localhost/testdb")
	defer os.Unsetenv("DATABASE_URL")

	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load() error: %v", err)
	}

	if cfg.Database.URL != "postgres://user:pass@localhost/testdb" {
		t.Errorf("expected DATABASE_URL from env, got %s", cfg.Database.URL)
	}
}

func TestLoadMQTTConfig(t *testing.T) {
	os.Setenv("MQTT_TLS_CERT", "/etc/certs/mqtt.crt")
	os.Setenv("MQTT_TLS_KEY", "/etc/certs/mqtt.key")
	defer func() {
		os.Unsetenv("MQTT_TLS_CERT")
		os.Unsetenv("MQTT_TLS_KEY")
	}()

	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load() error: %v", err)
	}

	if cfg.MQTT.TLSCert != "/etc/certs/mqtt.crt" {
		t.Errorf("expected TLS cert path, got %s", cfg.MQTT.TLSCert)
	}
	if cfg.MQTT.TLSKey != "/etc/certs/mqtt.key" {
		t.Errorf("expected TLS key path, got %s", cfg.MQTT.TLSKey)
	}
}
