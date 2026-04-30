package postgres

import (
	"context"
	"database/sql"
	"fmt"

	"datum-go/internal/storage"
)

// CreateUserAPIKey inserts a new user API key
func (s *PostgresStore) CreateUserAPIKey(key *storage.APIKey) error {
	ctx, cancel := queryCtx()
	defer cancel()
	query := `
		INSERT INTO user_api_keys (id, user_id, name, key_value, created_at)
		VALUES ($1, $2, $3, $4, $5)
	`
	_, err := s.db.ExecContext(ctx, query,
		key.ID, key.UserID, key.Name, key.Key, key.CreatedAt,
	)
	if err != nil {
		return fmt.Errorf("failed to create api key: %w", err)
	}
	return nil
}

// GetUserAPIKeys lists all keys for a user
func (s *PostgresStore) GetUserAPIKeys(userID string) ([]storage.APIKey, error) {
	ctx, cancel := queryCtx()
	defer cancel()
	query := `SELECT id, user_id, name, key_value, created_at FROM user_api_keys WHERE user_id = $1`
	return scanAPIKeysCtx(ctx, s.db, query, userID)
}

// DeleteUserAPIKey deletes a key
func (s *PostgresStore) DeleteUserAPIKey(userID, keyID string) error {
	ctx, cancel := queryCtx()
	defer cancel()
	result, err := s.db.ExecContext(ctx, "DELETE FROM user_api_keys WHERE id = $1 AND user_id = $2", keyID, userID)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("key not found or unauthorized")
	}
	return nil
}

// GetUserByUserAPIKey retrieves user by API key
func (s *PostgresStore) GetUserByUserAPIKey(apiKey string) (*storage.User, error) {
	ctx, cancel := queryCtx()
	defer cancel()
	query := `
		SELECT u.id, u.email, u.password_hash, u.role, u.status, u.created_at, u.updated_at, u.last_login_at
		FROM users u
		JOIN user_api_keys k ON u.id = k.user_id
		WHERE k.key_value = $1
	`
	row := s.db.QueryRowContext(ctx, query, apiKey)
	return scanUser(row)
}

// --- Helpers ---

func scanAPIKeys(db *sql.DB, query string, args ...interface{}) ([]storage.APIKey, error) {
	rows, err := db.Query(query, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var keys []storage.APIKey
	for rows.Next() {
		var k storage.APIKey
		if err := rows.Scan(&k.ID, &k.UserID, &k.Name, &k.Key, &k.CreatedAt); err != nil {
			return nil, err
		}
		keys = append(keys, k)
	}
	return keys, nil
}

func scanAPIKeysCtx(ctx context.Context, db *sql.DB, query string, args ...interface{}) ([]storage.APIKey, error) {
	rows, err := db.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var keys []storage.APIKey
	for rows.Next() {
		var k storage.APIKey
		if err := rows.Scan(&k.ID, &k.UserID, &k.Name, &k.Key, &k.CreatedAt); err != nil {
			return nil, err
		}
		keys = append(keys, k)
	}
	return keys, nil
}

// Helper interface for Row/Rows scan
type Scanner interface {
	Scan(dest ...interface{}) error
}
