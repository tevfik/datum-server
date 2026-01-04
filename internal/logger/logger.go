package logger

import (
	"os"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
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

	displayFormat := logFormat
	if displayFormat == "" {
		displayFormat = "pretty (default)"
	}

	Logger.Info().
		Str("configured_level", logLevel).
		Str("format", displayFormat).
		Msg("Logger initialized")
}

// GetLogger returns the configured logger instance
func GetLogger() *zerolog.Logger {
	return &Logger
}

// GinLogger is a middleware that logs request details using zerolog
// It filters out noisy endpoints like /stream/frame
func GinLogger() gin.HandlerFunc {
	return func(c *gin.Context) {
		start := time.Now()
		path := c.Request.URL.Path
		raw := c.Request.URL.RawQuery

		// Process request
		c.Next()

		// Skip logging for stream frames to avoid spamming logs
		if strings.Contains(path, "/stream/frame") {
			return
		}

		end := time.Now()
		latency := end.Sub(start)

		msg := "Request"
		if len(c.Errors) > 0 {
			msg = c.Errors.String()
		}

		logger := log.Logger
		event := logger.Info()

		if c.Writer.Status() >= 400 {
			event = logger.Error()
		}

		event.
			Str("method", c.Request.Method).
			Str("path", path).
			Str("query", raw).
			Int("status", c.Writer.Status()).
			Dur("latency", latency).
			Str("ip", c.ClientIP()).
			Msg(msg)
	}
}
