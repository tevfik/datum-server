package main

import (
	"encoding/json"
	"fmt"
	"os"
	"time"

	"github.com/olekukonko/tablewriter"
	"github.com/spf13/cobra"
)

var dataCmd = &cobra.Command{
	Use:   "data",
	Short: "Query device data",
	Long:  "Query and retrieve time-series data from devices",
}

var dataGetCmd = &cobra.Command{
	Use:   "get",
	Short: "Get device data",
	Long: `Get time-series data for a device.

Use time filters to retrieve data from specific time ranges.`,
	Example: `  # Get last hour of data
  datumctl data get --device my-device --last 1h

  # Get data from specific time range
  datumctl data get --device my-device --from "2025-12-25 10:00" --to "2025-12-25 12:00"

  # Get latest 10 readings
  datumctl data get --device my-device --limit 10`,
	RunE: runDataGet,
}

var dataPostCmd = &cobra.Command{
	Use:   "post",
	Short: "Post data to device",
	Long: `Send data to a device endpoint.

Useful for testing or manual data ingestion.`,
	Example: `  # Post temperature data
  datumctl data post --device my-device --data '{"temperature": 25.5, "humidity": 60}'

  # Post with API key
  datumctl data post --api-key device-key-123 --data '{"temperature": 25.5}'`,
	RunE: runDataPost,
}

var dataStatsCmd = &cobra.Command{
	Use:   "stats",
	Short: "Get data statistics",
	Long:  "Get aggregated statistics for device data",
	Example: `  # Get device stats
  datumctl data stats --device my-device --last 24h`,
	RunE: runDataStats,
}

var (
	dataDevice string
	dataLast   string
	dataFrom   string
	dataTo     string
	dataLimit  int
	dataJSON   string
)

func init() {
	rootCmd.AddCommand(dataCmd)

	dataCmd.AddCommand(dataGetCmd)
	dataCmd.AddCommand(dataPostCmd)
	dataCmd.AddCommand(dataStatsCmd)

	dataGetCmd.Flags().StringVar(&dataDevice, "device", "", "Device ID (required)")
	dataGetCmd.Flags().StringVar(&dataLast, "last", "", "Get last N time (e.g., 1h, 30m, 7d)")
	dataGetCmd.Flags().StringVar(&dataFrom, "from", "", "Start time (RFC3339 or YYYY-MM-DD HH:MM)")
	dataGetCmd.Flags().StringVar(&dataTo, "to", "", "End time (RFC3339 or YYYY-MM-DD HH:MM)")
	dataGetCmd.Flags().IntVar(&dataLimit, "limit", 100, "Maximum number of results")
	dataGetCmd.MarkFlagRequired("device")

	dataPostCmd.Flags().StringVar(&dataDevice, "device", "", "Device ID")
	dataPostCmd.Flags().StringVar(&dataJSON, "data", "", "JSON data to post (required)")
	dataPostCmd.MarkFlagRequired("data")

	dataStatsCmd.Flags().StringVar(&dataDevice, "device", "", "Device ID (required)")
	dataStatsCmd.Flags().StringVar(&dataLast, "last", "24h", "Time range for stats")
	dataStatsCmd.MarkFlagRequired("device")
}

func runDataGet(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	// Build query parameters
	path := fmt.Sprintf("/api/data/%s", dataDevice)
	queryParams := ""

	if dataLast != "" {
		duration, err := parseDuration(dataLast)
		if err != nil {
			return fmt.Errorf("invalid duration: %w", err)
		}
		from := time.Now().Add(-duration)
		queryParams = fmt.Sprintf("?from=%s&limit=%d", from.Format(time.RFC3339), dataLimit)
	} else if dataFrom != "" {
		fromTime, err := parseTime(dataFrom)
		if err != nil {
			return fmt.Errorf("invalid from time: %w", err)
		}
		queryParams = fmt.Sprintf("?from=%s&limit=%d", fromTime.Format(time.RFC3339), dataLimit)

		if dataTo != "" {
			toTime, err := parseTime(dataTo)
			if err != nil {
				return fmt.Errorf("invalid to time: %w", err)
			}
			queryParams += fmt.Sprintf("&to=%s", toTime.Format(time.RFC3339))
		}
	}

	resp, err := client.Get(path + queryParams)
	if err != nil {
		return err
	}

	var data []map[string]interface{}
	if err := ParseResponse(resp, &data); err != nil {
		return err
	}

	if outputJSON {
		return printJSON(data)
	}

	// Print as table
	if len(data) == 0 {
		fmt.Println("No data found")
		return nil
	}

	table := tablewriter.NewWriter(os.Stdout)

	// Get all unique keys
	keys := make([]string, 0)
	keyMap := make(map[string]bool)
	for _, item := range data {
		for key := range item {
			if key != "timestamp" && key != "device_id" && !keyMap[key] {
				keys = append(keys, key)
				keyMap[key] = true
			}
		}
	}

	// Build header
	headerArgs := []any{"Timestamp"}
	for _, key := range keys {
		headerArgs = append(headerArgs, key)
	}
	table.Header(headerArgs...)

	// Add rows
	for _, item := range data {
		rowArgs := []any{getString(item, "timestamp")}
		for _, key := range keys {
			rowArgs = append(rowArgs, getString(item, key))
		}
		table.Append(rowArgs...)
	}

	fmt.Printf("\n📊 Data for device: %s (%d records)\n\n", dataDevice, len(data))
	table.Render()
	return nil
}

func runDataPost(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	// Parse JSON data
	var jsonData map[string]interface{}
	if err := json.Unmarshal([]byte(dataJSON), &jsonData); err != nil {
		return fmt.Errorf("invalid JSON: %w", err)
	}

	// Determine endpoint
	var path string
	if dataDevice != "" {
		path = fmt.Sprintf("/api/data/%s", dataDevice)
	} else {
		path = "/public/data"
	}

	resp, err := client.Post(path, jsonData)
	if err != nil {
		return err
	}

	if err := ParseResponse(resp, nil); err != nil {
		return err
	}

	fmt.Printf("✅ Data posted successfully\n")
	return nil
}

func runDataStats(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	duration, err := parseDuration(dataLast)
	if err != nil {
		return fmt.Errorf("invalid duration: %w", err)
	}
	from := time.Now().Add(-duration)

	path := fmt.Sprintf("/api/data/%s/stats?from=%s", dataDevice, from.Format(time.RFC3339))

	resp, err := client.Get(path)
	if err != nil {
		return err
	}

	var stats map[string]interface{}
	if err := ParseResponse(resp, &stats); err != nil {
		return err
	}

	if outputJSON {
		return printJSON(stats)
	}

	fmt.Printf("\n📈 Statistics for device: %s (last %s)\n\n", dataDevice, dataLast)
	fmt.Printf("  Count:      %v\n", stats["count"])
	fmt.Printf("  First:      %v\n", stats["first_timestamp"])
	fmt.Printf("  Last:       %v\n", stats["last_timestamp"])

	if metrics, ok := stats["metrics"].(map[string]interface{}); ok {
		fmt.Printf("\n  Metrics:\n")
		for key, value := range metrics {
			if metricStats, ok := value.(map[string]interface{}); ok {
				fmt.Printf("    %s:\n", key)
				fmt.Printf("      Min:  %v\n", metricStats["min"])
				fmt.Printf("      Max:  %v\n", metricStats["max"])
				fmt.Printf("      Avg:  %v\n", metricStats["avg"])
			}
		}
	}
	fmt.Println()

	return nil
}

func parseDuration(s string) (time.Duration, error) {
	return time.ParseDuration(s)
}

func parseTime(s string) (time.Time, error) {
	// Try RFC3339 first
	t, err := time.Parse(time.RFC3339, s)
	if err == nil {
		return t, nil
	}

	// Try common format
	t, err = time.Parse("2006-01-02 15:04", s)
	if err == nil {
		return t, nil
	}

	return time.Time{}, fmt.Errorf("unsupported time format (use RFC3339 or 'YYYY-MM-DD HH:MM')")
}
