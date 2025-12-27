package logger

import (
	"os"
	"strings"
	"time"

	"github.com/rs/zerolog"
	"github.com/rs/zerolog/log"
)

var Logger zerolog.Logger

// InitLogger initializes the global structured logger
func InitLogger() {
	// Configure based on environment
	logLevel := strings.ToUpper(os.Getenv("LOG_LEVEL"))
	if logLevel == "" {
		logLevel = "INFO"
	}

	// Set log level
	var level zerolog.Level
	switch logLevel {
	case "DEBUG":
		level = zerolog.DebugLevel
	case "INFO":
		level = zerolog.InfoLevel
	case "WARN", "WARNING":
		level = zerolog.WarnLevel
	case "ERROR":
		level = zerolog.ErrorLevel
	case "FATAL":
		level = zerolog.FatalLevel
	default:
		level = zerolog.InfoLevel
	}

	// Configure pretty logging for development
	logFormat := os.Getenv("LOG_FORMAT")
	if logFormat == "pretty" || logFormat == "" {
		// Human-readable console output with colors
		output := zerolog.ConsoleWriter{
			Out:        os.Stdout,
			TimeFormat: time.RFC3339,
		}
		Logger = zerolog.New(output).
			Level(level).
			With().
			Timestamp().
			Caller().
			Logger()
	} else {
		// JSON format for production
		Logger = zerolog.New(os.Stdout).
			Level(level).
			With().
			Timestamp().
			Caller().
			Logger()
	}

	// Set as global logger
	log.Logger = Logger

	Logger.Info().
		Str("level", logLevel).
		Str("format", logFormat).
		Msg("Logger initialized")
}

// GetLogger returns the configured logger instance
func GetLogger() *zerolog.Logger {
	return &Logger
}
