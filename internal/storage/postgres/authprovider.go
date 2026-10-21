package postgres

import (
	"context"
	"strings"
	"time"
)

// ValidateAuthClient checks if the given clientID and secret exist and are active.
func (s *PostgresStore) ValidateAuthClient(clientID, clientSecret string) (string, error) {
	var dbClientID string
	var scopesStr string
	query := "SELECT client_id, scopes FROM auth_clients WHERE client_id = $1 AND client_secret = $2 AND status = 'active'"
	row := s.db.QueryRowContext(context.Background(), query, clientID, clientSecret)
	err := row.Scan(&dbClientID, &scopesStr)
	if err != nil {
		return "", err
	}
	return scopesStr, nil
}

// GetAuthClientRolesAndEmails retrieves the allowed roles and emails for an app.
func (s *PostgresStore) GetAuthClientRolesAndEmails(appName string) (string, string, error) {
	query := "SELECT allowed_roles, allowed_emails FROM auth_clients WHERE name = $1 AND status = 'active'"
	row := s.db.QueryRowContext(context.Background(), query, appName)
	var allowedRoles, allowedEmails string
	err := row.Scan(&allowedRoles, &allowedEmails)
	if err != nil {
		return "", "", err
	}
	return allowedRoles, allowedEmails, nil
}

// ListAuthClients retrieves all registered SSO clients.
func (s *PostgresStore) ListAuthClients() ([]map[string]interface{}, error) {
	query := "SELECT name, client_id, status, allowed_roles, allowed_emails, created_at FROM auth_clients ORDER BY created_at DESC"
	rows, err := s.db.QueryContext(context.Background(), query)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var clients []map[string]interface{}
	for rows.Next() {
		var name, clientID, status, allowedRoles, allowedEmails string
		var createdAt time.Time
		if err := rows.Scan(&name, &clientID, &status, &allowedRoles, &allowedEmails, &createdAt); err != nil {
			continue
		}

		roles := parseArray(allowedRoles)
		emails := parseArray(allowedEmails)

		clients = append(clients, map[string]interface{}{
			"name":           name,
			"client_id":      clientID,
			"status":         status,
			"allowed_roles":  roles,
			"allowed_emails": emails,
			"created_at":     createdAt,
		})
	}
	return clients, nil
}

func parseArray(pgArray string) []string {
	if pgArray == "" || pgArray == "{}" {
		return []string{}
	}
	cleaned := strings.Trim(pgArray, "{}")
	return strings.Split(cleaned, ",")
}

// CreateAuthClient adds a new SSO auth client to the database.
func (s *PostgresStore) CreateAuthClient(name, clientID, clientSecret, creatorEmail string, roles []string, emails []string) error {
	query := `
		INSERT INTO auth_clients (name, client_id, client_secret, created_by, allowed_roles, allowed_emails)
		VALUES ($1, $2, $3, $4, $5, $6)
	`

	formatPgArray := func(arr []string) string {
		if len(arr) == 0 {
			return "{}"
		}
		return "{" + strings.Join(arr, ",") + "}"
	}

	_, err := s.db.ExecContext(context.Background(), query,
		name, clientID, clientSecret, creatorEmail,
		formatPgArray(roles), formatPgArray(emails))

	return err
}

// DeleteAuthClient deletes an SSO auth client by name.
func (s *PostgresStore) DeleteAuthClient(name string) (bool, error) {
	query := "DELETE FROM auth_clients WHERE name = $1"
	res, err := s.db.ExecContext(context.Background(), query, name)
	if err != nil {
		return false, err
	}
	rowsAffected, _ := res.RowsAffected()
	return rowsAffected > 0, nil
}
