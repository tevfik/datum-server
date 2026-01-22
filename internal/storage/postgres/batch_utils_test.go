package postgres

import (
	"encoding/json"
	"testing"
	"time"

	"datum-go/internal/storage"

	"github.com/stretchr/testify/assert"
)

func TestBuildBulkInsertQuery(t *testing.T) {
	now := time.Now()

	tests := []struct {
		name          string
		points        []*storage.DataPoint
		expectedQuery string
		expectedArgsCount int
		expectedValidCount int
		expectError   bool
	}{
		{
			name:          "Empty list",
			points:        []*storage.DataPoint{},
			expectedQuery: "",
			expectedArgsCount: 0,
			expectedValidCount: 0,
		},
		{
			name: "Single point",
			points: []*storage.DataPoint{
				{
					DeviceID:  "dev1",
					Timestamp: now,
					Data:      map[string]interface{}{"temp": 25},
				},
			},
			expectedQuery: "INSERT INTO data_points (time, device_id, data) VALUES ($1, $2, $3)",
			expectedArgsCount: 3,
			expectedValidCount: 1,
		},
		{
			name: "Multiple points",
			points: []*storage.DataPoint{
				{
					DeviceID:  "dev1",
					Timestamp: now,
					Data:      map[string]interface{}{"temp": 25},
				},
				{
					DeviceID:  "dev2",
					Timestamp: now,
					Data:      map[string]interface{}{"temp": 30},
				},
			},
			expectedQuery: "INSERT INTO data_points (time, device_id, data) VALUES ($1, $2, $3),($4, $5, $6)",
			expectedArgsCount: 6,
			expectedValidCount: 2,
		},
		{
			name: "Point with marshalling error (skipped)",
			points: []*storage.DataPoint{
				{
					DeviceID:  "dev1",
					Timestamp: now,
					Data:      map[string]interface{}{"temp": 25},
				},
				{
					DeviceID:  "dev2",
					Timestamp: now,
					Data:      map[string]interface{}{"invalid": make(chan int)}, // Channels cannot be marshalled
				},
				{
					DeviceID:  "dev3",
					Timestamp: now,
					Data:      map[string]interface{}{"humidity": 50},
				},
			},
			expectedQuery: "INSERT INTO data_points (time, device_id, data) VALUES ($1, $2, $3),($4, $5, $6)",
			expectedArgsCount: 6, // 3 points, 1 skipped -> 2 points * 3 args = 6
			expectedValidCount: 2,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			query, args, validPoints, err := buildBulkInsertQuery(tt.points)
			if tt.expectError {
				assert.Error(t, err)
			} else {
				assert.NoError(t, err)
				assert.Equal(t, tt.expectedQuery, query)
				assert.Equal(t, tt.expectedArgsCount, len(args))
				assert.Equal(t, tt.expectedValidCount, len(validPoints))

				if len(args) > 0 {
					// Verify content of first arg set
					assert.Equal(t, tt.points[0].Timestamp, args[0])
					assert.Equal(t, tt.points[0].DeviceID, args[1])

					// Verify JSON bytes
					expectedJSON, _ := json.Marshal(tt.points[0].Data)
					assert.Equal(t, expectedJSON, args[2])
				}
			}
		})
	}
}
