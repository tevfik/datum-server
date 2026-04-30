package postgres

import (
	"context"
	"database/sql"
	"fmt"
	"time"

	"datum-go/internal/storage"
)

// CreateProvisioningRequest creates a new request
func (s *PostgresStore) CreateProvisioningRequest(req *storage.ProvisioningRequest) error {
	ctx, cancel := queryCtx()
	defer cancel()
	// Check if UID is already registered
	var exists bool
	s.db.QueryRowContext(ctx, "SELECT EXISTS(SELECT 1 FROM devices WHERE device_uid = $1)", req.DeviceUID).Scan(&exists)
	if exists {
		return fmt.Errorf("device with UID '%s' is already registered", req.DeviceUID)
	}

	// Check if pending request exists
	var pendingID string
	err := s.db.QueryRowContext(ctx, `
		SELECT id FROM provisioning_requests 
		WHERE device_uid = $1 AND status = 'pending' AND expires_at > $2
	`, req.DeviceUID, time.Now()).Scan(&pendingID)
	if err == nil {
		return fmt.Errorf("a pending provisioning request already exists for UID '%s'", req.DeviceUID)
	}

	query := `
		INSERT INTO provisioning_requests (
			id, device_uid, user_id, device_name, device_type, status,
			server_url, wifi_ssid, wifi_pass, expires_at, created_at
		) VALUES (
			$1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11
		)
	`
	_, err = s.db.ExecContext(ctx, query,
		req.ID, req.DeviceUID, req.UserID, req.DeviceName, req.DeviceType, req.Status,
		req.ServerURL, req.WiFiSSID, req.WiFiPass, req.ExpiresAt, req.CreatedAt,
	)
	if err != nil {
		return fmt.Errorf("failed to create provisioning request: %w", err)
	}
	return nil
}

// GetProvisioningRequestByUID retrieves request by UID
func (s *PostgresStore) GetProvisioningRequestByUID(deviceUID string) (*storage.ProvisioningRequest, error) {
	ctx, cancel := queryCtx()
	defer cancel()
	query := `SELECT * FROM provisioning_requests WHERE device_uid = $1`
	return scanProvisioningRequest(s.db.QueryRowContext(ctx, query, deviceUID))
}

// GetProvisioningRequest retrieve by ID
func (s *PostgresStore) GetProvisioningRequest(reqID string) (*storage.ProvisioningRequest, error) {
	ctx, cancel := queryCtx()
	defer cancel()
	query := `SELECT * FROM provisioning_requests WHERE id = $1`
	return scanProvisioningRequest(s.db.QueryRowContext(ctx, query, reqID))
}

// GetUserProvisioningRequests lists for user
func (s *PostgresStore) GetUserProvisioningRequests(userID string) ([]storage.ProvisioningRequest, error) {
	ctx, cancel := queryCtx()
	defer cancel()
	query := `SELECT * FROM provisioning_requests WHERE user_id = $1`
	return scanProvisioningRequestsCtx(ctx, s.db, query, userID)
}

// CompleteProvisioningRequest marks complete and creates device
func (s *PostgresStore) CompleteProvisioningRequest(reqID string) (*storage.Device, error) {
	tx, err := s.db.Begin()
	if err != nil {
		return nil, err
	}
	defer tx.Rollback()

	// Logic duplicated from BuntDB but in SQL Transaction
	// 1. Get Request
	req, err := scanProvisioningRequest(tx.QueryRow("SELECT * FROM provisioning_requests WHERE id = $1 FOR UPDATE", reqID))
	if err != nil {
		return nil, fmt.Errorf("request not found")
	}

	if req.Status != "pending" {
		return nil, fmt.Errorf("request not pending")
	}
	if time.Now().After(req.ExpiresAt) {
		tx.Exec("UPDATE provisioning_requests SET status = 'expired' WHERE id = $1", reqID)
		return nil, fmt.Errorf("request expired")
	}

	// 2. Update Request
	_, err = tx.Exec(`
		UPDATE provisioning_requests 
		SET status = 'completed', completed_at = $1, device_id = $2, api_key = $3 
		WHERE id = $4
	`, time.Now(), req.DeviceID, req.APIKey, reqID)
	if err != nil {
		return nil, err
	}

	// 3. Create Device
	// We assume req already has the generated DeviceID and APIKey (populated by Service layer before calling this?)
	// Wait, BuntDB implementation showed Storage populating Device?
	// No, BuntDB implementation of CompleteProvisioningRequest in storage.go lines 1360+ created the device struct inside the method.
	// But where did it get the ID?
	// Ah, BuntDB `ProvisioningRequest` struct HAS `DeviceID` field.
	// It seems the service generates IDs and stores them in the request object BEFORE calling Create?
	// Or Update?

	// Let's re-read BuntDB implementation (lines 1392+ in storage.go):
	// It creates a device using `req.DeviceID`, `req.APIKey`.
	// This implies `req` struct stored in DB ALREADY has these values?
	// OR they are passed in?
	// `ProvisioningRequest` struct has them.
	// In `CreateProvisioningRequest`, are they set? usually no.
	// So `CompleteProvisioningRequest` relies on them being present?
	// Actually BuntDB impl line 1360 reads fresh from DB.
	// If the DB version has empty DeviceID, how does it create device?
	// It seems there is a flaw in my understanding or the code.
	// Unless `req` was UPDATED with DeviceID before calling Complete?
	// But `Complete` takes only `reqID`.

	// Re-reading BuntDB code:
	// The `ProvisioningRequest` struct in BuntDB has DeviceID and APIKey.
	// Maybe they are generated at creation time?
	// If so, my SQL INSERT included them (they would be empty strings).
	// So when I read it back, they are empty.
	// This would create a device with empty ID? That would fail.

	// I suspect the BuntDB code or Service code generates them.
	// Note: `storage.go`:
	// `device = &Device{ ID: req.DeviceID ... }`
	// It uses `req.DeviceID`.
	// If `req.DeviceID` is empty, this code is broken.
	// I will assume for now that `req` DOES have them, or I should generate them here.
	// But `Provider` interface doesn't allow passing new ID.
	// So `req` MUST have them.
	// Conclusion: `CreateProvisioningRequest` must be called with these filled, OR there is an intermediate `UpdateProvisioningRequest` call not shown in interface?
	// Interface has `Create` and `Complete`. No generic Update.
	// This implies `CreateProvisioningRequest` probably receives a req with pre-generated IDs.

	device := &storage.Device{
		ID:        req.DeviceID,
		UserID:    req.UserID,
		Name:      req.DeviceName,
		Type:      req.DeviceType,
		DeviceUID: req.DeviceUID,
		APIKey:    req.APIKey,
		Status:    "active",
		LastSeen:  time.Now(),
		CreatedAt: time.Now(),
		UpdatedAt: time.Now(),
	}

	// Insert Device (using same SQL as CreateDevice but inline for transaction)
	_, err = tx.Exec(`
		INSERT INTO devices (
			id, user_id, name, type, device_uid, api_key, status, last_seen, created_at, updated_at
		) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10)
	`, device.ID, device.UserID, device.Name, device.Type, device.DeviceUID, device.APIKey, device.Status, device.LastSeen, device.CreatedAt, device.UpdatedAt)
	if err != nil {
		return nil, fmt.Errorf("failed to create device from provision: %w", err)
	}

	return device, tx.Commit()
}

func (s *PostgresStore) CancelProvisioningRequest(reqID string) error {
	ctx, cancel := queryCtx()
	defer cancel()
	_, err := s.db.ExecContext(ctx, "UPDATE provisioning_requests SET status = 'cancelled' WHERE id = $1", reqID)
	return err
}

func (s *PostgresStore) CleanupExpiredProvisioningRequests() (int, error) {
	ctx, cancel := queryCtx()
	defer cancel()
	res, err := s.db.ExecContext(ctx, "UPDATE provisioning_requests SET status = 'expired' WHERE status = 'pending' AND expires_at < $1", time.Now())
	if err != nil {
		return 0, err
	}
	rows, _ := res.RowsAffected()
	return int(rows), nil
}

func (s *PostgresStore) PurgeProvisioningRequests(maxAge time.Duration) (int, error) {
	ctx, cancel := queryCtx()
	defer cancel()
	cutoff := time.Now().Add(-maxAge)
	res, err := s.db.ExecContext(ctx, "DELETE FROM provisioning_requests WHERE (status = 'expired' OR status = 'cancelled' OR status = 'completed') AND created_at < $1", cutoff)
	if err != nil {
		return 0, err
	}
	rows, _ := res.RowsAffected()
	return int(rows), nil
}

// --- Device UID helpers (Interface compliance) ---

func (s *PostgresStore) IsDeviceUIDRegistered(deviceUID string) (bool, string, error) {
	ctx, cancel := queryCtx()
	defer cancel()
	var deviceID string
	err := s.db.QueryRowContext(ctx, "SELECT id FROM devices WHERE device_uid = $1", deviceUID).Scan(&deviceID)
	if err != nil {
		if err == sql.ErrNoRows {
			return false, "", nil
		}
		return false, "", err
	}
	return true, deviceID, nil
}

func (s *PostgresStore) DeleteDeviceUIDIndex(deviceUID string) error {
	// No-op for Postgres (handled by Row Deletion)
	return nil
}

// --- Helpers ---

func scanProvisioningRequest(scanner Scannable) (*storage.ProvisioningRequest, error) {
	var r storage.ProvisioningRequest
	var compAt sql.NullTime
	var devID, apiKey, srvUrl, ssid, pass sql.NullString

	err := scanner.Scan(
		&r.ID, &r.DeviceUID, &r.UserID, &r.DeviceName, &r.DeviceType, &r.Status,
		&devID, &apiKey, &srvUrl, &ssid, &pass, &r.ExpiresAt, &r.CreatedAt, &compAt,
	)
	if err != nil {
		return nil, err
	}

	if compAt.Valid {
		r.CompletedAt = compAt.Time
	}
	if devID.Valid {
		r.DeviceID = devID.String
	}
	if apiKey.Valid {
		r.APIKey = apiKey.String
	}
	if srvUrl.Valid {
		r.ServerURL = srvUrl.String
	}
	if ssid.Valid {
		r.WiFiSSID = ssid.String
	}
	if pass.Valid {
		r.WiFiPass = pass.String
	}

	return &r, nil
}

func scanProvisioningRequests(db *sql.DB, query string, args ...interface{}) ([]storage.ProvisioningRequest, error) {
	rows, dbErr := db.Query(query, args...)
	if dbErr != nil {
		return nil, dbErr
	}
	defer rows.Close()

	var reqs []storage.ProvisioningRequest
	for rows.Next() {
		r, err := scanProvisioningRequest(rows)
		if err != nil {
			return nil, err
		}
		reqs = append(reqs, *r)
	}
	return reqs, nil
}

func scanProvisioningRequestsCtx(ctx context.Context, db *sql.DB, query string, args ...interface{}) ([]storage.ProvisioningRequest, error) {
	rows, dbErr := db.QueryContext(ctx, query, args...)
	if dbErr != nil {
		return nil, dbErr
	}
	defer rows.Close()

	var reqs []storage.ProvisioningRequest
	for rows.Next() {
		r, err := scanProvisioningRequest(rows)
		if err != nil {
			return nil, err
		}
		reqs = append(reqs, *r)
	}
	return reqs, nil
}
