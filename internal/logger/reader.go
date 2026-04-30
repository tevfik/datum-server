package logger

import (
	"bufio"
	"os"
	"strings"
)

// GetRecentLogs reads the last N lines from the log file with optional filtering
func GetRecentLogs(limit int, level string, search string) ([]string, error) {
	if LogFilePath == "" {
		return []string{}, nil
	}

	file, err := os.Open(LogFilePath)
	if err != nil {
		if os.IsNotExist(err) {
			return []string{}, nil
		}
		return nil, err
	}
	defer file.Close()

	var lines []string
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := scanner.Text()

		// Filter by level (case-insensitive) - simple string match for now since logs are JSON
		if level != "" && !strings.Contains(strings.ToLower(line), strings.ToLower(level)) {
			continue
		}

		// Filter by search query
		if search != "" && !strings.Contains(strings.ToLower(line), strings.ToLower(search)) {
			continue
		}

		lines = append(lines, line)
	}

	if err := scanner.Err(); err != nil {
		return nil, err
	}

	if len(lines) <= limit {
		return lines, nil
	}
	return lines[len(lines)-limit:], nil
}
