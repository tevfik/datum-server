package processing

import (
	"time"

	"datum-go/internal/storage"
)

// TelemetryProcessor handles the processing of incoming device telemetry
type TelemetryProcessor struct {
	Store storage.Provider
}

// NewTelemetryProcessor creates a new instance
func NewTelemetryProcessor(store storage.Provider) *TelemetryProcessor {
	return &TelemetryProcessor{
		Store: store,
	}
}

// ProcessingResult contains the result of a telemetry processing operation
type ProcessingResult struct {
	Timestamp       time.Time
	CommandsPending int
}

// Process handles the ingestion of a telemetry data point
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

	if err := tp.Store.StoreData(point); err != nil {
		return nil, err
	}

	// Check for pending commands to notify the device/response
	commandsPending := tp.Store.GetPendingCommandCount(deviceID)

	return &ProcessingResult{
		Timestamp:       point.Timestamp,
		CommandsPending: int(commandsPending),
	}, nil
}
