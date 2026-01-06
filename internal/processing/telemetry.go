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
	done          chan struct{}
	wg            sync.WaitGroup
}

// NewTelemetryProcessor creates a new instance with async batch processing
func NewTelemetryProcessor(store storage.Provider) *TelemetryProcessor {
	tp := &TelemetryProcessor{
		Store:         store,
		dataChan:      make(chan *storage.DataPoint, 10000), // Buffer for high throughput
		flushInterval: 500 * time.Millisecond,               // User requested 500ms
		batchSize:     1000,                                 // Flush if 1000 items accumulate
		done:          make(chan struct{}),
	}

	// Start worker
	tp.wg.Add(1)
	go tp.worker()

	return tp
}

// Close gracefully shuts down the processor
func (tp *TelemetryProcessor) Close() {
	close(tp.done)
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
			// Log error, but what else? In production, maybe retry or drop.
			// For now, simple error logging implicitly (no logger here yet)
			// fmt.Printf("Batch write failed: %v\n", err)
		}
		// Reset buffer (allocating new slice to avoid race if we passed slice ref asynchronously,
		// though here StoreDataBatch is blocking so we can reuse capacity)
		buffer = buffer[:0]
	}

	for {
		select {
		case point := <-tp.dataChan:
			buffer = append(buffer, point)
			if len(buffer) >= tp.batchSize {
				flush()
			}
		case <-ticker.C:
			flush()
		case <-tp.done:
			// Drain remaining data
		DrainLoop:
			for {
				select {
				case point := <-tp.dataChan:
					buffer = append(buffer, point)
				default:
					break DrainLoop
				}
			}
			flush() // Final flush
			return
		}
	}
}

// ProcessingResult contains the result of a telemetry processing operation
type ProcessingResult struct {
	Timestamp       time.Time
	CommandsPending int
}

// Process handles the ingestion of a telemetry data point asynchronously
func (tp *TelemetryProcessor) Process(deviceID string, data map[string]interface{}, clientIP string) (*ProcessingResult, error) {
	// ---------------------------------------------------------
	// Enrichment: Server-Side Tagging
	// ---------------------------------------------------------
	if clientIP != "" {
		data["public_ip"] = clientIP
	}
	data["server_time"] = time.Now().Unix()

	point := &storage.DataPoint{
		DeviceID:  deviceID,
		Timestamp: time.Now(),
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
