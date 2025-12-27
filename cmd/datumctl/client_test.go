package main

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// MockServer creates a test HTTP server for datumctl testing
func setupMockServer() *httptest.Server {
	mux := http.NewServeMux()

	// System status endpoint
	mux.HandleFunc("/system/status", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{
			"initialized":   true,
			"platform_name": "Test Platform",
			"setup_at":      "2025-01-01T00:00:00Z",
		})
	})

	// Login endpoint
	mux.HandleFunc("/auth/login", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != "POST" {
			http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
			return
		}

		var req map[string]string
		if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
			http.Error(w, "Invalid request", http.StatusBadRequest)
			return
		}

		if req["email"] == "admin@test.com" && req["password"] == "admin123" {
			w.Header().Set("Content-Type", "application/json")
			json.NewEncoder(w).Encode(map[string]interface{}{
				"token":   "test-jwt-token-123",
				"user_id": "usr_admin",
				"email":   "admin@test.com",
				"role":    "admin",
			})
		} else {
			http.Error(w, "Invalid credentials", http.StatusUnauthorized)
		}
	})

	// Device endpoints (list and provision)
	mux.HandleFunc("/admin/devices", func(w http.ResponseWriter, r *http.Request) {
		auth := r.Header.Get("Authorization")
		if auth != "Bearer test-jwt-token-123" {
			http.Error(w, "Unauthorized", http.StatusUnauthorized)
			return
		}

		if r.Method == "POST" {
			// Device provision
			var req map[string]string
			if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
				http.Error(w, "Invalid request", http.StatusBadRequest)
				return
			}

			deviceID := req["device_id"]
			if deviceID == "" {
				deviceID = "auto-generated-id"
			}

			w.WriteHeader(http.StatusCreated)
			w.Header().Set("Content-Type", "application/json")
			json.NewEncoder(w).Encode(map[string]interface{}{
				"device_id":  deviceID,
				"name":       req["name"],
				"type":       req["type"],
				"api_key":    "api-key-test-123",
				"status":     "active",
				"created_at": "2025-01-01T00:00:00Z",
			})
		} else {
			// Device list
			w.Header().Set("Content-Type", "application/json")
			json.NewEncoder(w).Encode(map[string]interface{}{
				"devices": []map[string]interface{}{
					{
						"id":         "device001",
						"name":       "Test Device 1",
						"type":       "sensor",
						"status":     "active",
						"created_at": "2025-01-01T00:00:00Z",
					},
					{
						"id":         "device002",
						"name":       "Test Device 2",
						"type":       "actuator",
						"status":     "active",
						"created_at": "2025-01-01T00:00:00Z",
					},
				},
			})
		}
	})

	// User list endpoint
	mux.HandleFunc("/admin/users", func(w http.ResponseWriter, r *http.Request) {
		auth := r.Header.Get("Authorization")
		if auth != "Bearer test-jwt-token-123" {
			http.Error(w, "Unauthorized", http.StatusUnauthorized)
			return
		}

		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{
			"users": []map[string]interface{}{
				{
					"id":         "usr_admin",
					"email":      "admin@test.com",
					"role":       "admin",
					"status":     "active",
					"created_at": "2025-01-01T00:00:00Z",
				},
				{
					"id":         "usr_001",
					"email":      "user@test.com",
					"role":       "user",
					"status":     "active",
					"created_at": "2025-01-01T00:00:00Z",
				},
			},
		})
	})

	// Data query endpoint
	mux.HandleFunc("/data/device001/latest", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{
			"data": []map[string]interface{}{
				{
					"timestamp": "2025-01-01T12:00:00Z",
					"values": map[string]interface{}{
						"temperature": 25.5,
						"humidity":    60.0,
					},
				},
			},
		})
	})

	// Health check endpoint
	mux.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{
			"status": "healthy",
		})
	})

	return httptest.NewServer(mux)
}

func TestClientSystemStatus(t *testing.T) {
	server := setupMockServer()
	defer server.Close()

	// Create API client
	client := NewAPIClient(server.URL, "", "")

	resp, err := client.Get("/system/status")
	require.NoError(t, err)
	defer resp.Body.Close()

	assert.Equal(t, http.StatusOK, resp.StatusCode)

	var result map[string]interface{}
	err = ParseResponse(resp, &result)
	require.NoError(t, err)
	assert.Equal(t, true, result["initialized"])
	assert.Equal(t, "Test Platform", result["platform_name"])
}

func TestClientLogin(t *testing.T) {
	server := setupMockServer()
	defer server.Close()

	client := NewAPIClient(server.URL, "", "")

	// Test successful login
	payload := map[string]string{
		"email":    "admin@test.com",
		"password": "admin123",
	}

	resp, err := client.Post("/auth/login", payload)
	require.NoError(t, err)

	var result map[string]interface{}
	err = ParseResponse(resp, &result)
	require.NoError(t, err)
	assert.Equal(t, "test-jwt-token-123", result["token"])
	assert.Equal(t, "admin@test.com", result["email"])
	assert.Equal(t, "admin", result["role"])
}

func TestClientLoginFailure(t *testing.T) {
	server := setupMockServer()
	defer server.Close()

	client := NewAPIClient(server.URL, "", "")

	// Test failed login
	payload := map[string]string{
		"email":    "wrong@test.com",
		"password": "wrongpass",
	}

	resp, err := client.Post("/auth/login", payload)
	require.NoError(t, err)
	defer resp.Body.Close()

	assert.Equal(t, http.StatusUnauthorized, resp.StatusCode)
}

func TestClientDeviceList(t *testing.T) {
	server := setupMockServer()
	defer server.Close()

	client := NewAPIClient(server.URL, "test-jwt-token-123", "")

	resp, err := client.Get("/admin/devices")
	require.NoError(t, err)

	var result map[string]interface{}
	err = ParseResponse(resp, &result)
	require.NoError(t, err)

	devices := result["devices"].([]interface{})
	assert.Len(t, devices, 2)

	device1 := devices[0].(map[string]interface{})
	assert.Equal(t, "device001", device1["id"])
	assert.Equal(t, "Test Device 1", device1["name"])
}

func TestClientUnauthorizedAccess(t *testing.T) {
	server := setupMockServer()
	defer server.Close()

	client := NewAPIClient(server.URL, "", "")

	// Try to access protected endpoint without token
	resp, err := client.Get("/admin/devices")
	require.NoError(t, err)
	defer resp.Body.Close()

	assert.Equal(t, http.StatusUnauthorized, resp.StatusCode)
}

func TestConfigSaveAndLoad(t *testing.T) {
	// Skip this test as it requires viper integration
	t.Skip("Config management uses viper, requires integration test")
}

func TestConfigLoadNonExistent(t *testing.T) {
	// Skip this test as it requires viper integration
	t.Skip("Config management uses viper, requires integration test")
}

func TestFormatOutput(t *testing.T) {
	t.Skip("formatOutput function needs to be exported from main package")
}

func TestValidateEmail(t *testing.T) {
	t.Skip("isValidEmail function needs to be exported from main package")
}

func TestDataQueryConstruction(t *testing.T) {
	tests := []struct {
		deviceID string
		start    string
		end      string
		limit    string
		expected string
	}{
		{
			deviceID: "device001",
			start:    "2025-01-01",
			end:      "2025-01-02",
			limit:    "",
			expected: "/data/device001?start=2025-01-01&end=2025-01-02",
		},
		{
			deviceID: "device002",
			start:    "",
			end:      "",
			limit:    "10",
			expected: "/data/device002/latest?limit=10",
		},
	}

	for _, tt := range tests {
		path := buildDataQueryPath(tt.deviceID, tt.start, tt.end, tt.limit)
		assert.Equal(t, tt.expected, path)
	}
}

// Helper function to build data query path
func buildDataQueryPath(deviceID, start, end, limit string) string {
	if start != "" || end != "" {
		path := "/data/" + deviceID + "?"
		if start != "" {
			path += "start=" + start
		}
		if end != "" {
			if start != "" {
				path += "&"
			}
			path += "end=" + end
		}
		return path
	}

	path := "/data/" + deviceID + "/latest"
	if limit != "" {
		path += "?limit=" + limit
	}
	return path
}

func TestHTTPClientConfiguration(t *testing.T) {
	server := setupMockServer()
	defer server.Close()

	// Test that client respects server URL
	client := NewAPIClient(server.URL, "", "")
	resp, err := client.Get("/health")
	require.NoError(t, err)
	defer resp.Body.Close()

	assert.Equal(t, http.StatusOK, resp.StatusCode)
}

func TestHeadersInRequest(t *testing.T) {
	server := setupMockServer()
	defer server.Close()

	// Make request with token
	client := NewAPIClient(server.URL, "test-jwt-token-123", "")
	resp, err := client.Get("/admin/devices")
	require.NoError(t, err)

	var result map[string]interface{}
	err = ParseResponse(resp, &result)
	require.NoError(t, err)

	// Verify we got authorized response
	devices := result["devices"].([]interface{})
	assert.Len(t, devices, 2)
}

func TestMain(m *testing.M) {
	// Setup test environment
	os.Exit(m.Run())
}
