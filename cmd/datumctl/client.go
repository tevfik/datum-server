package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"
)

// APIClient handles HTTP communication with Datum server
type APIClient struct {
	BaseURL    string
	Token      string
	APIKey     string
	HTTPClient *http.Client
}

// NewAPIClient creates a new API client
func NewAPIClient(baseURL, token, apiKey string) *APIClient {
	return &APIClient{
		BaseURL: baseURL,
		Token:   token,
		APIKey:  apiKey,
		HTTPClient: &http.Client{
			Timeout: 30 * time.Second,
		},
	}
}

// Request makes an HTTP request to the API
func (c *APIClient) Request(method, path string, body interface{}) (*http.Response, error) {
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

	if showCurl {
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

	return resp, nil
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
