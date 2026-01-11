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

var deviceRotateKeyCmd = &cobra.Command{
	Use:   "rotate-key [device-id]",
	Short: "Rotate device API key",
	Long: `Rotate the API key for a device.

The old key remains valid during the grace period (default: 7 days),
allowing the device to transition to the new key without downtime.

The new key can be delivered to the device via:
- Command channel (SSE or polling)
- Manual update (for offline devices)`,
	Args: cobra.ExactArgs(1),
	Example: `  # Rotate key with default 7-day grace period
  datumctl device rotate-key my-device-001

  # Rotate key with custom grace period
  datumctl device rotate-key my-device-001 --grace-days 14

  # Rotate key and notify device via command channel
  datumctl device rotate-key my-device-001 --notify`,
	RunE: runDeviceRotateKey,
}

var deviceRevokeKeyCmd = &cobra.Command{
	Use:   "revoke-key [device-id]",
	Short: "Revoke device API key (emergency)",
	Long: `Immediately revoke all API keys for a device.

⚠️  WARNING: This is an emergency action!
The device will be unable to authenticate until re-provisioned.

Use this when:
- A device key has been compromised
- A device has been stolen
- Immediate access termination is required`,
	Args: cobra.ExactArgs(1),
	Example: `  # Revoke device key immediately
  datumctl device revoke-key my-device-001

  # Revoke without confirmation (use with caution)
  datumctl device revoke-key my-device-001 --force`,
	RunE: runDeviceRevokeKey,
}

var deviceTokenInfoCmd = &cobra.Command{
	Use:     "token-info [device-id]",
	Short:   "Show device token information",
	Long:    `Display token status, expiration, and rotation information for a device.`,
	Args:    cobra.ExactArgs(1),
	Example: `  datumctl device token-info my-device-001`,
	RunE:    runDeviceTokenInfo,
}

var (
	deviceName      string
	deviceID        string
	deviceType      string
	forceDelete     bool
	gracePeriodDays int
	notifyDevice    bool
)

func init() {
	rootCmd.AddCommand(deviceCmd)

	deviceCmd.AddCommand(deviceListCmd)
	deviceCmd.AddCommand(deviceGetCmd)
	deviceCmd.AddCommand(deviceCreateCmd)
	deviceCmd.AddCommand(deviceDeleteCmd)
	deviceCmd.AddCommand(deviceRotateKeyCmd)
	deviceCmd.AddCommand(deviceRevokeKeyCmd)
	deviceCmd.AddCommand(deviceTokenInfoCmd)

	deviceCreateCmd.Flags().StringVar(&deviceName, "name", "", "Device name (required)")
	deviceCreateCmd.Flags().StringVar(&deviceID, "id", "", "Device ID (auto-generated if not provided)")
	deviceCreateCmd.Flags().StringVar(&deviceType, "type", "sensor", "Device type")
	deviceCreateCmd.MarkFlagRequired("name")

	deviceDeleteCmd.Flags().BoolVarP(&forceDelete, "force", "f", false, "Skip confirmation")

	deviceRotateKeyCmd.Flags().IntVar(&gracePeriodDays, "grace-days", 7, "Grace period in days (both old and new keys valid)")
	deviceRotateKeyCmd.Flags().BoolVar(&notifyDevice, "notify", false, "Notify device via command channel")

	deviceRevokeKeyCmd.Flags().BoolVarP(&forceDelete, "force", "f", false, "Skip confirmation")
}

func runDeviceList(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	resp, err := client.Get("/admin/dev")
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
	resp, err := client.Get("/admin/dev/" + deviceID)
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

	resp, err := client.Post("/admin/dev", createReq)
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
		fmt.Printf("     curl -X POST %s/pub/%s \\\n", serverURL, getString(result, "device_id"))
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

	resp, err := client.Delete("/admin/dev/"+deviceID, nil)
	if err != nil {
		return err
	}

	if err := ParseResponse(resp, nil); err != nil {
		return err
	}

	fmt.Printf("✅ Device '%s' deleted\n", deviceID)
	return nil
}

func runDeviceRotateKey(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	deviceID := args[0]

	payload := map[string]interface{}{
		"grace_period_days": gracePeriodDays,
		"notify_device":     notifyDevice,
	}

	resp, err := client.Post("/admin/dev/"+deviceID+"/rotate-key", payload)
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

	fmt.Println("\n🔑 API Key Rotated Successfully")
	fmt.Printf("\nDevice ID: %s\n", deviceID)

	if newKey, ok := result["new_token"].(string); ok {
		fmt.Printf("New Token: %s\n", newKey)
	}
	if expiresAt, ok := result["token_expires_at"].(string); ok {
		fmt.Printf("Token Expires: %s\n", expiresAt)
	}
	if gracePeriodEnd, ok := result["grace_period_end"].(string); ok {
		fmt.Printf("Grace Period Until: %s\n", gracePeriodEnd)
		fmt.Printf("\n⚠️  Old key remains valid until grace period ends\n")
	}
	if notifyDevice {
		if notified, ok := result["device_notified"].(bool); ok && notified {
			fmt.Printf("✅ Device notified via command channel\n")
		} else {
			fmt.Printf("⚠️  Device not online - command queued for delivery\n")
		}
	}
	fmt.Println()

	return nil
}

func runDeviceRevokeKey(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	deviceID := args[0]

	if !forceDelete {
		fmt.Printf("⚠️  REVOKE all keys for device '%s'? This is PERMANENT! (y/N): ", deviceID)
		var confirm string
		fmt.Scanln(&confirm)
		if confirm != "y" && confirm != "Y" {
			fmt.Println("Cancelled")
			return nil
		}
	}

	payload := map[string]interface{}{
		"immediate": true,
	}

	resp, err := client.Post("/admin/dev/"+deviceID+"/revoke-key", payload)
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

	fmt.Println("\n🚨 Device Keys Revoked")
	fmt.Printf("\nDevice ID: %s\n", deviceID)
	fmt.Printf("Status: All keys invalidated\n")
	fmt.Printf("\n⚠️  Device will be unable to authenticate until re-provisioned\n")
	fmt.Println()

	return nil
}

func runDeviceTokenInfo(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	deviceID := args[0]

	resp, err := client.Get("/admin/dev/" + deviceID + "/token-info")
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

	fmt.Println("\n🔐 Device Token Information")
	fmt.Printf("\nDevice ID: %s\n", deviceID)

	if hasToken, ok := result["has_token"].(bool); ok && hasToken {
		fmt.Printf("Token Status: Active\n")
		if expiresAt, ok := result["token_expires_at"].(string); ok {
			fmt.Printf("Token Expires: %s\n", expiresAt)
		}
		if needsRefresh, ok := result["needs_refresh"].(bool); ok && needsRefresh {
			fmt.Printf("⚠️  Token approaching expiry - refresh recommended\n")
		}
		if inGracePeriod, ok := result["in_grace_period"].(bool); ok && inGracePeriod {
			fmt.Printf("🔄 Rotation in progress (grace period active)\n")
			if gracePeriodEnd, ok := result["grace_period_end"].(string); ok {
				fmt.Printf("Grace Period Ends: %s\n", gracePeriodEnd)
			}
		}
	} else {
		fmt.Printf("Token Status: Using legacy API key\n")
		fmt.Printf("💡 Consider migrating to token-based auth for better security\n")
	}

	if lastRotated, ok := result["last_rotated_at"].(string); ok && lastRotated != "" {
		fmt.Printf("Last Rotated: %s\n", lastRotated)
	}

	fmt.Println()
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
