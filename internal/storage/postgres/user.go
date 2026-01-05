package postgres

import (
	"database/sql"
	"fmt"
	"time"

	"datum-go/internal/storage"
)

// CreateUser inserts a new user
func (s *PostgresStore) CreateUser(user *storage.User) error {
	query := `
		INSERT INTO users (id, email, password_hash, role, status, created_at, updated_at, last_login_at)
		VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
	`
	_, err := s.db.Exec(query,
		user.ID,
		user.Email,
		user.PasswordHash,
		user.Role,
		user.Status,
		user.CreatedAt,
		user.UpdatedAt,
		user.LastLoginAt,
	)
	if err != nil {
		return fmt.Errorf("failed to create user: %w", err)
	}
	return nil
}

// GetUserByEmail retrieves a user by email
func (s *PostgresStore) GetUserByEmail(email string) (*storage.User, error) {
	query := `
		SELECT id, email, password_hash, role, status, created_at, updated_at, last_login_at
		FROM users WHERE email = $1
	`
	row := s.db.QueryRow(query, email)
	return scanUser(row)
}

// GetUserByID retrieves a user by ID
func (s *PostgresStore) GetUserByID(userID string) (*storage.User, error) {
	query := `
		SELECT id, email, password_hash, role, status, created_at, updated_at, last_login_at
		FROM users WHERE id = $1
	`
	row := s.db.QueryRow(query, userID)
	return scanUser(row)
}

// GetUserCount returns the total number of users
func (s *PostgresStore) GetUserCount() (int, error) {
	var count int
	err := s.db.QueryRow("SELECT COUNT(*) FROM users").Scan(&count)
	return count, err
}

// ListAllUsers returns all users
func (s *PostgresStore) ListAllUsers() ([]storage.User, error) {
	query := `
		SELECT id, email, password_hash, role, status, created_at, updated_at, last_login_at
		FROM users
	`
	rows, err := s.db.Query(query)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var users []storage.User
	for rows.Next() {
		var u storage.User
		if err := scanUserRow(rows, &u); err != nil {
			return nil, err
		}
		users = append(users, u)
	}
	return users, nil
}

// UpdateUser updates role and status
func (s *PostgresStore) UpdateUser(userID string, role, status string) error {
	query := `UPDATE users SET role = $1, status = $2, updated_at = $3 WHERE id = $4`
	_, err := s.db.Exec(query, role, status, time.Now(), userID)
	return err
}

// UpdateUserLastLogin updates last login timestamp
func (s *PostgresStore) UpdateUserLastLogin(userID string) error {
	query := `UPDATE users SET last_login_at = $1 WHERE id = $2`
	_, err := s.db.Exec(query, time.Now(), userID)
	return err
}

// UpdateUserPassword updates password hash
func (s *PostgresStore) UpdateUserPassword(userID string, passwordHash string) error {
	query := `UPDATE users SET password_hash = $1, updated_at = $2 WHERE id = $3`
	_, err := s.db.Exec(query, passwordHash, time.Now(), userID)
	return err
}

// DeleteUser removes a user
func (s *PostgresStore) DeleteUser(userID string) error {
	// Cascade delete handles devices, provisioning_requests
	_, err := s.db.Exec("DELETE FROM users WHERE id = $1", userID)
	return err
}

// --- Helpers ---

func scanUser(row *sql.Row) (*storage.User, error) {
	var u storage.User
	var updatedAt, lastLoginAt sql.NullTime

	err := row.Scan(
		&u.ID, &u.Email, &u.PasswordHash, &u.Role, &u.Status,
		&u.CreatedAt, &updatedAt, &lastLoginAt,
	)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, fmt.Errorf("user not found")
		}
		return nil, err
	}

	if updatedAt.Valid {
		u.UpdatedAt = updatedAt.Time
	}
	if lastLoginAt.Valid {
		u.LastLoginAt = lastLoginAt.Time
	}
	return &u, nil
}

func scanUserRow(rows *sql.Rows, u *storage.User) error {
	var updatedAt, lastLoginAt sql.NullTime
	err := rows.Scan(
		&u.ID, &u.Email, &u.PasswordHash, &u.Role, &u.Status,
		&u.CreatedAt, &updatedAt, &lastLoginAt,
	)
	if err != nil {
		return err
	}
	if updatedAt.Valid {
		u.UpdatedAt = updatedAt.Time
	}
	if lastLoginAt.Valid {
		u.LastLoginAt = lastLoginAt.Time
	}
	return nil
}

// --- Password Reset ---

func (s *PostgresStore) SavePasswordResetToken(userID, token string, ttl time.Duration) error {
	expiresAt := time.Now().Add(ttl)
	_, err := s.db.Exec(`
		INSERT INTO password_reset_tokens (token, user_id, expires_at)
		VALUES ($1, $2, $3)
		ON CONFLICT (token) DO NOTHING
	`, token, userID, expiresAt)
	return err
}

func (s *PostgresStore) GetUserByResetToken(token string) (*storage.User, error) {
	query := `
		SELECT u.id, u.email, u.password_hash, u.role, u.status, u.created_at, u.updated_at, u.last_login_at
		FROM users u
		JOIN password_reset_tokens t ON u.id = t.user_id
		WHERE t.token = $1 AND t.expires_at > $2
	`
	row := s.db.QueryRow(query, token, time.Now())
	return scanUser(row)
}

func (s *PostgresStore) DeleteResetToken(token string) error {
	_, err := s.db.Exec("DELETE FROM password_reset_tokens WHERE token = $1", token)
	return err
}
