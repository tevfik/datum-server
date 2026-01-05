package postgres

import (
	"database/sql"
	_ "embed"
	"fmt"

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
