package main

import (
	"fmt"

	"github.com/AlecAivazis/survey/v2"
	"github.com/spf13/cobra"
	"golang.org/x/term"
)

var (
	setupPlatformName  string
	setupAdminEmail    string
	setupAdminPassword string
	setupAllowRegister bool
	setupDataRetention int
	setupYes           bool
)

var setupCmd = &cobra.Command{
	Use:   "setup",
	Short: "Initialize the system with admin user",
	Long: `Setup the Datum server for first time use.

This creates the admin user and initializes the system.
Only works if the system is not already initialized.`,
	Example: `  # Interactive setup
  datumctl setup

  # Quick setup with flags
  datumctl setup --email admin@example.com --platform "My IoT Platform"`,
	RunE: runSetup,
}

func init() {
	rootCmd.AddCommand(setupCmd)

	setupCmd.Flags().StringVar(&setupPlatformName, "platform", "", "Platform name")
	setupCmd.Flags().StringVar(&setupAdminEmail, "email", "", "Admin email")
	setupCmd.Flags().StringVar(&setupAdminPassword, "password", "", "Admin password (will prompt if not provided)")
	setupCmd.Flags().BoolVar(&setupAllowRegister, "allow-register", false, "Allow user registration")
	setupCmd.Flags().IntVar(&setupDataRetention, "retention", 30, "Data retention in days")
	setupCmd.Flags().BoolVarP(&setupYes, "yes", "y", false, "Skip confirmation prompt")
}

func runSetup(cmd *cobra.Command, args []string) error {
	loadConfig()

	fmt.Println("\n🎯 Datum Server - Initial Setup")
	fmt.Println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")

	// Check if already initialized
	client := NewAPIClient(serverURL, "", "")
	resp, err := client.Get("/system/status")
	if err != nil {
		fmt.Printf("⚠️  Cannot connect to server at %s\n", serverURL)

		var newURL string
		if err := survey.AskOne(&survey.Input{
			Message: "Enter Datum Server URL:",
			Default: serverURL,
		}, &newURL); err != nil {
			return err
		}

		serverURL = newURL
		client = NewAPIClient(serverURL, "", "")
		resp, err = client.Get("/system/status")
		if err != nil {
			return fmt.Errorf("still cannot connect to server: %w", err)
		}
	}

	var status map[string]interface{}
	if err := ParseResponse(resp, &status); err != nil {
		return err
	}

	if initialized, ok := status["initialized"].(bool); ok && initialized {
		return fmt.Errorf("system is already initialized\nUse 'datumctl admin reset-system' to reset and reinitialize")
	}

	// Collect information
	if setupPlatformName == "" {
		if err := survey.AskOne(&survey.Input{
			Message: "Platform name:",
			Default: "Datum IoT Platform",
		}, &setupPlatformName, survey.WithValidator(survey.Required)); err != nil {
			return err
		}
	}

	if setupAdminEmail == "" {
		if err := survey.AskOne(&survey.Input{
			Message: "Admin email:",
		}, &setupAdminEmail, survey.WithValidator(survey.Required)); err != nil {
			return err
		}
	}

	if setupAdminPassword == "" {
		fmt.Print("Admin password (min 8 chars): ")
		passwordBytes, err := term.ReadPassword(0)
		fmt.Println()
		if err != nil {
			return err
		}
		setupAdminPassword = string(passwordBytes)

		if len(setupAdminPassword) < 8 {
			return fmt.Errorf("password must be at least 8 characters")
		}
	}

	// Confirm
	fmt.Println("\n📋 Setup Summary:")
	fmt.Printf("  Platform: %s\n", setupPlatformName)
	fmt.Printf("  Admin Email: %s\n", setupAdminEmail)
	fmt.Printf("  Allow Registration: %v\n", setupAllowRegister)
	fmt.Printf("  Data Retention: %d days\n", setupDataRetention)
	fmt.Println()

	confirm := setupYes
	if !setupYes {
		if err := survey.AskOne(&survey.Confirm{
			Message: "Proceed with setup?",
			Default: true,
		}, &confirm); err != nil {
			return err
		}
	}

	if !confirm {
		fmt.Println("Setup cancelled")
		return nil
	}

	// Send setup request
	payload := map[string]interface{}{
		"platform_name":  setupPlatformName,
		"admin_email":    setupAdminEmail,
		"admin_password": setupAdminPassword,
		"allow_register": setupAllowRegister,
		"data_retention": setupDataRetention,
	}

	resp, err = client.Post("/system/setup", payload)
	if err != nil {
		return fmt.Errorf("setup failed: %w", err)
	}

	var response map[string]interface{}
	if err := ParseResponse(resp, &response); err != nil {
		return err
	}

	if outputJSON {
		return printJSON(response)
	}

	// Show success
	fmt.Println("\n✅ System initialized successfully!")
	fmt.Println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
	fmt.Printf("\n👤 Admin User Created\n")
	fmt.Printf("  Email: %s\n", setupAdminEmail)
	fmt.Printf("  Role: admin\n")

	if tokenStr, ok := response["token"].(string); ok {
		fmt.Printf("\n🔑 Your access token:\n%s\n", tokenStr)

		// Auto-save token
		saveToken(tokenStr)
		fmt.Println("\n✅ Token saved to ~/.datumctl.yaml")
		fmt.Println("You are now logged in!")
	}

	fmt.Println("\n📝 Next steps:")
	fmt.Println("  1. Create devices: datumctl device create --name \"My Device\"")
	fmt.Println("  2. Use interactive mode: datumctl interactive")
	fmt.Println("  3. View documentation: datumctl --help")
	fmt.Println()

	return nil
}
