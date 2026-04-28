package postgres

import (
	"fmt"
	"time"

	"datum-go/internal/storage"
)

// ============ Session Operations ============

func (s *PostgresStore) CreateSession(session *storage.Session) error {
	ctx, cancel := queryCtx()
	defer cancel()
	_, err := s.db.ExecContext(ctx, `
		INSERT INTO sessions (jti, user_id, created_at, expires_at, user_agent, ip)
		VALUES ($1, $2, $3, $4, $5, $6)
	`, session.JTI, session.UserID, session.CreatedAt, session.ExpiresAt, session.UserAgent, session.IP)
	return err
}

func (s *PostgresStore) GetSession(jti string) (*storage.Session, error) {
	ctx, cancel := queryCtx()
	defer cancel()
	row := s.db.QueryRowContext(ctx, `
		SELECT jti, user_id, created_at, expires_at, user_agent, ip
		FROM sessions WHERE jti = $1 AND expires_at > NOW()
	`, jti)
	var ses storage.Session
	err := row.Scan(&ses.JTI, &ses.UserID, &ses.CreatedAt, &ses.ExpiresAt, &ses.UserAgent, &ses.IP)
	if err != nil {
		return nil, fmt.Errorf("session not found")
	}
	return &ses, nil
}

func (s *PostgresStore) DeleteSession(jti string) error {
	ctx, cancel := queryCtx()
	defer cancel()
	_, err := s.db.ExecContext(ctx, `DELETE FROM sessions WHERE jti = $1`, jti)
	return err
}

func (s *PostgresStore) GetUserSessions(userID string) ([]*storage.Session, error) {
	ctx, cancel := queryCtx()
	defer cancel()
	rows, err := s.db.QueryContext(ctx, `
		SELECT jti, user_id, created_at, expires_at, user_agent, ip
		FROM sessions WHERE user_id = $1 AND expires_at > NOW()
		ORDER BY created_at DESC
	`, userID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var sessions []*storage.Session
	for rows.Next() {
		var ses storage.Session
		if err := rows.Scan(&ses.JTI, &ses.UserID, &ses.CreatedAt, &ses.ExpiresAt, &ses.UserAgent, &ses.IP); err != nil {
			return nil, err
		}
		sessions = append(sessions, &ses)
	}
	return sessions, rows.Err()
}

func (s *PostgresStore) DeleteAllUserSessions(userID string) error {
	ctx, cancel := queryCtx()
	defer cancel()
	_, err := s.db.ExecContext(ctx, `DELETE FROM sessions WHERE user_id = $1`, userID)
	return err
}

// ============ Push Token Operations ============

func (s *PostgresStore) SavePushToken(pt *storage.PushToken) error {
	ctx, cancel := queryCtx()
	defer cancel()
	_, err := s.db.ExecContext(ctx, `
		INSERT INTO push_tokens (id, user_id, platform, token, created_at)
		VALUES ($1, $2, $3, $4, $5)
		ON CONFLICT (user_id, token) DO UPDATE SET platform = $3, created_at = $5
	`, pt.ID, pt.UserID, pt.Platform, pt.Token, pt.CreatedAt)
	return err
}

func (s *PostgresStore) GetUserPushTokens(userID string) ([]*storage.PushToken, error) {
	ctx, cancel := queryCtx()
	defer cancel()
	rows, err := s.db.QueryContext(ctx, `
		SELECT id, user_id, platform, token, created_at
		FROM push_tokens WHERE user_id = $1 ORDER BY created_at DESC
	`, userID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var tokens []*storage.PushToken
	for rows.Next() {
		var pt storage.PushToken
		if err := rows.Scan(&pt.ID, &pt.UserID, &pt.Platform, &pt.Token, &pt.CreatedAt); err != nil {
			return nil, err
		}
		tokens = append(tokens, &pt)
	}
	return tokens, rows.Err()
}

func (s *PostgresStore) DeletePushToken(userID, tokenID string) error {
	ctx, cancel := queryCtx()
	defer cancel()
	result, err := s.db.ExecContext(ctx, `
		DELETE FROM push_tokens WHERE id = $1 AND user_id = $2
	`, tokenID, userID)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("push token not found")
	}
	return nil
}

// ============ Profile ============

func (s *PostgresStore) UpdateUserProfile(userID, displayName string) error {
	ctx, cancel := queryCtx()
	defer cancel()
	_, err := s.db.ExecContext(ctx,
		`UPDATE users SET display_name = $1, updated_at = $2 WHERE id = $3`,
		displayName, time.Now(), userID,
	)
	return err
}
