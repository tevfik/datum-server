package storage

import (
	"encoding/json"
	"strings"

	"github.com/tidwall/buntdb"
)

// GetUserDeviceCounts returns the number of devices for each user
func (s *Storage) GetUserDeviceCounts() (map[string]int, error) {
	counts := make(map[string]int)
	err := s.db.View(func(tx *buntdb.Tx) error {
		tx.AscendKeys("user:*:devices", func(key, value string) bool {
			// key: user:{userID}:devices
			parts := strings.Split(key, ":")
			if len(parts) == 3 && parts[0] == "user" && parts[2] == "devices" {
				userID := parts[1]
				var devices []string
				if json.Unmarshal([]byte(value), &devices) == nil {
					counts[userID] = len(devices)
				}
			}
			return true
		})
		return nil
	})
	return counts, err
}
