package main

import (
	"fmt"
	"os"

	"github.com/AlecAivazis/survey/v2"
	"github.com/olekukonko/tablewriter"
	"github.com/spf13/cobra"
	"golang.org/x/term"
)

var (
	adminUsername    string
	adminNewPassword string
	adminForceReset  bool
	adminEmail       string
	adminPassword    string
	adminRole        string
	adminForceDelete bool
)

var adminCmd = &cobra.Command{
	Use:   "admin",
	Short: "Admin operations (user management, system reset)",
	Long: `Administrative commands for managing users and system.

Requires admin authentication.`,
}

var resetPasswordCmd = &cobra.Command{
	Use:   "reset-password [username]",
	Short: "Reset a user's password",
	Long: `Reset a user's password. Admin authentication required.

If no new password is provided, a random password will be generated.`,
	Args: cobra.ExactArgs(1),
	RunE: runResetPassword,
}

var createUserCmd = &cobra.Command{
	Use:   "create-user",
	Short: "Create a new user",
	Long:  `Create a new user account. Admin authentication required.`,
	RunE:  runCreateUser,
}

var deleteUserCmd = &cobra.Command{
	Use:   "delete-user [email]",
	Short: "Delete a user",
	Long:  `Delete a user account. Admin authentication required.`,
	Args:  cobra.ExactArgs(1),
	RunE:  runDeleteUser,
}

var listUsersCmd = &cobra.Command{
	Use:   "list-users",
	Short: "List all users",
	Long:  `List all registered users. Admin authentication required.`,
	RunE:  runListUsers,
}

var resetSystemCmd = &cobra.Command{
	Use:   "reset-system",
	Short: "Reset the entire system (DANGEROUS!)",
	Long: `Reset the entire system database and return to uninitialized state.

⚠️  WARNING: This will delete ALL data including:
  - All users (including admin)
  - All devices
  - All stored data
  - System configuration

After reset, you'll need to run setup again.`,
	RunE: runResetSystem,
}

var statsCmd = &cobra.Command{
	Use:   "stats",
	Short: "Get system statistics",
	Long:  `Retrieve comprehensive system statistics including database, users, devices, and data points.`,
	Example: `  # Get system stats
  datumctl admin stats

  # Get stats in JSON format
  datumctl admin stats --json`,
	RunE: runAdminStats,
}

var adminConfigCmd = &cobra.Command{
	Use:   "get-config",
	Short: "Get system configuration",
	Long:  `Retrieve current system configuration including retention settings and platform info.`,
	Example: `  # Get system config
  datumctl admin get-config

  # Get config in JSON format
  datumctl admin get-config --json`,
	RunE: runAdminConfig,
}

func init() {
	rootCmd.AddCommand(adminCmd)

	// Reset password command
	adminCmd.AddCommand(resetPasswordCmd)
	resetPasswordCmd.Flags().StringVar(&adminNewPassword, "new-password", "", "New password (if not provided, generates random)")

	// Create user command
	adminCmd.AddCommand(createUserCmd)
	createUserCmd.Flags().StringVar(&adminEmail, "email", "", "User email")
	createUserCmd.Flags().StringVar(&adminPassword, "password", "", "User password (min 8 chars)")
	createUserCmd.Flags().StringVar(&adminRole, "role", "user", "User role (user or admin)")

	// List users command
	adminCmd.AddCommand(listUsersCmd)

	// Delete user command
	adminCmd.AddCommand(deleteUserCmd)
	deleteUserCmd.Flags().BoolVar(&adminForceDelete, "force", false, "Skip confirmation")

	// Reset system command
	adminCmd.AddCommand(resetSystemCmd)
	resetSystemCmd.Flags().BoolVar(&adminForceReset, "force", false, "Skip confirmation")

	// Stats command
	adminCmd.AddCommand(statsCmd)

	// Config command
	adminCmd.AddCommand(adminConfigCmd)
}

func runResetPassword(cmd *cobra.Command, args []string) error {
	loadConfig()

	username := args[0]

	// Check admin authentication
	if token == "" && apiKey == "" {
		return fmt.Errorf("admin authentication required. Please login first")
	}

	// Prompt for new password if not provided
	if adminNewPassword == "" {
		fmt.Print("Enter new password (leave empty for random): ")
		passwordBytes, _ := term.ReadPassword(0)
		fmt.Println()
		adminNewPassword = string(passwordBytes)
	}

	// Prepare request
	payload := map[string]interface{}{}
	if adminNewPassword != "" {
		payload["new_password"] = adminNewPassword
	}

	// Make API call
	client := NewAPIClient(serverURL, token, apiKey)

	resp, err := client.Post("/admin/users/"+username+"/reset-password", payload)
	if err != nil {
		return fmt.Errorf("failed to reset password: %w", err)
	}

	var response map[string]interface{}
	if err := ParseResponse(resp, &response); err != nil {
		return err
	}

	if outputJSON {
		return printJSON(response)
	}

	fmt.Printf("✅ Password reset successfully\n")
	if newPass, ok := response["new_password"].(string); ok {
		fmt.Printf("New password: %s\n", newPass)
		fmt.Println("\n⚠️  Save this password - it won't be shown again!")
	}

	// If token is missing (legacy server), try to login to get it
	if _, hasToken := response["token"]; !hasToken {
		pass := adminNewPassword
		if newPass, ok := response["new_password"].(string); ok {
			pass = newPass
		}

		if pass != "" {
			// Try to login to get token
			loginClient := NewAPIClient(serverURL, "", "")
			loginPayload := map[string]interface{}{
				"email":    username,
				"password": pass,
			}

			if loginResp, err := loginClient.Post("/auth/login", loginPayload); err == nil {
				var loginData map[string]interface{}
				if err := ParseResponse(loginResp, &loginData); err == nil {
					if token, ok := loginData["token"].(string); ok {
						response["token"] = token
					}
				}
			}
		}
	}

	if token, ok := response["token"].(string); ok {
		fmt.Println("\n🔑 New Access Token (JWT):")
		fmt.Println(token)
		fmt.Println("\nUse this token for API access or Stream Viewer login.")
	}

	return nil
}

func runCreateUser(cmd *cobra.Command, args []string) error {
	loadConfig()

	// Check admin authentication
	if token == "" && apiKey == "" {
		return fmt.Errorf("admin authentication required. Please login first")
	}

	// Prompt for user details
	var email, password, role string

	if adminEmail != "" {
		email = adminEmail
	} else {
		if err := survey.AskOne(&survey.Input{
			Message: "Email:",
		}, &email, survey.WithValidator(survey.Required)); err != nil {
			return err
		}
	}

	if adminPassword != "" {
		password = adminPassword
	} else {
		fmt.Print("Password (min 8 chars): ")
		passwordBytes, _ := term.ReadPassword(0)
		fmt.Println()
		password = string(passwordBytes)
	}

	if len(password) < 8 {
		return fmt.Errorf("password must be at least 8 characters")
	}

	if adminRole != "" {
		role = adminRole
	} else {
		if err := survey.AskOne(&survey.Select{
			Message: "Role:",
			Options: []string{"user", "admin"},
			Default: "user",
		}, &role); err != nil {
			return err
		}
	}

	// Make API call
	payload := map[string]interface{}{
		"email":    email,
		"password": password,
		"role":     role,
	}

	client := NewAPIClient(serverURL, token, apiKey)

	resp, err := client.Post("/admin/users", payload)
	if err != nil {
		return fmt.Errorf("failed to create user: %w", err)
	}

	var response map[string]interface{}
	if err := ParseResponse(resp, &response); err != nil {
		return err
	}

	if outputJSON {
		return printJSON(response)
	}

	fmt.Printf("✅ User created successfully\n")
	if userID, ok := response["user_id"].(string); ok {
		fmt.Printf("User ID: %s\n", userID)
		fmt.Printf("Email: %s\n", email)
		fmt.Printf("Role: %s\n", role)
	}

	return nil
}

func runListUsers(cmd *cobra.Command, args []string) error {
	loadConfig()

	// Check admin authentication
	if token == "" && apiKey == "" {
		return fmt.Errorf("admin authentication required. Please login first")
	}

	client := NewAPIClient(serverURL, token, apiKey)

	resp, err := client.Get("/admin/users")
	if err != nil {
		return fmt.Errorf("failed to list users: %w", err)
	}

	var response map[string]interface{}
	if err := ParseResponse(resp, &response); err != nil {
		return err
	}

	if outputJSON {
		return printJSON(response)
	}

	users, ok := response["users"].([]interface{})
	if !ok || len(users) == 0 {
		fmt.Println("No users found")
		return nil
	}

	fmt.Printf("Total users: %d\n\n", len(users))

	table := tablewriter.NewWriter(os.Stdout)
	table.Header("Email", "Role", "Status", "Created")
	for _, u := range users {
		user := u.(map[string]interface{})
		table.Append(
			getString(user, "email"),
			getString(user, "role"),
			getString(user, "status"),
			getString(user, "created_at"),
		)
	}
	table.Render()

	return nil
}

func runDeleteUser(cmd *cobra.Command, args []string) error {
	loadConfig()

	email := args[0]

	// Check admin authentication
	if token == "" && apiKey == "" {
		return fmt.Errorf("admin authentication required. Please login first")
	}

	// Confirm deletion
	if !adminForceDelete {
		var confirm bool
		if err := survey.AskOne(&survey.Confirm{
			Message: fmt.Sprintf("Delete user '%s'?", email),
		}, &confirm); err != nil {
			return err
		}
		if !confirm {
			fmt.Println("Cancelled")
			return nil
		}
	}

	client := NewAPIClient(serverURL, token, apiKey)

	resp, err := client.Delete("/admin/users/"+email, nil)
	if err != nil {
		return fmt.Errorf("failed to delete user: %w", err)
	}

	var response map[string]interface{}
	if err := ParseResponse(resp, &response); err != nil {
		return err
	}

	if outputJSON {
		return printJSON(response)
	}

	fmt.Printf("✅ User deleted: %s\n", email)
	return nil
}

func runResetSystem(cmd *cobra.Command, args []string) error {
	loadConfig()

	// Check admin authentication
	if token == "" && apiKey == "" {
		return fmt.Errorf("admin authentication required. Please login first")
	}

	fmt.Println("\n⚠️  WARNING: SYSTEM RESET")
	fmt.Println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
	fmt.Println("This will delete ALL data:")
	fmt.Println("  - All users (including admin)")
	fmt.Println("  - All devices")
	fmt.Println("  - All stored data")
	fmt.Println("  - System configuration")
	fmt.Println("\nYou'll need to run setup again after reset.")
	fmt.Println()

	// Confirm deletion
	if !adminForceReset {
		var confirmText string
		if err := survey.AskOne(&survey.Input{
			Message: "Type 'DELETE ALL DATA' to confirm:",
		}, &confirmText); err != nil {
			return err
		}
		if confirmText != "DELETE ALL DATA" {
			fmt.Println("Cancelled - confirmation text didn't match")
			return nil
		}
	}

	client := NewAPIClient(serverURL, token, apiKey)

	resetReq := map[string]string{
		"confirm": "RESET",
	}
	resp, err := client.Delete("/admin/database/reset", resetReq)
	if err != nil {
		return fmt.Errorf("failed to reset system: %w", err)
	}

	var response map[string]interface{}
	if err := ParseResponse(resp, &response); err != nil {
		return err
	}

	if outputJSON {
		return printJSON(response)
	}

	fmt.Printf("\n✅ System reset successfully\n")
	fmt.Println("Run 'datumctl status' to check if system needs setup")

	return nil
}

func runAdminStats(cmd *cobra.Command, args []string) error {
	loadConfig()

	// Check admin authentication
	if token == "" && apiKey == "" {
		return fmt.Errorf("admin authentication required. Please login first")
	}

	client := NewAPIClient(serverURL, token, apiKey)
	resp, err := client.Get("/admin/database/stats")
	if err != nil {
		return fmt.Errorf("failed to get stats: %w", err)
	}

	var stats map[string]interface{}
	if err := ParseResponse(resp, &stats); err != nil {
		return err
	}

	if outputJSON {
		return printJSON(stats)
	}

	// Print formatted stats
	fmt.Println("\n📊 System Statistics")

	// Database stats
	if dbStats, ok := stats["database"].(map[string]interface{}); ok {
		fmt.Println("Database:")
		if users, ok := dbStats["users"].(float64); ok {
			fmt.Printf("  Users: %.0f\n", users)
		}
		if devices, ok := dbStats["devices"].(float64); ok {
			fmt.Printf("  Devices: %.0f\n", devices)
		}
		if dataPoints, ok := dbStats["data_points"].(float64); ok {
			fmt.Printf("  Data Points: %.0f\n", dataPoints)
		}
		fmt.Println()
	}

	// System info
	if sysInfo, ok := stats["system"].(map[string]interface{}); ok {
		fmt.Println("System:")
		if platf, ok := sysInfo["platform_name"].(string); ok {
			fmt.Printf("  Platform: %s\n", platf)
		}
		if uptime, ok := sysInfo["uptime"].(float64); ok {
			fmt.Printf("  Uptime: %.0fs\n", uptime)
		}
		fmt.Println()
	}

	return nil
}

func runAdminConfig(cmd *cobra.Command, args []string) error {
	loadConfig()

	// Check admin authentication
	if token == "" && apiKey == "" {
		return fmt.Errorf("admin authentication required. Please login first")
	}

	client := NewAPIClient(serverURL, token, apiKey)
	resp, err := client.Get("/admin/config")
	if err != nil {
		return fmt.Errorf("failed to get config: %w", err)
	}

	var config map[string]interface{}
	if err := ParseResponse(resp, &config); err != nil {
		return err
	}

	if outputJSON {
		return printJSON(config)
	}

	// Print formatted config
	fmt.Println("\n⚙️  System Configuration")

	if platf, ok := config["platform_name"].(string); ok {
		fmt.Printf("Platform: %s\n", platf)
	}
	if ret, ok := config["retention_days"].(float64); ok {
		fmt.Printf("Retention: %.0f days\n", ret)
	}
	if public, ok := config["public_mode"].(bool); ok {
		fmt.Printf("Public Mode: %v\n", public)
	}
	if init, ok := config["initialized"].(bool); ok {
		fmt.Printf("Initialized: %v\n", init)
	}
	fmt.Println()

	return nil
}
