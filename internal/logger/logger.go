package logger

import (
	"io"
	"os"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/rs/zerolog"
	"github.com/rs/zerolog/log"
)

var Logger zerolog.Logger

var LogFilePath string

// InitLogger initializes the global structured logger
func InitLogger(logPath string) {
	LogFilePath = logPath

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

	var logWriter zerolog.LevelWriter

	// Configure pretty logging for development (console)
	consoleWriter := zerolog.ConsoleWriter{
		Out:        os.Stdout,
		TimeFormat: time.RFC3339,
	}

	if logPath != "" {
		// Create/Open log file
		file, err := os.OpenFile(logPath, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
		if err != nil {
			// Fallback to console only if file fails
			log.Error().Err(err).Msg("Failed to open log file")
			logWriter = multiLevelWriter{consoleWriter}
		} else {
			// Write to both console and file
			// Note: File will get JSON logs, Console gets pretty logs if configured
			// For simplicity, let's write JSON to file and Pretty to console
			logWriter = zerolog.MultiLevelWriter(consoleWriter, file)
		}
	} else {
		logWriter = multiLevelWriter{consoleWriter}
	}

	// Configure global logger
	Logger = zerolog.New(logWriter).
		Level(level).
		With().
		Timestamp().
		Caller().
		Logger()

	// Set as global logger
	log.Logger = Logger

	Logger.Info().
		Str("configured_level", logLevel).
		Str("log_file", logPath).
		Msg("Logger initialized")
}

// multiLevelWriter adapts a writer to LevelWriter interface (needed for zerolog 1.20+)
type multiLevelWriter struct {
	io.Writer
}

func (w multiLevelWriter) WriteLevel(l zerolog.Level, p []byte) (n int, err error) {
	return w.Write(p)
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

		// Skip logging for command polling to avoid spamming logs
		if strings.HasSuffix(path, "/commands") && c.Request.Method == "GET" {
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
