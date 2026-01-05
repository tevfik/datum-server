package postgres

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"time"

	"datum-go/internal/storage"
)

// CreateCommand stores a new command
func (s *PostgresStore) CreateCommand(cmd *storage.Command) error {
	if cmd.ExpiresAt.IsZero() {
		cmd.ExpiresAt = time.Now().Add(24 * time.Hour)
	}

	paramsJSON, err := json.Marshal(cmd.Params)
	if err != nil {
		return err
	}

	query := `
		INSERT INTO commands (id, device_id, action, params, status, created_at, expires_at)
		VALUES ($1, $2, $3, $4, $5, $6, $7)
	`
	_, err = s.db.Exec(query,
		cmd.ID, cmd.DeviceID, cmd.Action, paramsJSON, cmd.Status, cmd.CreatedAt, cmd.ExpiresAt,
	)
	if err != nil {
		return fmt.Errorf("failed to create command: %w", err)
	}
	return nil
}

// GetPendingCommands retrieves commands that are pending and not expired
func (s *PostgresStore) GetPendingCommands(deviceID string) ([]storage.Command, error) {
	query := `
		SELECT id, device_id, action, params, status, created_at, expires_at
		FROM commands
		WHERE device_id = $1 AND status = 'pending' AND expires_at > $2
		ORDER BY created_at ASC
	`
	rows, err := s.db.Query(query, deviceID, time.Now())
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var commands []storage.Command
	for rows.Next() {
		var cmd storage.Command
		var paramsJSON []byte
		var expiresAt sql.NullTime

		err := rows.Scan(
			&cmd.ID, &cmd.DeviceID, &cmd.Action, &paramsJSON, &cmd.Status, &cmd.CreatedAt, &expiresAt,
		)
		if err != nil {
			return nil, err
		}

		if expiresAt.Valid {
			cmd.ExpiresAt = expiresAt.Time
		}
		if len(paramsJSON) > 0 {
			json.Unmarshal(paramsJSON, &cmd.Params)
		}

		commands = append(commands, cmd)
	}
	return commands, nil
}

// GetPendingCommandCount returns count of pending commands
func (s *PostgresStore) GetPendingCommandCount(deviceID string) int {
	var count int
	query := `
		SELECT COUNT(*) 
		FROM commands 
		WHERE device_id = $1 AND status = 'pending' AND expires_at > $2
	`
	_ = s.db.QueryRow(query, deviceID, time.Now()).Scan(&count)
	return count
}

// AcknowledgeCommand updates command status
func (s *PostgresStore) AcknowledgeCommand(cmdID string, result map[string]interface{}) error {
	resultJSON, err := json.Marshal(result)
	if err != nil {
		return err
	}

	query := `
		UPDATE commands 
		SET status = 'acknowledged', result = $1, acked_at = $2
		WHERE id = $3
	`
	res, err := s.db.Exec(query, resultJSON, time.Now(), cmdID)
	if err != nil {
		return err
	}

	rows, _ := res.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("command not found")
	}
	return nil
}
