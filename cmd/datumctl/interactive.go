package main

import (
	"fmt"
	"strings"

	"github.com/AlecAivazis/survey/v2"
	"github.com/spf13/cobra"
)

var interactiveCmd = &cobra.Command{
	Use:     "interactive",
	Aliases: []string{"i", "menu"},
	Short:   "Interactive menu mode",
	Long: `Launch interactive menu to explore and execute commands.

No need to remember command names or flags - just select from menus!`,
	RunE: runInteractive,
}

func init() {
	rootCmd.AddCommand(interactiveCmd)
}

func runInteractive(cmd *cobra.Command, args []string) error {
	loadConfig()

	// Welcome message
	fmt.Println("\n🎯 Datum IoT Platform - Interactive Mode")
	fmt.Println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")

	// Check authentication
	if token == "" && apiKey == "" {
		fmt.Println("\n⚠️  Not authenticated. Please login first.")
		if err := promptLogin(); err != nil {
			return err
		}
	} else {
		fmt.Printf("\n✅ Connected to: %s\n", serverURL)
	}

	// Main menu loop
	for {
		action, err := showMainMenu()
		if err != nil {
			return err
		}

		switch action {
		case "Device Management":
			if err := deviceMenu(); err != nil {
				fmt.Printf("\n❌ Error: %v\n", err)
			}
		case "Data Queries":
			if err := dataMenu(); err != nil {
				fmt.Printf("\n❌ Error: %v\n", err)
			}
		case "System Status":
			fmt.Println("\n> datumctl status")
			if err := runStatus(nil, nil); err != nil {
				fmt.Printf("\n❌ Error: %v\n", err)
			}
		case "Configuration":
			fmt.Println("\n> datumctl config show")
			runConfigShow(nil, nil)
		case "Exit":
			fmt.Println("\n👋 Goodbye!")
			return nil
		}

		fmt.Println()
	}
}

func showMainMenu() (string, error) {
	var action string
	prompt := &survey.Select{
		Message: "\nWhat would you like to do?",
		Options: []string{
			"Device Management",
			"Data Queries",
			"System Status",
			"Configuration",
			"Exit",
		},
		PageSize: 10,
	}

	if err := survey.AskOne(prompt, &action); err != nil {
		return "", err
	}

	return action, nil
}

func deviceMenu() error {
	var action string
	prompt := &survey.Select{
		Message: "Device Management:",
		Options: []string{
			"List all devices",
			"Get device details",
			"Create new device",
			"Delete device",
			"← Back to main menu",
		},
	}

	if err := survey.AskOne(prompt, &action); err != nil {
		return err
	}

	switch action {
	case "List all devices":
		fmt.Println("\n> datumctl device list")
		if err := runDeviceList(nil, nil); err != nil {
			if strings.Contains(err.Error(), "404") || strings.Contains(err.Error(), "cannot unmarshal object") {
				fmt.Println("\n📱 No devices found. Create your first device!")
				return nil
			}
			return err
		}
		return nil

	case "Get device details":
		var deviceID string
		if err := survey.AskOne(&survey.Input{
			Message: "Enter device ID:",
		}, &deviceID); err != nil {
			return err
		}
		fmt.Printf("\n> datumctl device get %s\n", deviceID)
		return runDeviceGet(nil, []string{deviceID})

	case "Create new device":
		return promptCreateDevice()

	case "Delete device":
		var deviceID string
		if err := survey.AskOne(&survey.Input{
			Message: "Enter device ID to delete:",
		}, &deviceID); err != nil {
			return err
		}

		var confirm bool
		if err := survey.AskOne(&survey.Confirm{
			Message: fmt.Sprintf("Delete device '%s'?", deviceID),
		}, &confirm); err != nil {
			return err
		}

		if confirm {
			forceDelete = true
			fmt.Printf("\n> datumctl device delete %s --force\n", deviceID)
			return runDeviceDelete(nil, []string{deviceID})
		}
		fmt.Println("Cancelled")
	}

	return nil
}

func promptCreateDevice() error {
	var name string
	if err := survey.AskOne(&survey.Input{
		Message: "Device name:",
	}, &name, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	var id string
	survey.AskOne(&survey.Input{
		Message: "Device ID (leave empty for auto-generate):",
	}, &id)

	var deviceTypeInput string
	survey.AskOne(&survey.Input{
		Message: "Device type (e.g., sensor, temperature, humidity, pressure, motion):",
		Default: "sensor",
	}, &deviceTypeInput)

	deviceName = name
	cmdStr := fmt.Sprintf("datumctl device create --name %q --type %q", name, deviceTypeInput)
	if id != "" {
		cmdStr += fmt.Sprintf(" --id %q", id)
	}
	fmt.Printf("\n> %s\n", cmdStr)

	deviceID = id
	deviceType = deviceTypeInput

	return runDeviceCreate(nil, nil)
}

func dataMenu() error {
	var action string
	prompt := &survey.Select{
		Message: "Data Queries:",
		Options: []string{
			"Get device data",
			"Post data",
			"Get statistics",
			"← Back to main menu",
		},
	}

	if err := survey.AskOne(prompt, &action); err != nil {
		return err
	}

	switch action {
	case "Get device data":
		return promptGetData()

	case "Post data":
		return promptPostData()

	case "Get statistics":
		return promptDataStats()
	}

	return nil
}

func promptGetData() error {
	var device string
	if err := survey.AskOne(&survey.Input{
		Message: "Device ID:",
		Help:    "Enter a valid device ID. Use 'List all devices' first to see available devices.",
	}, &device, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	if device == "" {
		fmt.Println("\n⚠️  No device ID provided. Returning to menu.")
		return nil
	}

	var timeRange string
	if err := survey.AskOne(&survey.Select{
		Message: "Time range:",
		Options: []string{"Last 1 hour", "Last 6 hours", "Last 24 hours", "Last 7 days", "Custom"},
		Default: "Last 1 hour",
	}, &timeRange); err != nil {
		return err
	}

	var lastTime string
	switch timeRange {
	case "Last 1 hour":
		lastTime = "1h"
	case "Last 6 hours":
		lastTime = "6h"
	case "Last 24 hours":
		lastTime = "24h"
	case "Last 7 days":
		lastTime = "168h"
	case "Custom":
		survey.AskOne(&survey.Input{
			Message: "Duration (e.g., 30m, 2h, 3d):",
		}, &lastTime)
	}

	var limitInput string
	survey.AskOne(&survey.Input{
		Message: "Limit (max results):",
		Default: "100",
	}, &limitInput)

	dataDevice = device
	dataLast = lastTime
	fmt.Sscanf(limitInput, "%d", &dataLimit)

	fmt.Printf("\n> datumctl data get --device %s --last %s --limit %d\n", device, lastTime, dataLimit)

	return runDataGet(nil, nil)
}

func promptPostData() error {
	var device string
	survey.AskOne(&survey.Input{
		Message: "Device ID (or leave empty to use API key):",
	}, &device)

	var jsonData string
	if err := survey.AskOne(&survey.Input{
		Message: "JSON data:",
		Help:    `Example: {"temperature": 25.5, "humidity": 60}`,
	}, &jsonData, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	dataDevice = device
	dataJSON = jsonData

	cmdStr := fmt.Sprintf("datumctl data post --data %q", jsonData)
	if device != "" {
		cmdStr += fmt.Sprintf(" --device %s", device)
	}
	fmt.Printf("\n> %s\n", cmdStr)

	return runDataPost(nil, nil)
}

func promptDataStats() error {
	var device string
	if err := survey.AskOne(&survey.Input{
		Message: "Device ID:",
		Help:    "Enter a valid device ID. Use 'List all devices' first to see available devices.",
	}, &device, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	if device == "" {
		fmt.Println("\n⚠️  No device ID provided. Returning to menu.")
		return nil
	}

	var timeRange string
	if err := survey.AskOne(&survey.Select{
		Message: "Time range for statistics:",
		Options: []string{"1h", "6h", "24h", "7d", "30d"},
		Default: "24h",
	}, &timeRange); err != nil {
		fmt.Printf("\n> datumctl data stats --device %s --last %s\n", device, timeRange)

		return err
	}

	dataDevice = device
	dataLast = timeRange

	return runDataStats(nil, nil)
}

func promptLogin() error {
	var email string
	if err := survey.AskOne(&survey.Input{
		Message: "Email:",
	}, &email, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	var password string
	if err := survey.AskOne(&survey.Password{
		Message: "Password:",
	}, &password, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	loginEmail = email
	loginPassword = password

	return runLogin(nil, nil)
}
