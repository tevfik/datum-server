package processing

import (
	"fmt"
	"os"
	"strconv"
	"sync"
	"sync/atomic"
	"time"

	"datum-go/internal/logger"
	"datum-go/internal/storage"
)

// TelemetryProcessor handles the processing of incoming device telemetry
type TelemetryProcessor struct {
	Store         storage.Provider
	dataChan      chan *storage.DataPoint
	flushInterval time.Duration
	batchSize     int
	wg            sync.WaitGroup
	droppedCount  uint64 // atomic counter for dropped data points
}

// NewTelemetryProcessor creates a new instance with async batch processing
func NewTelemetryProcessor(store storage.Provider) *TelemetryProcessor {
	bufSize := 10000
	if v := os.Getenv("TELEMETRY_BUFFER_SIZE"); v != "" {
		if parsed, err := strconv.Atoi(v); err == nil && parsed > 0 {
			bufSize = parsed
		}
	}

	tp := &TelemetryProcessor{
		Store:         store,
		dataChan:      make(chan *storage.DataPoint, bufSize),
		flushInterval: 500 * time.Millisecond,
		batchSize:     5000,
	}

	// Start worker pool
	workerCount := 8
	for i := 0; i < workerCount; i++ {
		tp.wg.Add(1)
		go tp.worker()
	}

	return tp
}

// DroppedCount returns the number of data points dropped due to full buffer.
func (tp *TelemetryProcessor) DroppedCount() uint64 {
	return atomic.LoadUint64(&tp.droppedCount)
}

// BufferUsage returns the current buffer utilization as a fraction (0.0 - 1.0).
func (tp *TelemetryProcessor) BufferUsage() float64 {
	return float64(len(tp.dataChan)) / float64(cap(tp.dataChan))
}

// Close gracefully shuts down the processor
func (tp *TelemetryProcessor) Close() {
	close(tp.dataChan)
	tp.wg.Wait()
}

func (tp *TelemetryProcessor) worker() {
	defer tp.wg.Done()

	buffer := make([]*storage.DataPoint, 0, tp.batchSize)
	ticker := time.NewTicker(tp.flushInterval)
	defer ticker.Stop()

	flush := func() {
		if len(buffer) == 0 {
			return
		}
		if err := tp.Store.StoreDataBatch(buffer); err != nil {
			logger.GetLogger().Error().Err(err).Int("batch_size", len(buffer)).Msg("Failed to store data batch")
		}
		buffer = make([]*storage.DataPoint, 0, tp.batchSize)
	}

	for {
		select {
		case point, ok := <-tp.dataChan:
			if !ok {
				flush()
				return
			}
			buffer = append(buffer, point)
			if len(buffer) >= tp.batchSize {
				flush()
			}
		case <-ticker.C:
			flush()
		}
	}
}

// ProcessingResult contains the result of a telemetry processing operation
type ProcessingResult struct {
	Timestamp       time.Time
	CommandsPending int
}

// Process handles the ingestion of a telemetry data point asynchronously
func (tp *TelemetryProcessor) Process(deviceID string, data map[string]interface{}) (*ProcessingResult, error) {
	data["server_time"] = time.Now().Unix()

	ts := time.Now()
	if tStr, ok := data["timestamp"].(string); ok {
		if t, err := time.Parse(time.RFC3339, tStr); err == nil {
			ts = t
		} else {
			logger.GetLogger().Warn().Str("timestamp", tStr).Err(err).Msg("Failed to parse telemetry timestamp")
		}
	}

	point := &storage.DataPoint{
		DeviceID:  deviceID,
		Timestamp: ts,
		Data:      data,
	}

	// Async Push
	select {
	case tp.dataChan <- point:
		// Success
	default:
		atomic.AddUint64(&tp.droppedCount, 1)
		return nil, fmt.Errorf("telemetry buffer full, dropping data (total dropped: %d)", atomic.LoadUint64(&tp.droppedCount))
	}

	// Check for pending commands to notify the device/response
	// Note: Use a cached or lightweight check if possible, reading pending command count is fast (in-memory usually)
	commandsPending := tp.Store.GetPendingCommandCount(deviceID)

	return &ProcessingResult{
		Timestamp:       point.Timestamp,
		CommandsPending: int(commandsPending),
	}, nil
}
