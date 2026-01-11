package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"github.com/spf13/viper"
)

// APIClient handles HTTP communication with Datum server
type APIClient struct {
	BaseURL      string
	Token        string
	RefreshToken string
	APIKey       string
	HTTPClient   *http.Client
}

// NewAPIClient creates a new API client
func NewAPIClient(baseURL, token, apiKey string) *APIClient {
	// Try to load refresh token from viper if not explicitly passed/managed
	// In a real CLI structure, we might want to pass it explicitly.
	// For now, let's grab it from viper if token matches what's in viper.
	refreshToken := ""
	if viper.IsSet("refresh_token") {
		refreshToken = viper.GetString("refresh_token")
	}

	return &APIClient{
		BaseURL:      baseURL,
		Token:        token,
		RefreshToken: refreshToken,
		APIKey:       apiKey,
		HTTPClient:   &http.Client{Timeout: 30 * time.Second},
	}
}

// Request makes an HTTP request to the API
func (c *APIClient) Request(method, path string, body interface{}) (*http.Response, error) {
	return c.requestWithRetry(method, path, body, false)
}

func (c *APIClient) requestWithRetry(method, path string, body interface{}, isRetry bool) (*http.Response, error) {
	var reqBody io.Reader
	var jsonData []byte
	if body != nil {
		var err error
		jsonData, err = json.Marshal(body)
		if err != nil {
			return nil, fmt.Errorf("failed to marshal request body: %w", err)
		}
		reqBody = bytes.NewBuffer(jsonData)
	}

	fullURL := c.BaseURL + path
	req, err := http.NewRequest(method, fullURL, reqBody)
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	// Set headers
	req.Header.Set("Content-Type", "application/json")

	// Authentication
	if c.Token != "" {
		req.Header.Set("Authorization", "Bearer "+c.Token)
	} else if c.APIKey != "" {
		req.Header.Set("Authorization", "Bearer "+c.APIKey)
	}

	if showCurl && !isRetry { // Only show curl on first try or strictly if desired
		var cmd strings.Builder
		cmd.WriteString(fmt.Sprintf("curl -X %s %s", method, fullURL))
		cmd.WriteString(" \\\n  -H 'Content-Type: application/json'")

		if c.Token != "" {
			cmd.WriteString(fmt.Sprintf(" \\\n  -H 'Authorization: Bearer %s'", c.Token))
		} else if c.APIKey != "" {
			cmd.WriteString(fmt.Sprintf(" \\\n  -H 'Authorization: Bearer %s'", c.APIKey))
		}

		if len(jsonData) > 0 {
			cmd.WriteString(fmt.Sprintf(" \\\n  -d '%s'", string(jsonData)))
		}

		fmt.Println("\n" + cmd.String() + "\n")
	}

	if verbose {
		fmt.Printf("→ %s %s\n", method, path)
	}

	resp, err := c.HTTPClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("request failed: %w", err)
	}

	if verbose {
		fmt.Printf("← %d %s\n", resp.StatusCode, resp.Status)
	}

	// Auto-Refresh Logic
	if resp.StatusCode == http.StatusUnauthorized && c.RefreshToken != "" && !isRetry {
		// Attempt refresh
		if err := c.refreshAccessToken(); err == nil {
			// Refresh successful, retry original request
			// We need to re-create the body reader since it was read
			resp.Body.Close()
			return c.requestWithRetry(method, path, body, true)
		} else {
			// If debugging
			if verbose {
				fmt.Printf("Token refresh failed: %v\n", err)
			}
		}
	}

	return resp, nil
}

func (c *APIClient) refreshAccessToken() error {
	refreshReq := map[string]string{
		"refresh_token": c.RefreshToken,
	}

	jsonData, _ := json.Marshal(refreshReq)
	req, _ := http.NewRequest("POST", c.BaseURL+"/auth/refresh", bytes.NewBuffer(jsonData))
	req.Header.Set("Content-Type", "application/json")

	resp, err := c.HTTPClient.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("refresh failed with status: %d", resp.StatusCode)
	}

	var refreshResp struct {
		Token        string `json:"token"`
		RefreshToken string `json:"refresh_token"`
	}

	body, _ := io.ReadAll(resp.Body)
	if err := json.Unmarshal(body, &refreshResp); err != nil {
		return err
	}

	// Update client state
	c.Token = refreshResp.Token
	if refreshResp.RefreshToken != "" {
		c.RefreshToken = refreshResp.RefreshToken
	}

	// Update persistent config
	viper.Set("token", c.Token)
	viper.Set("refresh_token", c.RefreshToken)
	viper.WriteConfig() // Best effort

	return nil
}

// Get makes a GET request
func (c *APIClient) Get(path string) (*http.Response, error) {
	return c.Request("GET", path, nil)
}

// Post makes a POST request
func (c *APIClient) Post(path string, body interface{}) (*http.Response, error) {
	return c.Request("POST", path, body)
}

// Put makes a PUT request
func (c *APIClient) Put(path string, body interface{}) (*http.Response, error) {
	return c.Request("PUT", path, body)
}

// Delete makes a DELETE request
func (c *APIClient) Delete(path string, body interface{}) (*http.Response, error) {
	return c.Request("DELETE", path, body)
}

// ParseResponse reads and parses JSON response
func ParseResponse(resp *http.Response, target interface{}) error {
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return fmt.Errorf("failed to read response: %w", err)
	}

	if resp.StatusCode >= 400 {
		return fmt.Errorf("API error (%d): %s", resp.StatusCode, string(body))
	}

	if target != nil && len(body) > 0 {
		if err := json.Unmarshal(body, target); err != nil {
			return fmt.Errorf("failed to parse response: %w", err)
		}
	}

	return nil
}
