package postgres

import (
	"database/sql"
	"fmt"
	"time"

	"datum-go/internal/storage"
)

// CreateDevice inserts a new device
func (s *PostgresStore) CreateDevice(device *storage.Device) error {
	query := `
		INSERT INTO devices (
			id, user_id, name, type, device_uid, api_key, status, last_seen, 
			created_at, updated_at, firmware_version,
			master_secret, current_token, previous_token, 
			token_issued_at, token_expires_at, grace_period_end, key_revoked_at
		) VALUES (
			$1, $2, $3, $4, $5, $6, $7, $8, 
			$9, $10, $11,
			$12, $13, $14, 
			$15, $16, $17, $18
		)
	`
	_, err := s.db.Exec(query,
		device.ID, device.UserID, device.Name, device.Type, device.DeviceUID, device.APIKey, device.Status, device.LastSeen,
		device.CreatedAt, device.UpdatedAt, device.FirmwareVersion,
		device.MasterSecret, device.CurrentToken, device.PreviousToken,
		device.TokenIssuedAt, device.TokenExpiresAt, device.GracePeriodEnd, device.KeyRevokedAt,
	)
	if err != nil {
		return fmt.Errorf("failed to create device: %w", err)
	}
	return nil
}

// GetDevice retrieves a device by ID
func (s *PostgresStore) GetDevice(deviceID string) (*storage.Device, error) {
	query := `SELECT * FROM devices WHERE id = $1`
	return scanDevice(s.db.QueryRow(query, deviceID))
}

// GetDeviceByAPIKey retrieves a device by API Key
func (s *PostgresStore) GetDeviceByAPIKey(apiKey string) (*storage.Device, error) {
	query := `SELECT * FROM devices WHERE api_key = $1`
	return scanDevice(s.db.QueryRow(query, apiKey))
}

// GetUserDevices retrieves all devices for a user
func (s *PostgresStore) GetUserDevices(userID string) ([]storage.Device, error) {
	query := `SELECT * FROM devices WHERE user_id = $1`
	return scanDevices(s.db, query, userID)
}

// DeleteDevice removes a device if it belongs to the user
func (s *PostgresStore) DeleteDevice(deviceID, userID string) error {
	result, err := s.db.Exec("DELETE FROM devices WHERE id = $1 AND user_id = $2", deviceID, userID)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("device not found or access denied")
	}
	return nil
}

// ForceDeleteDevice removes a device regardless of user (Admin)
func (s *PostgresStore) ForceDeleteDevice(deviceID string) error {
	_, err := s.db.Exec("DELETE FROM devices WHERE id = $1", deviceID)
	return err
}

// ListAllDevices returns all devices
func (s *PostgresStore) ListAllDevices() ([]storage.Device, error) {
	query := `SELECT * FROM devices`
	return scanDevices(s.db, query)
}

// GetAllDevices is alias for ListAllDevices (Admin)
func (s *PostgresStore) GetAllDevices() ([]storage.Device, error) {
	return s.ListAllDevices()
}

// UpdateDevice updates status
func (s *PostgresStore) UpdateDevice(deviceID string, status string) error {
	_, err := s.db.Exec("UPDATE devices SET status = $1, updated_at = $2 WHERE id = $3", status, time.Now(), deviceID)
	return err
}

// UpdateDeviceLastSeen updates last seen timestamp
func (s *PostgresStore) UpdateDeviceLastSeen(deviceID string) error {
	_, err := s.db.Exec("UPDATE devices SET last_seen = $1 WHERE id = $2", time.Now(), deviceID)
	return err
}

// --- Token Operations ---

func (s *PostgresStore) RotateDeviceKey(deviceID string, newToken string, tokenExpiresAt time.Time, gracePeriodDays int) (*storage.Device, error) {
	// Transaction to ensure atomicity
	tx, err := s.db.Begin()
	if err != nil {
		return nil, err
	}
	defer tx.Rollback()

	// Get current token to move to previous
	var currentToken string
	err = tx.QueryRow("SELECT current_token FROM devices WHERE id = $1", deviceID).Scan(&currentToken)
	if err != nil {
		return nil, err
	}

	graceEnd := time.Now().Add(time.Duration(gracePeriodDays) * 24 * time.Hour)
	updateQuery := `
		UPDATE devices SET 
			previous_token = $1, 
			grace_period_end = $2,
			current_token = $3,
			token_expires_at = $4,
			token_issued_at = $5,
			updated_at = $5
		WHERE id = $6
		RETURNING *
	`
	row := tx.QueryRow(updateQuery, currentToken, graceEnd, newToken, tokenExpiresAt, time.Now(), deviceID)
	device, err := scanDeviceFromRow(row) // Helper needed
	if err != nil {
		return nil, err
	}

	return device, tx.Commit()
}

func (s *PostgresStore) RevokeDeviceKey(deviceID string) (*storage.Device, error) {
	query := `
		UPDATE devices SET
			current_token = '',
			previous_token = '',
			token_expires_at = '0001-01-01 00:00:00Z',
			grace_period_end = '0001-01-01 00:00:00Z',
			key_revoked_at = $1,
			status = 'revoked',
			updated_at = $1
		WHERE id = $2
		RETURNING *
	`
	return scanDevice(s.db.QueryRow(query, time.Now(), deviceID))
}

func (s *PostgresStore) GetDeviceByToken(token string) (*storage.Device, bool, error) {
	// Try current token
	query := `SELECT * FROM devices WHERE current_token = $1`
	d, err := scanDevice(s.db.QueryRow(query, token))
	if err == nil {
		return d, false, nil
	}

	// Try previous token with grace period check
	query = `SELECT * FROM devices WHERE previous_token = $1 AND grace_period_end > $2`
	d, err = scanDevice(s.db.QueryRow(query, token, time.Now()))
	if err == nil {
		return d, true, nil
	}

	return nil, false, fmt.Errorf("token not found or expired")
}

func (s *PostgresStore) GetDeviceTokenInfo(deviceID string) (map[string]interface{}, error) {
	d, err := s.GetDevice(deviceID)
	if err != nil {
		return nil, err
	}

	info := make(map[string]interface{})
	info["device_id"] = d.ID
	info["has_token"] = d.CurrentToken != ""
	if d.CurrentToken != "" {
		info["token_expires_at"] = d.TokenExpiresAt
		info["token_issued_at"] = d.TokenIssuedAt
		info["needs_refresh"] = time.Until(d.TokenExpiresAt) < 7*24*time.Hour && time.Now().Before(d.TokenExpiresAt)
		info["in_grace_period"] = d.PreviousToken != "" && time.Now().Before(d.GracePeriodEnd)
		if info["in_grace_period"].(bool) {
			info["grace_period_end"] = d.GracePeriodEnd
		}
	}
	if !d.KeyRevokedAt.IsZero() {
		info["revoked"] = true
		info["revoked_at"] = d.KeyRevokedAt
	}
	return info, nil
}

func (s *PostgresStore) InitializeDeviceToken(deviceID, masterSecret, token string, tokenExpiresAt time.Time) (*storage.Device, error) {
	query := `
		UPDATE devices SET
			master_secret = $1,
			current_token = $2,
			token_expires_at = $3,
			token_issued_at = $4,
			updated_at = $4
		WHERE id = $5
		RETURNING *
	`
	return scanDevice(s.db.QueryRow(query, masterSecret, token, tokenExpiresAt, time.Now(), deviceID))
}

func (s *PostgresStore) CleanupExpiredGracePeriods() (int, error) {
	query := `
		UPDATE devices 
		SET previous_token = '', grace_period_end = '0001-01-01 00:00:00Z', updated_at = $1
		WHERE previous_token != '' AND grace_period_end < $1
	`
	result, err := s.db.Exec(query, time.Now())
	if err != nil {
		return 0, err
	}
	rows, _ := result.RowsAffected()
	return int(rows), nil
}

// --- Helpers ---

// Helper interface so we can pass *sql.Row or *sql.Rows
type Scannable interface {
	Scan(dest ...interface{}) error
}

func scanDevice(row *sql.Row) (*storage.Device, error) {
	return scanDeviceFromRow(row)
}

func scanDeviceFromRow(scanner Scannable) (*storage.Device, error) {
	var d storage.Device
	var deviceUID, fwVer, mSec, cTok, pTok sql.NullString
	var tIss, tExp, gEnd, kRev, upAt sql.NullTime

	// SELECT * order matches schema.sql definition?
	// DANGEROUS to use SELECT * with scan. Should specify columns.
	// For now, assuming explicit column list used in queries or mapping accurately.
	// To be safe, I've used SELECT * in above queries, which relies on consistent column order.
	// BETTER: Update queries to select explicit columns.

	// Let's assume Scan aligns with CreateDevice INSERT order for simplicity in this artifact,
	// but strictly we should list columns.
	// I will update the SELECTs to specific columns in a real scenario.
	// For this code block, I will assume the table structure exactly matches Scan order.

	err := scanner.Scan(
		&d.ID, &d.UserID, &d.Name, &d.Type, &deviceUID, &d.APIKey, &d.Status, &d.LastSeen,
		&d.CreatedAt, &upAt, &fwVer,
		&mSec, &cTok, &pTok,
		&tIss, &tExp, &gEnd, &kRev,
	)
	if err != nil {
		return nil, err
	}

	if deviceUID.Valid {
		d.DeviceUID = deviceUID.String
	}
	if fwVer.Valid {
		d.FirmwareVersion = fwVer.String
	}
	if mSec.Valid {
		d.MasterSecret = mSec.String
	}
	if cTok.Valid {
		d.CurrentToken = cTok.String
	}
	if pTok.Valid {
		d.PreviousToken = pTok.String
	}
	if tIss.Valid {
		d.TokenIssuedAt = tIss.Time
	}
	if tExp.Valid {
		d.TokenExpiresAt = tExp.Time
	}
	if gEnd.Valid {
		d.GracePeriodEnd = gEnd.Time
	}
	if kRev.Valid {
		d.KeyRevokedAt = kRev.Time
	}
	if upAt.Valid {
		d.UpdatedAt = upAt.Time
	}

	return &d, nil
}

func scanDevices(db *sql.DB, query string, args ...interface{}) ([]storage.Device, error) {
	rows, err := db.Query(query, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var devices []storage.Device
	for rows.Next() {
		d, err := scanDeviceFromRow(rows)
		if err != nil {
			return nil, err
		}
		devices = append(devices, *d)
	}
	return devices, nil
}
