package utils

import (
	"encoding/json"
	"fmt"
	"io"
	"net/mail"
	"time"
)

// GetString safely retrieves a string from a map[string]interface{}
func GetString(m map[string]interface{}, key string) string {
	if val, ok := m[key]; ok {
		if str, ok := val.(string); ok {
			return str
		}
		return fmt.Sprintf("%v", val)
	}
	return "-"
}

// PrintJSON prints data as pretty-printed JSON to the provided writer
func PrintJSON(w io.Writer, data interface{}) error {
	encoder := json.NewEncoder(w)
	encoder.SetIndent("", "  ")
	return encoder.Encode(data)
}

// ParseDuration wraps time.ParseDuration
func ParseDuration(s string) (time.Duration, error) {
	return time.ParseDuration(s)
}

// ParseTime tries to parse time in RFC3339 or "YYYY-MM-DD HH:MM" format
func ParseTime(s string) (time.Time, error) {
	// Try RFC3339 first
	t, err := time.Parse(time.RFC3339, s)
	if err == nil {
		return t, nil
	}

	// Try common format
	t, err = time.Parse("2006-01-02 15:04", s)
	if err == nil {
		return t, nil
	}

	return time.Time{}, fmt.Errorf("unsupported time format (use RFC3339 or 'YYYY-MM-DD HH:MM')")
}

// IsValidEmail validates an email address
func IsValidEmail(email string) bool {
	_, err := mail.ParseAddress(email)
	return err == nil
}

// IdentifierContainsEmail checks if a string looks like an email (contains @)
func IdentifierContainsEmail(s string) bool {
	for _, c := range s {
		if c == '@' {
			return true
		}
	}
	return false
}

// FormatOutput formats data based on the type (placeholder for future expansion)
// Currently just a wrapper around PrintJSON for testing purposes or simple output
func FormatOutput(w io.Writer, data interface{}, format string) error {
	if format == "json" {
		return PrintJSON(w, data)
	}
	// Default to just printing string representation for now if not json
	_, err := fmt.Fprintln(w, data)
	return err
}
