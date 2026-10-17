package main

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
)

func TestStorageRoutes(t *testing.T) {
	gin.SetMode(gin.TestMode)
	r := gin.New()

	tempDir, err := os.MkdirTemp("", "datum_storage_test_*")
	assert.NoError(t, err)
	defer os.RemoveAll(tempDir)

	RegisterStorageRoutes(r, tempDir, nil)

	// Test 1: GET /buckets (default buckets should exist)
	req, _ := http.NewRequest(http.MethodGet, "/buckets", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	var resp map[string]interface{}
	err = json.Unmarshal(w.Body.Bytes(), &resp)
	assert.NoError(t, err)
	buckets, ok := resp["buckets"].([]interface{})
	assert.True(t, ok)
	assert.GreaterOrEqual(t, len(buckets), 4)

	// Test 2: Put a file into firmware bucket and list objects via GET /buckets/firmware
	firmwareBucketDir := filepath.Join(tempDir, "storage", "firmware")
	err = os.WriteFile(filepath.Join(firmwareBucketDir, "test_v1.bin"), []byte("firmware-binary-data"), 0644)
	assert.NoError(t, err)

	req, _ = http.NewRequest(http.MethodGet, "/buckets/firmware", nil)
	w = httptest.NewRecorder()
	r.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	var objResp map[string]interface{}
	err = json.Unmarshal(w.Body.Bytes(), &objResp)
	assert.NoError(t, err)
	objects, ok := objResp["objects"].([]interface{})
	assert.True(t, ok)
	assert.Equal(t, 1, len(objects))

	// Test 3: Download object via GET /buckets/firmware/test_v1.bin
	req, _ = http.NewRequest(http.MethodGet, "/buckets/firmware/test_v1.bin", nil)
	w = httptest.NewRecorder()
	r.ServeHTTP(w, req)
	assert.Equal(t, http.StatusOK, w.Code)
	assert.Equal(t, "firmware-binary-data", w.Body.String())

	// Test 4: Check /storage alias
	req, _ = http.NewRequest(http.MethodGet, "/storage", nil)
	w = httptest.NewRecorder()
	r.ServeHTTP(w, req)
	assert.Equal(t, http.StatusOK, w.Code)
}
