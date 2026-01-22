package postgres

import (
	"encoding/json"
	"fmt"
	"strings"

	"datum-go/internal/storage"
)

// buildBulkInsertQuery constructs a bulk INSERT statement for data points
// Returns the query, arguments, and the list of valid points included in the query
func buildBulkInsertQuery(points []*storage.DataPoint) (string, []interface{}, []*storage.DataPoint, error) {
	var args []interface{}
	var placeholders []string
	var validPoints []*storage.DataPoint

	count := 0
	for _, point := range points {
		jsonData, err := json.Marshal(point.Data)
		if err != nil {
			continue
		}

		// 3 params per row: time, device_id, data
		p1 := count*3 + 1
		p2 := count*3 + 2
		p3 := count*3 + 3
		placeholders = append(placeholders, fmt.Sprintf("($%d, $%d, $%d)", p1, p2, p3))

		args = append(args, point.Timestamp, point.DeviceID, jsonData)
		validPoints = append(validPoints, point)
		count++
	}

	if len(args) == 0 {
		return "", nil, nil, nil
	}

	query := "INSERT INTO data_points (time, device_id, data) VALUES " + strings.Join(placeholders, ",")
	return query, args, validPoints, nil
}
