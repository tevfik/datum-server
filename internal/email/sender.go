package email

import (
	"bytes"
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"time"

	"github.com/rs/zerolog/log"
)

type EmailSender struct {
	apiKey      string
	fromAddress string
	publicURL   string
	client      *http.Client
}

func NewEmailSender(publicURL string) *EmailSender {
	apiKey := os.Getenv("RESEND_API_KEY")
	fromAddress := os.Getenv("EMAIL_FROM")

	if apiKey == "" {
		log.Warn().Msg("RESEND_API_KEY is not set. Email sending will fail.")
	}
	if fromAddress == "" {
		fromAddress = "onboarding@resend.dev" // Default for testing
		log.Warn().Msgf("EMAIL_FROM is not set. Using default: %s", fromAddress)
	}
	if publicURL == "" {
		publicURL = "http://localhost:8000"
		log.Warn().Msgf("Public URL is empty. Using default: %s", publicURL)
	}

	return &EmailSender{
		apiKey:      apiKey,
		fromAddress: fromAddress,
		publicURL:   publicURL,
		client:      &http.Client{Timeout: 10 * time.Second},
	}
}

func (s *EmailSender) SendResetEmail(to, token string) error {
	if s.apiKey == "" {
		log.Warn().Str("to", to).Str("token", token).Msg("Skipping email send (No API Key). Check logs for token.")
		return nil // Don't block the flow, just log
	}

	resetLink := fmt.Sprintf("%s/reset-password?token=%s", s.publicURL, token)

	htmlContent := fmt.Sprintf(`
		<div style="font-family: sans-serif; max-width: 600px; margin: 0 auto;">
			<h2>Password Reset Request</h2>
			<p>You requested a password reset for your Datum account.</p>
			<p>Click the button below to reset your password. This link is valid for 1 hour.</p>
			<a href="%s" style="display: inline-block; background-color: #007bff; color: white; padding: 10px 20px; text-decoration: none; border-radius: 5px;">Reset Password</a>
			<p>Or copy this link: <br> <a href="%s">%s</a></p>
			<p>If you didn't request this, please ignore this email.</p>
		</div>
	`, resetLink, resetLink, resetLink)

	payload := map[string]interface{}{
		"from":    s.fromAddress,
		"to":      []string{to},
		"subject": "Reset your Datum Password",
		"html":    htmlContent,
	}

	jsonData, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("failed to marshal email payload: %w", err)
	}

	req, err := http.NewRequest("POST", "https://api.resend.com/emails", bytes.NewBuffer(jsonData))
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Authorization", "Bearer "+s.apiKey)
	req.Header.Set("Content-Type", "application/json")

	resp, err := s.client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to send email request: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode >= 400 {
		var errorResponse map[string]interface{}
		// Try to parse JSON error
		if err := json.NewDecoder(resp.Body).Decode(&errorResponse); err == nil {
			// If message field exists, use it
			if msg, ok := errorResponse["message"].(string); ok {
				return fmt.Errorf("email API error: %s (status: %d)", msg, resp.StatusCode)
			}
			// Or just log the whole map
			return fmt.Errorf("email API error: %v (status: %d)", errorResponse, resp.StatusCode)
		}
		// Fallback for non-JSON or unparseable errors
		return fmt.Errorf("email API returned status: %d", resp.StatusCode)
	}

	log.Info().Str("to", to).Msg("Password reset email sent successfully")
	return nil
}
