package main

import (
	"os"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
)

// Test utility functions

func TestPathExists(t *testing.T) {
	// Test with existing file
	tmpFile, err := os.CreateTemp("", "test-path-*.txt")
	assert.NoError(t, err)
	defer os.Remove(tmpFile.Name())

	assert.True(t, pathExists(tmpFile.Name()))

	// Test with non-existing path
	assert.False(t, pathExists("/nonexistent/path/file.txt"))

	// Test with existing directory
	tmpDir := os.TempDir()
	assert.True(t, pathExists(tmpDir))
}

func TestTimeNow(t *testing.T) {
	before := time.Now()
	result := timeNow()
	after := time.Now()

	assert.True(t, result.After(before) || result.Equal(before))
	assert.True(t, result.Before(after) || result.Equal(after))
}
