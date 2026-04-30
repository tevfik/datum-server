package main

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"

	"datum-go/internal/cli/utils"
)

var updateFirmwareDeviceCmd = &cobra.Command{
	Use:   "update-firmware <device_id> <firmware_url>",
	Short: "Trigger OTA firmware update",
	Long:  `Send a command to the device to download and install new firmware from the given URL.`,
	Args:  cobra.ExactArgs(2),
	Example: `  # Update device firmware from a URL
  datumctl device update-firmware device-123 http://192.168.1.100:8000/firmware.bin`,
	RunE: runUpdateFirmwareDevice,
}

func init() {
	deviceCmd.AddCommand(updateFirmwareDeviceCmd)
}

func runUpdateFirmwareDevice(cmd *cobra.Command, args []string) error {
	loadConfig()

	deviceID := args[0]
	firmwareURL := args[1]

	// Build request payload
	requestBody := map[string]interface{}{
		"action": "update_firmware",
		"params": map[string]string{
			"url": firmwareURL,
		},
	}

	client := NewAPIClient(serverURL, token, apiKey)
	resp, err := client.Post(fmt.Sprintf("/dev/%s/cmd", deviceID), requestBody)
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

	fmt.Printf("✓ OTA Update Command Sent\n\n")
	fmt.Printf("📤 Command ID: %s\n", result["command_id"])
	fmt.Printf("📱 Device:     %s\n", deviceID)
	fmt.Printf("📦 URL:        %s\n", firmwareURL)
	fmt.Printf("📊 Status:     %s\n", result["status"])

	fmt.Println("\nThe device will process this command on its next poll interval.")

	// Wait logic could be added here similar to command send, but OTA takes time and device reboots,
	// so just sending it is usually enough. user can check status later.

	return nil
}
