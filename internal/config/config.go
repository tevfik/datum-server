// Package config provides centralized configuration loading for datum-server.
// Configuration is read from (in order of rising priority):
//  1. Built-in defaults
//  2. YAML config file (datum-server.yaml or $DATUM_CONFIG)
//  3. .env file (via godotenv, if present)
//  4. Environment variables
package config

import (
	"fmt"
	"os"
	"strings"

	"github.com/spf13/viper"
)

// Config holds all server configuration.
type Config struct {
	Server   ServerConfig   `mapstructure:"server"`
	Database DatabaseConfig `mapstructure:"database"`
	Auth     AuthConfig     `mapstructure:"auth"`
	MQTT     MQTTConfig     `mapstructure:"mqtt"`
	Email    EmailConfig    `mapstructure:"email"`
	Logging  LoggingConfig  `mapstructure:"logging"`
	Data     DataConfig     `mapstructure:"data"`
}

type ServerConfig struct {
	Port                  int    `mapstructure:"port"`
	URL                   string `mapstructure:"url"`
	DataDir               string `mapstructure:"data_dir"`
	CORSAllowedOrigins    string `mapstructure:"cors_allowed_origins"`
	EnableHSTS            bool   `mapstructure:"enable_hsts"`
	ContentSecurityPolicy string `mapstructure:"content_security_policy"`
	MaxRequestBodyBytes   int64  `mapstructure:"max_request_body_bytes"`
	DocsUser              string `mapstructure:"docs_user"`
	DocsPass              string `mapstructure:"docs_pass"`
}

type DatabaseConfig struct {
	URL                    string `mapstructure:"url"`
	MaxOpenConns           int    `mapstructure:"max_open_conns"`
	MaxIdleConns           int    `mapstructure:"max_idle_conns"`
	ConnMaxLifetimeMinutes int    `mapstructure:"conn_max_lifetime_minutes"`
}

type AuthConfig struct {
	JWTSecret              string `mapstructure:"jwt_secret"`
	JWTRefreshExpiryDays   int    `mapstructure:"jwt_refresh_expiry_days"`
	JWTAccessExpiryMinutes int    `mapstructure:"jwt_access_expiry_minutes"`
	RateLimitRequests      int    `mapstructure:"rate_limit_requests"`
	RateLimitWindowSeconds int    `mapstructure:"rate_limit_window_seconds"`
}

type MQTTConfig struct {
	TLSCert string `mapstructure:"tls_cert"`
	TLSKey  string `mapstructure:"tls_key"`
}

type EmailConfig struct {
	ResendAPIKey string `mapstructure:"resend_api_key"`
	FromAddress  string `mapstructure:"from_address"`
}

type LoggingConfig struct {
	Level  string `mapstructure:"level"`
	Output string `mapstructure:"output"`
}

type DataConfig struct {
	RetentionMaxDays        int `mapstructure:"retention_max_days"`
	PublicDataRetentionDays int `mapstructure:"public_data_retention_days"`
	RetentionCheckHours     int `mapstructure:"retention_check_hours"`
	TelemetryBufferSize     int `mapstructure:"telemetry_buffer_size"`
	QueryMaxLimit           int `mapstructure:"query_max_limit"`
}

// Load reads configuration from file, env, and defaults, returning the
// merged result. Environment variables take precedence over the config file.
func Load() (*Config, error) {
	v := viper.New()

	// --- defaults ---
	v.SetDefault("server.port", 8000)
	v.SetDefault("server.data_dir", "./data")
	v.SetDefault("server.cors_allowed_origins", "*")
	v.SetDefault("server.enable_hsts", true)
	v.SetDefault("server.max_request_body_bytes", 5242880) // 5MB

	v.SetDefault("database.max_open_conns", 25)
	v.SetDefault("database.max_idle_conns", 25)
	v.SetDefault("database.conn_max_lifetime_minutes", 5)

	v.SetDefault("auth.jwt_refresh_expiry_days", 30)
	v.SetDefault("auth.jwt_access_expiry_minutes", 15)
	v.SetDefault("auth.rate_limit_requests", 100)
	v.SetDefault("auth.rate_limit_window_seconds", 60)

	v.SetDefault("logging.level", "INFO")
	v.SetDefault("logging.output", "console")

	v.SetDefault("email.from_address", "onboarding@resend.dev")

	v.SetDefault("data.retention_max_days", 7)
	v.SetDefault("data.public_data_retention_days", 1)
	v.SetDefault("data.retention_check_hours", 1)
	v.SetDefault("data.telemetry_buffer_size", 10000)
	v.SetDefault("data.query_max_limit", 10000)

	// --- env bindings --- (flat env vars map to nested config)
	bindings := map[string]string{
		"server.port":                        "PORT",
		"server.url":                         "SERVER_URL",
		"server.data_dir":                    "DATA_DIR",
		"server.cors_allowed_origins":        "CORS_ALLOWED_ORIGINS",
		"server.enable_hsts":                 "ENABLE_HSTS",
		"server.content_security_policy":     "CONTENT_SECURITY_POLICY",
		"server.max_request_body_bytes":      "MAX_REQUEST_BODY_BYTES",
		"server.docs_user":                   "DOCS_USER",
		"server.docs_pass":                   "DOCS_PASS",
		"database.url":                       "DATABASE_URL",
		"database.max_open_conns":            "POSTGRES_MAX_OPEN_CONNS",
		"database.max_idle_conns":            "POSTGRES_MAX_IDLE_CONNS",
		"database.conn_max_lifetime_minutes": "POSTGRES_CONN_MAX_LIFETIME_MINUTES",
		"auth.jwt_secret":                    "JWT_SECRET",
		"auth.jwt_refresh_expiry_days":       "JWT_REFRESH_EXPIRY",
		"auth.jwt_access_expiry_minutes":     "JWT_ACCESS_EXPIRY_MINUTES",
		"auth.rate_limit_requests":           "RATE_LIMIT_REQUESTS",
		"auth.rate_limit_window_seconds":     "RATE_LIMIT_WINDOW_SECONDS",
		"mqtt.tls_cert":                      "MQTT_TLS_CERT",
		"mqtt.tls_key":                       "MQTT_TLS_KEY",
		"email.resend_api_key":               "RESEND_API_KEY",
		"email.from_address":                 "EMAIL_FROM",
		"logging.level":                      "LOG_LEVEL",
		"logging.output":                     "LOG_OUTPUT",
		"data.retention_max_days":            "RETENTION_MAX_DAYS",
		"data.public_data_retention_days":    "PUBLIC_DATA_RETENTION_DAYS",
		"data.retention_check_hours":         "RETENTION_CHECK_HOURS",
		"data.telemetry_buffer_size":         "TELEMETRY_BUFFER_SIZE",
		"data.query_max_limit":               "QUERY_MAX_LIMIT",
	}
	for key, env := range bindings {
		if err := v.BindEnv(key, env); err != nil {
			return nil, fmt.Errorf("bind env %s: %w", env, err)
		}
	}

	// --- config file ---
	configFile := os.Getenv("DATUM_CONFIG")
	if configFile != "" {
		v.SetConfigFile(configFile)
	} else {
		v.SetConfigName("datum-server")
		v.SetConfigType("yaml")
		v.AddConfigPath(".")
		v.AddConfigPath("/etc/datum/")
	}

	if err := v.ReadInConfig(); err != nil {
		if _, ok := err.(viper.ConfigFileNotFoundError); !ok {
			// Only fail on real read errors; missing file is fine
			if !strings.Contains(err.Error(), "Not Found") {
				return nil, fmt.Errorf("config file error: %w", err)
			}
		}
	}

	var cfg Config
	if err := v.Unmarshal(&cfg); err != nil {
		return nil, fmt.Errorf("unmarshal config: %w", err)
	}

	return &cfg, nil
}
