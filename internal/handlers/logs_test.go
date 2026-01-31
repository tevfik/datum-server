package handlers

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// setupLogFile creates a temporary log file with the given size (in bytes) and line length.
// It returns the path to the file and a cleanup function.
func setupLogFile(t testing.TB, size int, lineLen int) (string, func()) {
	tmpDir := t.TempDir()
	logPath := filepath.Join(tmpDir, "benchmark.log")
	f, err := os.Create(logPath)
	require.NoError(t, err)

	// Write lines until size is reached
	written := 0
	lineNum := 0
	for written < size {
		line := fmt.Sprintf("Log line %d: %s", lineNum, strings.Repeat("a", lineLen))
		// Ensure lines are separated by newline
		if written+len(line)+1 > size {
			// Trim last line to fit exactly if needed, or just stop
			break
		}
		_, err := f.WriteString(line + "\n")
		require.NoError(t, err)
		written += len(line) + 1
		lineNum++
	}
	f.Close()

	return logPath, func() {
		os.RemoveAll(tmpDir)
	}
}

func TestReadLastLines(t *testing.T) {
	// Create a file with known content
	tmpDir := t.TempDir()
	logPath := filepath.Join(tmpDir, "test.log")

	lines := []string{
		"Line 1",
		"Line 2",
		"Line 3",
		"Line 4",
		"Line 5",
	}
	content := strings.Join(lines, "\n") + "\n" // Log files usually end with newline
	err := os.WriteFile(logPath, []byte(content), 0644)
	require.NoError(t, err)

	tests := []struct {
		name     string
		n        int
		expected []string
	}{
		{
			name:     "Read less than total",
			n:        3,
			// Expects empty string at end due to trailing newline, just like strings.Split
			expected: []string{"Line 4", "Line 5", ""},
		},
		{
			name:     "Read exact total",
			n:        5,
			expected: []string{"Line 2", "Line 3", "Line 4", "Line 5", ""},
		},
		{
			name:     "Read more than total",
			n:        10,
			// strings.Split returns len(lines)+1 elements
			expected: []string{"Line 1", "Line 2", "Line 3", "Line 4", "Line 5", ""},
		},
		{
			name:     "Read one line",
			n:        1,
			expected: []string{""},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result, err := readLastLines(logPath, tt.n)
			assert.NoError(t, err)
			assert.Equal(t, tt.expected, result)
		})
	}
}

func BenchmarkReadLastLines(b *testing.B) {
	// Create a 10MB file
	logPath, cleanup := setupLogFile(b, 10*1024*1024, 100)
	defer cleanup()

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_, err := readLastLines(logPath, 500)
		if err != nil {
			b.Fatal(err)
		}
	}
}
