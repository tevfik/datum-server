package main

import (
	"fmt"
	"syscall"

	"github.com/spf13/cobra"
	"golang.org/x/term"
)

var registerCmd = &cobra.Command{
	Use:   "register",
	Short: "Create a new user account",
	Long: `Register a new user account on the Datum server.
	
If public registration is enabled on the server, this will create a new user 
and automatically log you in.`,
	Example: `  # Interactive registration
  datumctl register

  # Register with flags
  datumctl register --email user@example.com --password secretpass`,
	RunE: runRegister,
}

var (
	registerEmail    string
	registerPassword string
	registerSave     bool
)

func init() {
	rootCmd.AddCommand(registerCmd)

	registerCmd.Flags().StringVar(&registerEmail, "email", "", "User email")
	registerCmd.Flags().StringVar(&registerPassword, "password", "", "User password")
	registerCmd.Flags().BoolVar(&registerSave, "save", true, "Save credentials after registration")
}

func runRegister(cmd *cobra.Command, args []string) error {
	// Prompt for email if not provided
	if registerEmail == "" {
		fmt.Print("Email: ")
		fmt.Scanln(&registerEmail)
	}

	if registerEmail == "" {
		return fmt.Errorf("email is required")
	}

	// Prompt for password if not provided
	if registerPassword == "" {
		fmt.Print("Password (min 8 chars): ")
		passwordBytes, err := term.ReadPassword(int(syscall.Stdin))
		fmt.Println()
		if err != nil {
			return fmt.Errorf("failed to read password: %w", err)
		}
		registerPassword = string(passwordBytes)
	}

	if len(registerPassword) < 8 {
		return fmt.Errorf("password must be at least 8 characters")
	}

	// Make register request
	client := NewAPIClient(serverURL, "", "")

	registerReq := map[string]string{
		"email":    registerEmail,
		"password": registerPassword,
	}

	fmt.Println("Registering...")
	resp, err := client.Post("/auth/register", registerReq)
	if err != nil {
		return fmt.Errorf("registration failed: %w", err)
	}

	var registerResp struct {
		UserID string `json:"user_id"`
		Token  string `json:"token"`
		Role   string `json:"role"`
	}

	if err := ParseResponse(resp, &registerResp); err != nil {
		return fmt.Errorf("failed to parse response: %w", err)
	}

	fmt.Printf("\n✅ Registration successful!\n")
	fmt.Printf("   User ID: %s\n", registerResp.UserID)
	fmt.Printf("   Role: %s\n", registerResp.Role)

	// Save credentials if requested
	if registerSave && registerResp.Token != "" {
		if err := saveToken(registerResp.Token); err != nil {
			fmt.Printf("⚠️  Failed to save token: %v\n", err)
		} else {
			fmt.Printf("✅ Logged in and token saved to config.\n")
		}
	} else if registerResp.Token != "" {
		fmt.Printf("\n🔑 Your access token:\n%s\n", registerResp.Token)
	}

	return nil
}
