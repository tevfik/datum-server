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
		case "Command & Control":
			if err := commandMenu(); err != nil {
				fmt.Printf("\n❌ Error: %v\n", err)
			}
		case "Provisioning":
			if err := provisionMenu(); err != nil {
				fmt.Printf("\n❌ Error: %v\n", err)
			}

		case "Admin Management":
			if err := adminMenu(); err != nil {
				fmt.Printf("\n❌ Error: %v\n", err)
			}
		case "Setup System":
			fmt.Println("\n> datumctl setup")
			if err := runSetup(nil, nil); err != nil {
				fmt.Printf("\n❌ Error: %v\n", err)
			}
		case "Login / Switch User":
			if err := promptLogin(); err != nil {
				fmt.Printf("\n❌ Error: %v\n", err)
			}
		case "Register User":
			fmt.Println("\n> datumctl register")
			if err := runRegister(nil, nil); err != nil {
				fmt.Printf("\n❌ Error: %v\n", err)
			}
		case "Logout":
			fmt.Println("\n> datumctl logout")
			if err := runLogout(nil, nil); err != nil {
				fmt.Printf("\n❌ Error: %v\n", err)
			}
			// Reset global variables
			token = ""
			apiKey = ""
		case "System Status":
			fmt.Println("\n> datumctl status")
			if err := runStatus(nil, nil); err != nil {
				fmt.Printf("\n❌ Error: %v\n", err)
			}
		case "Configuration":
			fmt.Println("\n> datumctl config show")
			runConfigShow(nil, nil)
		case "Show Version":
			fmt.Println("\nDatum IoT Platform CLI")
			fmt.Printf("Version: %s\n", Version)
			// fmt.Printf("Build: %s\n", BuildDate)
		case fmt.Sprintf("Toggle Curl Output (Current: %v)", showCurl), "Toggle Curl Output (Current: true)", "Toggle Curl Output (Current: false)":
			showCurl = !showCurl
			fmt.Printf("\n🔄 Curl output toggled: %v\n", showCurl)
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
			"Command & Control",
			"Provisioning",
			"Admin Management",
			"Setup System",
			"Login / Switch User",
			"Register User",
			"Logout",
			"System Status",
			"Configuration",
			"Show Version",
			fmt.Sprintf("Toggle Curl Output (Current: %v)", showCurl),
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
			"Token info",
			"Rotate key",
			"Revoke key (emergency)",
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

	case "Token info":
		var deviceID string
		if err := survey.AskOne(&survey.Input{
			Message: "Enter device ID:",
		}, &deviceID); err != nil {
			return err
		}
		fmt.Printf("\n> datumctl device token-info %s\n", deviceID)
		return runDeviceTokenInfo(nil, []string{deviceID})

	case "Rotate key":
		var deviceID string
		if err := survey.AskOne(&survey.Input{
			Message: "Enter device ID:",
		}, &deviceID); err != nil {
			return err
		}

		var graceDays int
		if err := survey.AskOne(&survey.Input{
			Message: "Grace period (days, default 7):",
			Default: "7",
		}, &graceDays); err != nil {
			graceDays = 7
		}

		var notify bool
		if err := survey.AskOne(&survey.Confirm{
			Message: "Notify device via command channel?",
			Default: true,
		}, &notify); err != nil {
			return err
		}

		gracePeriodDays = graceDays
		notifyDevice = notify
		fmt.Printf("\n> datumctl device rotate-key %s --grace-days %d\n", deviceID, graceDays)
		return runDeviceRotateKey(nil, []string{deviceID})

	case "Revoke key (emergency)":
		var deviceID string
		if err := survey.AskOne(&survey.Input{
			Message: "Enter device ID:",
		}, &deviceID); err != nil {
			return err
		}

		fmt.Println("\n⚠️  WARNING: This will immediately invalidate all keys for this device!")
		fmt.Println("The device will be unable to authenticate until re-provisioned.")

		var confirm bool
		if err := survey.AskOne(&survey.Confirm{
			Message: fmt.Sprintf("REVOKE all keys for '%s'?", deviceID),
			Default: false,
		}, &confirm); err != nil {
			return err
		}

		if confirm {
			forceDelete = true
			fmt.Printf("\n> datumctl device revoke-key %s --force\n", deviceID)
			return runDeviceRevokeKey(nil, []string{deviceID})
		}
		fmt.Println("Cancelled")

	case "─── Key Management ───":
		// Separator, do nothing
		return deviceMenu()
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

func commandMenu() error {
	var action string
	prompt := &survey.Select{
		Message: "Command & Control:",
		Options: []string{
			"Send command",
			"List commands",
			"Get command details",
			"← Back to main menu",
		},
	}

	if err := survey.AskOne(prompt, &action); err != nil {
		return err
	}

	switch action {
	case "Send command":
		return promptSendCommand()
	case "List commands":
		return promptListCommands()
	case "Get command details":
		return promptGetCommand()
	}
	return nil
}

func promptSendCommand() error {
	var device string
	if err := survey.AskOne(&survey.Input{
		Message: "Device ID:",
	}, &device, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	var cmdAction string
	if err := survey.AskOne(&survey.Input{
		Message: "Command Action (e.g., reboot, update-config):",
	}, &cmdAction, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	var addParams bool
	if err := survey.AskOne(&survey.Confirm{
		Message: "Add parameters?",
		Default: false,
	}, &addParams); err != nil {
		return err
	}

	var params []string
	if addParams {
		var paramStr string
		if err := survey.AskOne(&survey.Input{
			Message: "Parameters (key=value, comma separated):",
			Help:    "Example: interval=60,mode=auto",
		}, &paramStr); err != nil {
			return err
		}
		if paramStr != "" {
			params = strings.Split(paramStr, ",")
		}
	}

	commandParams = params
	fmt.Printf("\n> datumctl command send %s %s", device, cmdAction)
	for _, p := range params {
		fmt.Printf(" --param %s", p)
	}
	fmt.Println()

	return runCommandSend(nil, []string{device, cmdAction})
}

func promptListCommands() error {
	var device string
	if err := survey.AskOne(&survey.Input{
		Message: "Device ID:",
	}, &device, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	fmt.Printf("\n> datumctl command list %s\n", device)
	return runCommandList(nil, []string{device})
}

func promptGetCommand() error {
	var device string
	if err := survey.AskOne(&survey.Input{
		Message: "Device ID:",
	}, &device, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	var cmdID string
	if err := survey.AskOne(&survey.Input{
		Message: "Command ID:",
	}, &cmdID, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	fmt.Printf("\n> datumctl command get %s %s\n", device, cmdID)
	return runCommandGet(nil, []string{device, cmdID})
}

func provisionMenu() error {
	var action string
	prompt := &survey.Select{
		Message: "Provisioning:",
		Options: []string{
			"Register device (WiFi AP)",
			"List requests",
			"Check status",
			"Cancel request",
			"← Back to main menu",
		},
	}

	if err := survey.AskOne(prompt, &action); err != nil {
		return err
	}

	switch action {
	case "Register device (WiFi AP)":
		return promptProvisionRegister()
	case "List requests":
		fmt.Println("\n> datumctl provision list")
		return provisionListCmd.RunE(nil, nil)
	case "Check status":
		return promptProvisionStatus()
	case "Cancel request":
		return promptProvisionCancel()
	}
	return nil
}

func promptProvisionStatus() error {
	var reqID string
	if err := survey.AskOne(&survey.Input{
		Message: "Provisioning Request ID:",
	}, &reqID, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	fmt.Printf("\n> datumctl provision status %s\n", reqID)
	return provisionStatusCmd.RunE(nil, []string{reqID})
}

func promptProvisionCancel() error {
	var reqID string
	if err := survey.AskOne(&survey.Input{
		Message: "Provisioning Request ID:",
	}, &reqID, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	var confirm bool
	if err := survey.AskOne(&survey.Confirm{
		Message: fmt.Sprintf("Cancel request '%s'?", reqID),
	}, &confirm); err != nil {
		return err
	}

	if confirm {
		fmt.Printf("\n> datumctl provision cancel %s\n", reqID)
		return provisionCancelCmd.RunE(nil, []string{reqID})
	}
	fmt.Println("Cancelled")
	return nil
}

func promptProvisionRegister() error {
	var uid string
	if err := survey.AskOne(&survey.Input{
		Message: "Device UID (MAC address):",
	}, &uid, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	var name string
	if err := survey.AskOne(&survey.Input{
		Message: "Device Name:",
	}, &name, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	var devType string
	survey.AskOne(&survey.Input{
		Message: "Device Type:",
		Default: "sensor",
	}, &devType)

	var ssid string
	survey.AskOne(&survey.Input{
		Message: "WiFi SSID:",
	}, &ssid)

	var pass string
	if ssid != "" {
		survey.AskOne(&survey.Password{
			Message: "WiFi Password:",
		}, &pass)
	}

	provisionUID = uid
	provisionName = name
	provisionType = devType
	provisionWiFiSSID = ssid
	provisionWiFiPass = pass

	cmdStr := fmt.Sprintf("datumctl provision register --uid %q --name %q --type %q", uid, name, devType)
	if ssid != "" {
		cmdStr += fmt.Sprintf(" --wifi-ssid %q", ssid)
	}
	if pass != "" {
		cmdStr += fmt.Sprintf(" --wifi-pass %q", pass)
	}
	fmt.Printf("\n> %s\n", cmdStr)

	return provisionRegisterCmd.RunE(nil, nil)
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

func adminMenu() error {
	var action string
	prompt := &survey.Select{
		Message: "Admin Management:",
		Options: []string{
			"List users",
			"Create user",
			"Update user role/status",
			"Delete user",
			"Reset user password",
			"System statistics",
			"System config",
			"Toggle Registration",
			"Reset system (Dangerous)",
			"← Back to main menu",
		},
	}

	if err := survey.AskOne(prompt, &action); err != nil {
		return err
	}

	// Reset admin flags to avoid carrying over values
	adminEmail = ""
	adminPassword = ""
	adminRole = ""
	adminNewPassword = ""
	adminForceDelete = false
	adminForceReset = false

	switch action {
	case "List users":
		fmt.Println("\n> datumctl admin list-users")
		return runListUsers(nil, nil)

	case "Create user":
		return runCreateUser(nil, nil)

	case "Update user role/status":
		return promptAdminUpdateUser()

	case "Delete user":
		return promptAdminDeleteUser()

	case "Reset user password":
		return promptAdminResetPassword()

	case "System statistics":
		fmt.Println("\n> datumctl admin stats")
		return runAdminStats(nil, nil)

	case "System config":
		fmt.Println("\n> datumctl admin get-config")
		return runAdminConfig(nil, nil)

	case "Toggle Registration":
		return promptToggleRegistration()

	case "Reset system (Dangerous)":
		return runResetSystem(nil, nil)
	}

	return nil
}

func promptAdminUpdateUser() error {
	var identifier string
	if err := survey.AskOne(&survey.Input{
		Message: "User Email or ID:",
	}, &identifier, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	var action string
	if err := survey.AskOne(&survey.Select{
		Message: "What to update?",
		Options: []string{"Role (Admin/User)", "Status (Active/Suspended)", "Both"},
	}, &action); err != nil {
		return err
	}

	var role string
	if action == "Role (Admin/User)" || action == "Both" {
		if err := survey.AskOne(&survey.Select{
			Message: "New Role:",
			Options: []string{"user", "admin"},
		}, &role); err != nil {
			return err
		}
		adminRole = role
	}

	var status string
	if action == "Status (Active/Suspended)" || action == "Both" {
		if err := survey.AskOne(&survey.Select{
			Message: "New Status:",
			Options: []string{"active", "suspended"},
		}, &status); err != nil {
			return err
		}
		adminStatus = status
	}

	cmdStr := fmt.Sprintf("datumctl admin update-user %s", identifier)
	if role != "" {
		cmdStr += fmt.Sprintf(" --role %s", role)
	}
	if status != "" {
		cmdStr += fmt.Sprintf(" --status %s", status)
	}
	fmt.Printf("\n> %s\n", cmdStr)

	return runUpdateUser(nil, []string{identifier})
}

func promptAdminDeleteUser() error {
	var email string
	if err := survey.AskOne(&survey.Input{
		Message: "User Email to delete:",
	}, &email, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	fmt.Printf("\n> datumctl admin delete-user %s\n", email)
	return runDeleteUser(nil, []string{email})
}

func promptAdminResetPassword() error {
	var email string
	if err := survey.AskOne(&survey.Input{
		Message: "User Email/Username:",
	}, &email, survey.WithValidator(survey.Required)); err != nil {
		return err
	}

	return runResetPassword(nil, []string{email})
}

func promptToggleRegistration() error {
	var enable bool
	if err := survey.AskOne(&survey.Confirm{
		Message: "Enable public user registration?",
		Default: false,
	}, &enable); err != nil {
		return err
	}

	arg := "false"
	if enable {
		arg = "true"
	}
	fmt.Printf("\n> datumctl admin toggle-registration %s\n", arg)
	return runToggleRegistration(nil, []string{arg})
}
