package main

import (
	"io"
	"os"
	"path/filepath"
	"testing"

	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
)

func TestSaveToken(t *testing.T) {
	tmpDir := t.TempDir()
	oldHome := os.Getenv("HOME")
	os.Setenv("HOME", tmpDir)
	defer os.Setenv("HOME", oldHome)

	// Reset viper and globals
	viper.Reset()
	configFile = ""
	serverURL = "http://localhost:8080"

	tokenVal := "test-token-xyz-12345"
	err := saveToken(tokenVal)
	assert.NoError(t, err)

	// Verify token was saved
	configPath := filepath.Join(tmpDir, ".datumctl.yaml")
	data, err := os.ReadFile(configPath)
	assert.NoError(t, err)
	assert.Contains(t, string(data), tokenVal)
}

func TestSaveAPIKey(t *testing.T) {
	tmpDir := t.TempDir()
	oldHome := os.Getenv("HOME")
	os.Setenv("HOME", tmpDir)
	defer os.Setenv("HOME", oldHome)

	// Reset viper and globals
	viper.Reset()
	configFile = ""
	serverURL = "http://localhost:8080"

	// Capture stdout to suppress output
	oldStdout := os.Stdout
	r, w, _ := os.Pipe()
	os.Stdout = w

	apiKeyVal := "test-api-key-abc-67890"
	err := saveAPIKey(apiKeyVal)

	w.Close()
	os.Stdout = oldStdout
	io.ReadAll(r)

	assert.NoError(t, err)

	// Verify API key was saved
	configPath := filepath.Join(tmpDir, ".datumctl.yaml")
	data, err := os.ReadFile(configPath)
	assert.NoError(t, err)
	assert.Contains(t, string(data), apiKeyVal)
}

func TestGetConfigPath(t *testing.T) {
	tmpDir := t.TempDir()
	oldHome := os.Getenv("HOME")
	os.Setenv("HOME", tmpDir)
	defer os.Setenv("HOME", oldHome)

	// Reset global variable
	oldConfigFile := configFile
	configFile = ""
	defer func() { configFile = oldConfigFile }()

	path := getConfigPath()
	expected := filepath.Join(tmpDir, ".datumctl.yaml")
	assert.Equal(t, expected, path)
}

func TestGetConfigPathWithCustomFile(t *testing.T) {
	customPath := "/tmp/custom-config.yaml"
	oldConfigFile := configFile
	configFile = customPath
	defer func() { configFile = oldConfigFile }()

	path := getConfigPath()
	assert.Equal(t, customPath, path)
}

func TestLoadConfig(t *testing.T) {
	tmpDir := t.TempDir()
	oldHome := os.Getenv("HOME")
	os.Setenv("HOME", tmpDir)
	defer os.Setenv("HOME", oldHome)

	// Reset viper and globals
	viper.Reset()
	configFile = ""
	oldServerURL := serverURL
	oldToken := token
	oldAPIKey := apiKey
	serverURL = DefaultServerURL
	token = ""
	apiKey = ""
	defer func() {
		serverURL = oldServerURL
		token = oldToken
		apiKey = oldAPIKey
	}()

	// Create a config file
	configPath := filepath.Join(tmpDir, ".datumctl.yaml")
	configData := `server: http://testserver:9090
token: test-token-123
api_key: test-apikey-456
`
	err := os.WriteFile(configPath, []byte(configData), 0600)
	assert.NoError(t, err)

	// Load config
	loadConfig()

	// Check global variables were set
	assert.Equal(t, "http://testserver:9090", serverURL)
	assert.Equal(t, "test-token-123", token)
	assert.Equal(t, "test-apikey-456", apiKey)
}

func TestLoadConfigNotExist(t *testing.T) {
	tmpDir := t.TempDir()
	oldHome := os.Getenv("HOME")
	os.Setenv("HOME", tmpDir)
	defer os.Setenv("HOME", oldHome)

	// Reset viper and globals
	viper.Reset()
	configFile = ""

	// Try to load non-existent config - should not error
	loadConfig()
	// No assertion needed - just verify it doesn't crash
}

func TestParseTime(t *testing.T) {
	tests := []struct {
		name      string
		timeStr   string
		shouldErr bool
	}{
		{"RFC3339", "2024-01-01T00:00:00Z", false},
		{"Common format", "2024-01-01 15:04", false},
		{"Invalid", "invalid-time", true},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			_, err := parseTime(tt.timeStr)
			if tt.shouldErr {
				assert.Error(t, err)
			} else {
				assert.NoError(t, err)
			}
		})
	}
}

func TestParseDuration(t *testing.T) {
	tests := []struct {
		name      string
		durStr    string
		shouldErr bool
	}{
		{"Hours", "24h", false},
		{"Minutes", "30m", false},
		{"Seconds", "60s", false},
		{"Invalid", "invalid", true},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			_, err := parseDuration(tt.durStr)
			if tt.shouldErr {
				assert.Error(t, err)
			} else {
				assert.NoError(t, err)
			}
		})
	}
}
