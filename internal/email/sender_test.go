package email

import (
	"bytes"
	"encoding/json"
	"io"
	"net/http"
	"os"
	"testing"

	"github.com/stretchr/testify/assert"
)

// MockRoundTripper for capturing requests
type MockRoundTripper struct {
	CapturedRequest *http.Request
	Response        *http.Response
	Err             error
}

func (m *MockRoundTripper) RoundTrip(req *http.Request) (*http.Response, error) {
	m.CapturedRequest = req
	return m.Response, m.Err
}

func TestNewEmailSender(t *testing.T) {
	// Test defaults
	os.Unsetenv("RESEND_API_KEY")
	os.Unsetenv("EMAIL_FROM")

	sender := NewEmailSender("")
	assert.Equal(t, "onboarding@resend.dev", sender.fromAddress)
	assert.Equal(t, "http://localhost:8000", sender.publicURL)
	assert.Empty(t, sender.apiKey)

	// Test with env vars
	os.Setenv("RESEND_API_KEY", "re_123")
	os.Setenv("EMAIL_FROM", "test@datum.com")

	sender = NewEmailSender("https://datum.com")
	assert.Equal(t, "re_123", sender.apiKey)
	assert.Equal(t, "test@datum.com", sender.fromAddress)
	assert.Equal(t, "https://datum.com", sender.publicURL)
	assert.NotNil(t, sender.client)
}

func TestSendResetEmail_SkipIfNoKey(t *testing.T) {
	sender := &EmailSender{
		apiKey: "", // Empty key
	}

	err := sender.SendResetEmail("user@example.com", "token123")
	assert.NoError(t, err) // Should return nil (skip)
}

func TestSendResetEmail_Success(t *testing.T) {
	// Setup mock response
	mockResp := &http.Response{
		StatusCode: 200,
		Body:       io.NopCloser(bytes.NewBufferString(`{"id":"email_123"}`)),
	}
	mockTransport := &MockRoundTripper{Response: mockResp}

	sender := &EmailSender{
		apiKey:      "re_test_key",
		fromAddress: "test@datum.com",
		publicURL:   "http://localhost:8000",
		client:      &http.Client{Transport: mockTransport},
	}

	err := sender.SendResetEmail("user@example.com", "token123")
	assert.NoError(t, err)

	// Verify request
	req := mockTransport.CapturedRequest
	assert.NotNil(t, req)
	assert.Equal(t, "POST", req.Method)
	assert.Equal(t, "https://api.resend.com/emails", req.URL.String())
	assert.Equal(t, "Bearer re_test_key", req.Header.Get("Authorization"))
	assert.Equal(t, "application/json", req.Header.Get("Content-Type"))

	// Verify payload
	var payload map[string]interface{}
	json.NewDecoder(req.Body).Decode(&payload)
	assert.Equal(t, "test@datum.com", payload["from"])
	assert.Equal(t, "user@example.com", payload["to"].([]interface{})[0])
	assert.Contains(t, payload["html"], "token123")
}

func TestSendResetEmail_APIError(t *testing.T) {
	// Setup mock error response
	mockResp := &http.Response{
		StatusCode: 400,
		Body:       io.NopCloser(bytes.NewBufferString(`{"message":"Invalid API Key"}`)),
	}
	mockTransport := &MockRoundTripper{Response: mockResp}

	sender := &EmailSender{
		apiKey:      "re_test_key",
		fromAddress: "test@datum.com",
		publicURL:   "http://localhost:8000",
		client:      &http.Client{Transport: mockTransport},
	}

	err := sender.SendResetEmail("user@example.com", "token123")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "email API error: Invalid API Key")
}
