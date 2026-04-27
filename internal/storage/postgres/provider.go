package postgres

import (
	"context"
	"database/sql"
	"embed"
	"fmt"
	"io/fs"
	"log"
	"os"
	"strconv"
	"time"

	"github.com/golang-migrate/migrate/v4"
	migratepg "github.com/golang-migrate/migrate/v4/database/postgres"
	"github.com/golang-migrate/migrate/v4/source/iofs"
	_ "github.com/lib/pq" // PostgreSQL Driver
)

// defaultQueryTimeout is the maximum time a single query may take.
const defaultQueryTimeout = 10 * time.Second

// queryCtx returns a context with the default query timeout.
func queryCtx() (context.Context, context.CancelFunc) {
	return context.WithTimeout(context.Background(), defaultQueryTimeout)
}

//go:embed migrations/*.sql
var migrationsFS embed.FS

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

// initSchema runs database migrations. On a fresh database it applies all
// migrations sequentially; on an existing one it brings the schema up to
// the latest version. If migrations fail (e.g. dirty state), it falls back
// to the legacy embedded schema.sql for backward compatibility.
func (s *PostgresStore) initSchema() error {
	// Try golang-migrate first
	if err := s.runMigrations(); err != nil {
		log.Printf("Warning: migrations failed (%v), falling back to schema.sql", err)
		if _, execErr := s.db.Exec(schemaSQL); execErr != nil {
			return fmt.Errorf("fallback schema.sql failed: %w", execErr)
		}
	}
	return nil
}

// runMigrations applies pending migrate-style migrations from the embedded FS.
func (s *PostgresStore) runMigrations() error {
	subFS, err := fs.Sub(migrationsFS, "migrations")
	if err != nil {
		return fmt.Errorf("failed to open migrations sub-FS: %w", err)
	}

	sourceDriver, err := iofs.New(subFS, ".")
	if err != nil {
		return fmt.Errorf("iofs source: %w", err)
	}

	dbDriver, err := migratepg.WithInstance(s.db, &migratepg.Config{})
	if err != nil {
		return fmt.Errorf("migrate db driver: %w", err)
	}

	m, err := migrate.NewWithInstance("iofs", sourceDriver, "postgres", dbDriver)
	if err != nil {
		return fmt.Errorf("migrate instance: %w", err)
	}

	if err := m.Up(); err != nil && err != migrate.ErrNoChange {
		return fmt.Errorf("migrate up: %w", err)
	}
	return nil
}

// Close closes the database connection
func (s *PostgresStore) Close() error {
	return s.db.Close()
}

// ============ Generic Document Store (Collections) ============
