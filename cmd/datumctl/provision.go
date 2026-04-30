package main

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"

	"datum-go/internal/cli/utils"
)

var provisionCmd = &cobra.Command{
	Use:   "provision",
	Short: "Manage device provisioning (WiFi AP workflow)",
	Long: `Manage IoT device provisioning requests for WiFi AP mode setup.

This command group handles the provisioning workflow where devices create
a WiFi access point for initial configuration.`,
}

var (
	provisionUID      string
	provisionName     string
	provisionType     string
	provisionWiFiSSID string
	provisionWiFiPass string
)

var provisionRegisterCmd = &cobra.Command{
	Use:   "register",
	Short: "Register a device for provisioning",
	Long: `Register a new device for WiFi AP provisioning.

This creates a provisioning request that the device can activate.
Typically called by a mobile app after discovering a device via WiFi AP.`,
	Example: `  # Register a device
  datumctl provision register \
    --uid AABBCCDDEEFF \
    --name "Living Room Sensor" \
    --type temperature \
    --wifi-ssid "HomeWiFi" \
    --wifi-pass "password123"`,
	RunE: func(cmd *cobra.Command, args []string) error {
		loadConfig()
		if provisionUID == "" {
			return fmt.Errorf("device UID is required (--uid)")
		}
		if provisionName == "" {
			return fmt.Errorf("device name is required (--name)")
		}

		payload := map[string]interface{}{
			"device_uid":  provisionUID,
			"device_name": provisionName,
		}

		if provisionType != "" {
			payload["device_type"] = provisionType
		}
		if provisionWiFiSSID != "" {
			payload["wifi_ssid"] = provisionWiFiSSID
		}
		if provisionWiFiPass != "" {
			payload["wifi_pass"] = provisionWiFiPass
		}

		client := NewAPIClient(serverURL, token, apiKey)
		resp, err := client.Post("/dev/register", payload)
		if err != nil {
			return err
		}

		var result map[string]interface{}
		if err := ParseResponse(resp, &result); err != nil {
			return err
		}

		if outputJSON {
			return utils.PrintJSON(os.Stdout, result)
		}

		fmt.Println("✅ Device registered successfully")
		fmt.Printf("\nRequest ID: %v\n", result["request_id"])
		fmt.Printf("Device ID: %v\n", result["device_id"])
		fmt.Printf("API Key: %v\n", result["api_key"])
		fmt.Printf("Status: %v\n", result["status"])
		if expiry, ok := result["expires_at"]; ok {
			fmt.Printf("Expires: %v\n", expiry)
		}
		fmt.Printf("\nActivation URL: %v\n", result["activate_url"])

		return nil
	},
}

var provisionListCmd = &cobra.Command{
	Use:   "list",
	Short: "List provisioning requests",
	Long:  `List all provisioning requests for the current user.`,
	Example: `  # List all provisioning requests
  datumctl provision list

  # List in JSON format
  datumctl provision list --json`,
	RunE: func(cmd *cobra.Command, args []string) error {
		loadConfig()
		client := NewAPIClient(serverURL, token, apiKey)
		resp, err := client.Get("/dev/prov")
		if err != nil {
			return err
		}

		var result map[string]interface{}
		if err := ParseResponse(resp, &result); err != nil {
			return err
		}

		if outputJSON {
			return utils.PrintJSON(os.Stdout, result)
		}

		requests, ok := result["requests"].([]interface{})
		if !ok || len(requests) == 0 {
			fmt.Println("No provisioning requests found")
			return nil
		}

		fmt.Printf("\n📋 Provisioning Requests (%d)\n\n", len(requests))
		for _, req := range requests {
			r := req.(map[string]interface{})
			fmt.Printf("Request ID: %v\n", r["request_id"])
			fmt.Printf("  Device UID: %v\n", r["device_uid"])
			fmt.Printf("  Name: %v\n", r["device_name"])
			fmt.Printf("  Status: %v\n", r["status"])
			fmt.Printf("  Created: %v\n", r["created_at"])
			if expiry, ok := r["expires_at"]; ok {
				fmt.Printf("  Expires: %v\n", expiry)
			}
			fmt.Println()
		}

		return nil
	},
}

var provisionStatusCmd = &cobra.Command{
	Use:   "status <request_id>",
	Short: "Get provisioning request status",
	Long:  `Get the status of a specific provisioning request.`,
	Example: `  # Check request status
  datumctl provision status prov_abc123`,
	Args: cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		loadConfig()
		requestID := args[0]
		client := NewAPIClient(serverURL, token, apiKey)
		resp, err := client.Get("/dev/prov/" + requestID)
		if err != nil {
			return err
		}

		var result map[string]interface{}
		if err := ParseResponse(resp, &result); err != nil {
			return err
		}

		if outputJSON {
			return utils.PrintJSON(os.Stdout, result)
		}

		fmt.Println("\n📋 Provisioning Request Details")
		fmt.Printf("Request ID: %v\n", result["request_id"])
		fmt.Printf("Device UID: %v\n", result["device_uid"])
		fmt.Printf("Device Name: %v\n", result["device_name"])
		fmt.Printf("Device Type: %v\n", result["device_type"])
		fmt.Printf("Status: %v\n", result["status"])
		fmt.Printf("Created: %v\n", result["created_at"])
		if expiry, ok := result["expires_at"]; ok {
			fmt.Printf("Expires: %v\n", expiry)
		}
		if deviceID, ok := result["device_id"]; ok {
			fmt.Printf("Device ID: %v\n", deviceID)
		}
		fmt.Println()

		return nil
	},
}

var provisionCancelCmd = &cobra.Command{
	Use:   "cancel <request_id>",
	Short: "Cancel a provisioning request",
	Long:  `Cancel a pending provisioning request.`,
	Example: `  # Cancel a request
  datumctl provision cancel prov_abc123`,
	Args: cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		loadConfig()
		requestID := args[0]
		client := NewAPIClient(serverURL, token, apiKey)
		resp, err := client.Delete("/dev/prov/"+requestID, nil)
		if err != nil {
			return err
		}

		var result map[string]interface{}
		if err := ParseResponse(resp, &result); err != nil {
			return err
		}

		if outputJSON {
			return utils.PrintJSON(os.Stdout, result)
		}

		fmt.Printf("✅ Provisioning request %s cancelled successfully\n", requestID)

		return nil
	},
}

func init() {
	// Register flags
	provisionRegisterCmd.Flags().StringVar(&provisionUID, "uid", "", "Device unique ID (MAC address, chip ID)")
	provisionRegisterCmd.Flags().StringVar(&provisionName, "name", "", "Device name")
	provisionRegisterCmd.Flags().StringVar(&provisionType, "type", "", "Device type (optional)")
	provisionRegisterCmd.Flags().StringVar(&provisionWiFiSSID, "wifi-ssid", "", "WiFi network name (optional)")
	provisionRegisterCmd.Flags().StringVar(&provisionWiFiPass, "wifi-pass", "", "WiFi password (optional)")

	// Add subcommands
	provisionCmd.AddCommand(provisionRegisterCmd)
	provisionCmd.AddCommand(provisionListCmd)
	provisionCmd.AddCommand(provisionStatusCmd)
	provisionCmd.AddCommand(provisionCancelCmd)

	// Add to root command
	rootCmd.AddCommand(provisionCmd)
}
