package postgres

import (
	"database/sql"
	_ "embed"
	"fmt"
	"os"
	"strconv"
	"time"

	_ "github.com/lib/pq" // PostgreSQL Driver
)

//go:embed schema.sql
var schemaSQL string

// PostgresStore implements the storage.Provider interface using PostgreSQL
type PostgresStore struct {
	db *sql.DB
}

// New creates a new PostgreSQL storage provider
func New(connString string) (*PostgresStore, error) {
	db, err := sql.Open("postgres", connString)
	if err != nil {
		return nil, fmt.Errorf("failed to open postgres connection: %w", err)
	}

	// Configure connection pool from environment variables
	configureConnectionPool(db)

	// Verify connection
	if err := db.Ping(); err != nil {
		db.Close()
		return nil, fmt.Errorf("failed to ping postgres: %w", err)
	}

	store := &PostgresStore{db: db}

	// Auto-migrate schema
	if err := store.initSchema(); err != nil {
		db.Close()
		return nil, fmt.Errorf("failed to initialize schema: %w", err)
	}

	return store, nil
}

// configureConnectionPool sets database pool settings from env vars or defaults.
// Defaults: MaxOpen=25, MaxIdle=25, MaxLifetime=5m
func configureConnectionPool(db *sql.DB) {
	maxOpen := getEnvInt("POSTGRES_MAX_OPEN_CONNS", 25)
	maxIdle := getEnvInt("POSTGRES_MAX_IDLE_CONNS", 25)
	maxLifetimeMinutes := getEnvInt("POSTGRES_CONN_MAX_LIFETIME_MINUTES", 5)

	db.SetMaxOpenConns(maxOpen)
	db.SetMaxIdleConns(maxIdle)
	db.SetConnMaxLifetime(time.Duration(maxLifetimeMinutes) * time.Minute)
}

// getEnvInt reads an integer from env, returning defaultVal if not set or invalid.
func getEnvInt(key string, defaultVal int) int {
	val := os.Getenv(key)
	if val == "" {
		return defaultVal
	}
	i, err := strconv.Atoi(val)
	if err != nil {
		return defaultVal
	}
	return i
}

// initSchema executes the embedded schema.sql
func (s *PostgresStore) initSchema() error {
	_, err := s.db.Exec(schemaSQL)
	if err != nil {
		return err
	}
	return nil
}

// Close closes the database connection
func (s *PostgresStore) Close() error {
	return s.db.Close()
}

// ============ Generic Document Store (Collections) ============
