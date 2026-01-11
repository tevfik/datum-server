package main

import (
	"fmt"

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
	client := NewAPIClient(serverURL, token, apiKey)

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

	resp, err := client.Put("/admin/dev/"+deviceID, updates)
	if err != nil {
		return err
	}

	var result map[string]interface{}
	// The server currently returns {"message": ...} for updates, not the object.
	// But let's check what it returns. admin.go says: c.JSON(http.StatusOK, gin.H{"message": "Device updated"})
	// So we won't get the updated object back.
	if err := ParseResponse(resp, &result); err != nil {
		return err
	}

	if outputJSON {
		return printJSON(result)
	}

	fmt.Printf("\n✅ Device %s updated successfully\n", deviceID)
	return nil
}
