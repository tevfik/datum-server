package main

import (
	"encoding/json"
	"fmt"
	"os"
	"time"

	"github.com/olekukonko/tablewriter"
	"github.com/spf13/cobra"
)

var deviceCmd = &cobra.Command{
	Use:   "device",
	Short: "Manage devices",
	Long:  "Create, list, view, and delete IoT devices",
}

var deviceListCmd = &cobra.Command{
	Use:   "list",
	Short: "List all devices",
	Long:  "List all devices with their status and last seen time",
	Example: `  # List devices in table format
  datumctl device list

  # List devices in JSON format
  datumctl device list --json`,
	RunE: runDeviceList,
}

var deviceGetCmd = &cobra.Command{
	Use:     "get [device-id]",
	Short:   "Get device details",
	Args:    cobra.ExactArgs(1),
	Example: `  datumctl device get my-device-001`,
	RunE:    runDeviceGet,
}

var deviceCreateCmd = &cobra.Command{
	Use:   "create",
	Short: "Create a new device",
	Long: `Create a new device and generate an API key.

The API key will be displayed once and cannot be retrieved later.`,
	Example: `  # Create device with auto-generated ID
  datumctl device create --name "Temperature Sensor"

  # Create device with custom ID
  datumctl device create --id temp-sensor-01 --name "Temp Sensor" --type temperature`,
	RunE: runDeviceCreate,
}

var deviceDeleteCmd = &cobra.Command{
	Use:     "delete [device-id]",
	Short:   "Delete a device",
	Args:    cobra.ExactArgs(1),
	Example: `  datumctl device delete my-device-001`,
	RunE:    runDeviceDelete,
}

var (
	deviceName  string
	deviceID    string
	deviceType  string
	forceDelete bool
)

func init() {
	rootCmd.AddCommand(deviceCmd)

	deviceCmd.AddCommand(deviceListCmd)
	deviceCmd.AddCommand(deviceGetCmd)
	deviceCmd.AddCommand(deviceCreateCmd)
	deviceCmd.AddCommand(deviceDeleteCmd)

	deviceCreateCmd.Flags().StringVar(&deviceName, "name", "", "Device name (required)")
	deviceCreateCmd.Flags().StringVar(&deviceID, "id", "", "Device ID (auto-generated if not provided)")
	deviceCreateCmd.Flags().StringVar(&deviceType, "type", "sensor", "Device type")
	deviceCreateCmd.MarkFlagRequired("name")

	deviceDeleteCmd.Flags().BoolVarP(&forceDelete, "force", "f", false, "Skip confirmation")
}

func runDeviceList(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	resp, err := client.Get("/admin/devices")
	if err != nil {
		return err
	}

	var result map[string]interface{}
	if err := ParseResponse(resp, &result); err != nil {
		// Check if it's an empty response or wrong format
		if resp.StatusCode == 404 {
			if outputJSON {
				return printJSON(map[string]interface{}{"devices": []interface{}{}})
			}
			fmt.Println("\n📱 No devices found")
			fmt.Println("\nCreate your first device:")
			fmt.Println("  datumctl device create --name my-device")
			return nil
		}
		return err
	}

	// Extract devices array from response
	devicesInterface, ok := result["devices"]
	if !ok {
		if outputJSON {
			return printJSON(map[string]interface{}{"devices": []interface{}{}})
		}
		fmt.Println("\n📱 No devices found. Create your first device!")
		return nil
	}

	devices, ok := devicesInterface.([]interface{})
	if !ok || len(devices) == 0 {
		if outputJSON {
			return printJSON(map[string]interface{}{"devices": []interface{}{}})
		}
		fmt.Println("\n📱 No devices found. Create your first device!")
		return nil
	}

	if outputJSON {
		return printJSON(result)
	}

	// Print as table
	table := tablewriter.NewWriter(os.Stdout)
	table.Header("ID", "Name", "Type", "Status", "Last Seen")

	for _, deviceInterface := range devices {
		device, ok := deviceInterface.(map[string]interface{})
		if !ok {
			continue
		}
		id := getString(device, "id")
		name := getString(device, "name")
		dtype := getString(device, "type")
		status := getString(device, "status")
		lastSeen := getString(device, "last_seen")

		// Format last seen time
		if lastSeen == "0001-01-01T00:00:00Z" {
			lastSeen = "Never"
		} else {
			// Try to parse and format nicely
			if t, err := time.Parse(time.RFC3339, lastSeen); err == nil {
				lastSeen = t.Local().Format("2006-01-02 15:04:05")
			}
		}

		table.Append(id, name, dtype, status, lastSeen)
	}

	fmt.Printf("\n📱 Devices (%d)\n\n", len(devices))
	table.Render()
	return nil
}

func runDeviceGet(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	deviceID := args[0]
	resp, err := client.Get("/admin/devices/" + deviceID)
	if err != nil {
		return err
	}

	var device map[string]interface{}
	if err := ParseResponse(resp, &device); err != nil {
		return err
	}

	if outputJSON {
		return printJSON(device)
	}

	// Pretty print device info
	fmt.Printf("\n📱 Device: %s\n\n", getString(device, "name"))
	fmt.Printf("  ID:         %s\n", getString(device, "id"))
	fmt.Printf("  Name:       %s\n", getString(device, "name"))
	fmt.Printf("  Type:       %s\n", getString(device, "type"))
	fmt.Printf("  Status:     %s\n", getString(device, "status"))
	fmt.Printf("  Created:    %s\n", getString(device, "created_at"))
	fmt.Printf("  Last Seen:  %s\n", getString(device, "last_seen"))
	fmt.Printf("  Data Count: %v\n", device["data_count"])
	fmt.Println()

	return nil
}

func runDeviceCreate(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	createReq := map[string]string{
		"name": deviceName,
	}
	if deviceType != "" {
		createReq["type"] = deviceType
	}
	if deviceID != "" {
		createReq["device_id"] = deviceID
	}

	resp, err := client.Post("/admin/devices", createReq)
	if err != nil {
		return err
	}

	var result map[string]interface{}
	if err := ParseResponse(resp, &result); err != nil {
		return err
	}

	if outputJSON {
		return printJSON(result)
	}

	fmt.Printf("\n✅ Device created!\n\n")
	fmt.Printf("  ID:      %s\n", getString(result, "device_id"))
	fmt.Printf("  Name:    %s\n", getString(result, "name"))
	fmt.Printf("  Type:    %s\n", getString(result, "type"))

	if apiKeyVal, ok := result["api_key"].(string); ok && apiKeyVal != "" {
		fmt.Printf("\n  🔑 API Key: %s\n", apiKeyVal)
		fmt.Printf("  ⚠️  Save this key - it won't be shown again!\n\n")

		fmt.Println("  📝 Usage examples:")
		fmt.Printf("     # Send data from device:\n")
		fmt.Printf("     curl -X POST %s/public/data \\\n", serverURL)
		fmt.Printf("       -H 'Authorization: Bearer %s' \\\n", apiKeyVal)
		fmt.Printf("       -H 'Content-Type: application/json' \\\n")
		fmt.Printf("       -d '{\"temperature\": 25.5, \"humidity\": 60}'\n\n")

		fmt.Println("     # Save to datumctl config (optional):")
		fmt.Printf("     datumctl login --api-key %s\n", apiKeyVal)
	}
	fmt.Println()

	return nil
}

func runDeviceDelete(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	deviceID := args[0]

	if !forceDelete {
		fmt.Printf("⚠️  Delete device '%s'? (y/N): ", deviceID)
		var confirm string
		fmt.Scanln(&confirm)
		if confirm != "y" && confirm != "Y" {
			fmt.Println("Cancelled")
			return nil
		}
	}

	resp, err := client.Delete("/admin/devices/"+deviceID, nil)
	if err != nil {
		return err
	}

	if err := ParseResponse(resp, nil); err != nil {
		return err
	}

	fmt.Printf("✅ Device '%s' deleted\n", deviceID)
	return nil
}

// Helper functions
func getString(m map[string]interface{}, key string) string {
	if val, ok := m[key]; ok {
		if str, ok := val.(string); ok {
			return str
		}
		return fmt.Sprintf("%v", val)
	}
	return "-"
}

func printJSON(data interface{}) error {
	encoder := json.NewEncoder(os.Stdout)
	encoder.SetIndent("", "  ")
	return encoder.Encode(data)
}
