package metrics

import (
	"runtime"
	"sync/atomic"
	"time"

	"github.com/gin-gonic/gin"
)

// Metrics stores application metrics
type Metrics struct {
	RequestsTotal      uint64
	RequestsSuccess    uint64
	RequestsError      uint64
	DataPointsReceived uint64
	CommandsSent       uint64
	DevicesConnected   uint64
	StartTime          time.Time
}

var globalMetrics = &Metrics{
	StartTime: time.Now(),
}

// GetGlobalMetrics returns the global metrics instance
func GetGlobalMetrics() *Metrics {
	return globalMetrics
}

// Increment metrics atomically
func (m *Metrics) IncrementRequests() {
	atomic.AddUint64(&m.RequestsTotal, 1)
}

func (m *Metrics) IncrementSuccess() {
	atomic.AddUint64(&m.RequestsSuccess, 1)
}

func (m *Metrics) IncrementError() {
	atomic.AddUint64(&m.RequestsError, 1)
}

func (m *Metrics) IncrementDataPoints(count uint64) {
	atomic.AddUint64(&m.DataPointsReceived, count)
}

func (m *Metrics) IncrementCommands() {
	atomic.AddUint64(&m.CommandsSent, 1)
}

// Middleware tracks request metrics
func Middleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		globalMetrics.IncrementRequests()

		c.Next()

		if c.Writer.Status() >= 200 && c.Writer.Status() < 400 {
			globalMetrics.IncrementSuccess()
		} else if c.Writer.Status() >= 400 {
			globalMetrics.IncrementError()
		}
	}
}

// Handler exposes metrics in JSON or Prometheus format
// GET /sys/metrics
func Handler(c *gin.Context) {
	var m runtime.MemStats
	runtime.ReadMemStats(&m)

	uptime := time.Since(globalMetrics.StartTime).Seconds()

	// Note: Store integration removed for decoupling.
	// If needed, we can inject counts via a callback or interface later.
	userCount := 0
	deviceCount := 0

	response := gin.H{
		"service":        "datum-api",
		"uptime_seconds": uptime,
		"requests": gin.H{
			"total":   atomic.LoadUint64(&globalMetrics.RequestsTotal),
			"success": atomic.LoadUint64(&globalMetrics.RequestsSuccess),
			"error":   atomic.LoadUint64(&globalMetrics.RequestsError),
		},
		"data": gin.H{
			"points_received": atomic.LoadUint64(&globalMetrics.DataPointsReceived),
			"commands_sent":   atomic.LoadUint64(&globalMetrics.CommandsSent),
		},
		"resources": gin.H{
			"users":   userCount,
			"devices": deviceCount,
		},
		"system": gin.H{
			"goroutines":      runtime.NumGoroutine(),
			"memory_alloc_mb": m.Alloc / 1024 / 1024,
			"memory_sys_mb":   m.Sys / 1024 / 1024,
			"gc_runs":         m.NumGC,
		},
	}

	// If Prometheus format is requested
	if c.Query("format") == "prometheus" {
		c.Header("Content-Type", "text/plain")
		c.String(200, `# HELP datum_requests_total Total number of HTTP requests
# TYPE datum_requests_total counter
datum_requests_total %d

# HELP datum_requests_success Successful HTTP requests
# TYPE datum_requests_success counter
datum_requests_success %d

# HELP datum_requests_error Failed HTTP requests
# TYPE datum_requests_error counter
datum_requests_error %d

# HELP datum_data_points_total Total data points received
# TYPE datum_data_points_total counter
datum_data_points_total %d

# HELP datum_commands_total Total commands sent
# TYPE datum_commands_total counter
datum_commands_total %d

# HELP datum_uptime_seconds Service uptime in seconds
# TYPE datum_uptime_seconds gauge
datum_uptime_seconds %.2f

# HELP datum_memory_alloc_bytes Allocated memory in bytes
# TYPE datum_memory_alloc_bytes gauge
datum_memory_alloc_bytes %d

# HELP datum_goroutines Number of goroutines
# TYPE datum_goroutines gauge
datum_goroutines %d
`,
			atomic.LoadUint64(&globalMetrics.RequestsTotal),
			atomic.LoadUint64(&globalMetrics.RequestsSuccess),
			atomic.LoadUint64(&globalMetrics.RequestsError),
			atomic.LoadUint64(&globalMetrics.DataPointsReceived),
			atomic.LoadUint64(&globalMetrics.CommandsSent),
			uptime,
			m.Alloc,
			runtime.NumGoroutine(),
		)
		return
	}

	c.JSON(200, response)
}
