package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"

	"github.com/spf13/cobra"
)

var (
	deviceUpdateName   string
	deviceUpdateType   string
	deviceUpdateStatus string
)

var updateDeviceCmd = &cobra.Command{
	Use:   "update <device_id>",
	Short: "Update device properties",
	Long:  `Update device name, type, or status.`,
	Example: `  # Update device name
  datumctl device update device-123 --name "New Name"

  # Update device type and status
  datumctl device update device-123 --type sensor --status active`,
	Args: cobra.ExactArgs(1),
	RunE: runUpdateDevice,
}

func init() {
	deviceCmd.AddCommand(updateDeviceCmd)
	updateDeviceCmd.Flags().StringVar(&deviceUpdateName, "name", "", "New device name")
	updateDeviceCmd.Flags().StringVar(&deviceUpdateType, "type", "", "New device type")
	updateDeviceCmd.Flags().StringVar(&deviceUpdateStatus, "status", "", "New device status (active, suspended, banned)")
}

func runUpdateDevice(cmd *cobra.Command, args []string) error {
	loadConfig()

	deviceID := args[0]

	// Build update payload
	updates := make(map[string]interface{})
	if deviceUpdateName != "" {
		updates["name"] = deviceUpdateName
	}
	if deviceUpdateType != "" {
		updates["type"] = deviceUpdateType
	}
	if deviceUpdateStatus != "" {
		updates["status"] = deviceUpdateStatus
	}

	if len(updates) == 0 {
		return fmt.Errorf("no updates specified. Use --name, --type, or --status")
	}

	client := &http.Client{}
	jsonData, _ := json.Marshal(updates)

	req, err := http.NewRequest("PATCH", serverURL+"/devices/"+deviceID, strings.NewReader(string(jsonData)))
	if err != nil {
		return err
	}

	req.Header.Set("Content-Type", "application/json")
	if token != "" {
		req.Header.Set("Authorization", "Bearer "+token)
	}

	resp, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("request failed: %w", err)
	}
	defer resp.Body.Close()

	body, _ := io.ReadAll(resp.Body)

	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("update failed (%d): %s", resp.StatusCode, string(body))
	}

	if outputJSON {
		fmt.Println(string(body))
	} else {
		var result map[string]interface{}
		if err := json.Unmarshal(body, &result); err == nil {
			fmt.Printf("\n✅ Device %s updated successfully\n\n", deviceID)
			if name, ok := result["name"].(string); ok {
				fmt.Printf("Name: %s\n", name)
			}
			if dtype, ok := result["type"].(string); ok {
				fmt.Printf("Type: %s\n", dtype)
			}
			if status, ok := result["status"].(string); ok {
				fmt.Printf("Status: %s\n", status)
			}
			fmt.Println()
		} else {
			fmt.Println(string(body))
		}
	}

	return nil
}
