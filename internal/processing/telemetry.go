package processing

import (
	"fmt"
	"sync"
	"time"

	"datum-go/internal/storage"
)

// TelemetryProcessor handles the processing of incoming device telemetry
type TelemetryProcessor struct {
	Store         storage.Provider
	dataChan      chan *storage.DataPoint
	flushInterval time.Duration
	batchSize     int
	wg            sync.WaitGroup
}

// NewTelemetryProcessor creates a new instance with async batch processing
func NewTelemetryProcessor(store storage.Provider) *TelemetryProcessor {
	tp := &TelemetryProcessor{
		Store:         store,
		dataChan:      make(chan *storage.DataPoint, 50000), // Increased buffer for high throughput (bursts)
		flushInterval: 500 * time.Millisecond,
		batchSize:     5000,
	}

	// Start worker pool
	// 5-8 workers is a good starting point for I/O bound tasks
	workerCount := 8
	for i := 0; i < workerCount; i++ {
		tp.wg.Add(1)
		go tp.worker()
	}

	return tp
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
			fmt.Printf("ERROR: Failed to store data batch: %v\n", err)
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
			fmt.Printf("WARN: Failed to parse timestamp '%s': %v\n", tStr, err)
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
		return nil, fmt.Errorf("telemetry buffer full, dropping data")
	}

	// Check for pending commands to notify the device/response
	// Note: Use a cached or lightweight check if possible, reading pending command count is fast (in-memory usually)
	commandsPending := tp.Store.GetPendingCommandCount(deviceID)

	return &ProcessingResult{
		Timestamp:       point.Timestamp,
		CommandsPending: int(commandsPending),
	}, nil
}
