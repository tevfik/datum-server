package logger

import (
	"os"
	"testing"

	"github.com/rs/zerolog"
	"github.com/stretchr/testify/assert"
)

func TestInitLoggerDefaultLevel(t *testing.T) {
	// Clear environment
	os.Unsetenv("LOG_LEVEL")
	os.Unsetenv("LOG_FORMAT")

	InitLogger("")

	// Default level should be INFO
	assert.Equal(t, zerolog.InfoLevel, Logger.GetLevel())
}

func TestInitLoggerDebugLevel(t *testing.T) {
	os.Setenv("LOG_LEVEL", "DEBUG")
	defer os.Unsetenv("LOG_LEVEL")

	InitLogger("")

	assert.Equal(t, zerolog.DebugLevel, Logger.GetLevel())
}

func TestInitLoggerWarnLevel(t *testing.T) {
	os.Setenv("LOG_LEVEL", "WARN")
	defer os.Unsetenv("LOG_LEVEL")

	InitLogger("")

	assert.Equal(t, zerolog.WarnLevel, Logger.GetLevel())
}

func TestInitLoggerWarningLevel(t *testing.T) {
	os.Setenv("LOG_LEVEL", "WARNING")
	defer os.Unsetenv("LOG_LEVEL")

	InitLogger("")

	assert.Equal(t, zerolog.WarnLevel, Logger.GetLevel())
}

func TestInitLoggerErrorLevel(t *testing.T) {
	os.Setenv("LOG_LEVEL", "ERROR")
	defer os.Unsetenv("LOG_LEVEL")

	InitLogger("")

	assert.Equal(t, zerolog.ErrorLevel, Logger.GetLevel())
}

func TestInitLoggerFatalLevel(t *testing.T) {
	os.Setenv("LOG_LEVEL", "FATAL")
	defer os.Unsetenv("LOG_LEVEL")

	InitLogger("")

	assert.Equal(t, zerolog.FatalLevel, Logger.GetLevel())
}

func TestInitLoggerInvalidLevel(t *testing.T) {
	os.Setenv("LOG_LEVEL", "INVALID")
	defer os.Unsetenv("LOG_LEVEL")

	InitLogger("")

	// Should default to INFO for invalid levels
	assert.Equal(t, zerolog.InfoLevel, Logger.GetLevel())
}

func TestInitLoggerLowercaseLevel(t *testing.T) {
	os.Setenv("LOG_LEVEL", "debug")
	defer os.Unsetenv("LOG_LEVEL")

	InitLogger("")

	// Should handle lowercase input
	assert.Equal(t, zerolog.DebugLevel, Logger.GetLevel())
}

func TestInitLoggerPrettyFormat(t *testing.T) {
	os.Setenv("LOG_FORMAT", "pretty")
	defer os.Unsetenv("LOG_FORMAT")

	// Should not panic
	InitLogger("")
	assert.NotNil(t, Logger)
}

func TestInitLoggerJSONFormat(t *testing.T) {
	os.Setenv("LOG_FORMAT", "json")
	defer os.Unsetenv("LOG_FORMAT")

	// Should not panic
	InitLogger("")
	assert.NotNil(t, Logger)
}

func TestGetLogger(t *testing.T) {
	InitLogger("")

	logger := GetLogger()

	assert.NotNil(t, logger)
	assert.Equal(t, &Logger, logger)
}
