package main

import (
	"fmt"
	"os"
	"path/filepath"
	"syscall"

	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"golang.org/x/term"
)

var loginCmd = &cobra.Command{
	Use:   "login",
	Short: "Authenticate with Datum server",
	Long: `Login to Datum server and save credentials.

The credentials will be saved to ~/.datumctl.yaml for future use.
You can also use --api-key flag to save an API key instead.`,
	Example: `  # Login with email/password
  datumctl login --email admin@example.com

  # Save API key
  datumctl login --api-key your-device-api-key

  # Specify server URL
  datumctl login --server https://datum.example.com --email admin@example.com`,
	RunE: runLogin,
}

var (
	loginEmail    string
	loginPassword string
	loginAPIKey   string
	loginSaveKey  bool
)

func init() {
	rootCmd.AddCommand(loginCmd)

	loginCmd.Flags().StringVar(&loginEmail, "email", "", "User email")
	loginCmd.Flags().StringVar(&loginPassword, "password", "", "User password (will prompt if not provided)")
	loginCmd.Flags().StringVar(&loginAPIKey, "api-key", "", "API key to save")
	loginCmd.Flags().BoolVar(&loginSaveKey, "save", true, "Save credentials to config file")
}

func runLogin(cmd *cobra.Command, args []string) error {
	// If API key is provided, just save it
	if loginAPIKey != "" {
		return saveAPIKey(loginAPIKey)
	}

	// Email/password login
	if loginEmail == "" {
		return fmt.Errorf("email is required (use --email flag)")
	}

	// Prompt for password if not provided
	if loginPassword == "" {
		fmt.Print("Password: ")
		passwordBytes, err := term.ReadPassword(int(syscall.Stdin))
		fmt.Println()
		if err != nil {
			return fmt.Errorf("failed to read password: %w", err)
		}
		loginPassword = string(passwordBytes)
	}

	// Make login request
	client := NewAPIClient(serverURL, "", "")

	loginReq := map[string]string{
		"email":    loginEmail,
		"password": loginPassword,
	}

	resp, err := client.Post("/auth/login", loginReq)
	if err != nil {
		return fmt.Errorf("login failed: %w", err)
	}

	var loginResp struct {
		Token        string `json:"token"`
		RefreshToken string `json:"refresh_token"`
		User         struct {
			ID    string `json:"id"`
			Email string `json:"email"`
			Role  string `json:"role"`
		} `json:"user"`
	}

	if err := ParseResponse(resp, &loginResp); err != nil {
		return err
	}

	fmt.Printf("✅ Login successful!\n")
	fmt.Printf("   User: %s (%s)\n", loginResp.User.Email, loginResp.User.Role)
	fmt.Printf("   Token: %s...\n", loginResp.Token[:20])

	// Save credentials
	if loginSaveKey {
		if err := saveToken(loginResp.Token, loginResp.RefreshToken); err != nil {
			return fmt.Errorf("failed to save token: %w", err)
		}
		fmt.Printf("   Saved to: %s\n", getConfigPath())
	}

	return nil
}

func saveToken(token, refreshToken string) error {
	viper.Set("server", serverURL)
	viper.Set("token", token)
	if refreshToken != "" {
		viper.Set("refresh_token", refreshToken)
	}

	configPath := getConfigPath()
	if err := os.MkdirAll(filepath.Dir(configPath), 0700); err != nil {
		return err
	}

	return viper.WriteConfigAs(configPath)
}

func saveAPIKey(apiKey string) error {
	viper.Set("server", serverURL)
	viper.Set("api_key", apiKey)

	configPath := getConfigPath()
	if err := os.MkdirAll(filepath.Dir(configPath), 0700); err != nil {
		return err
	}

	if err := viper.WriteConfigAs(configPath); err != nil {
		return err
	}

	fmt.Printf("✅ API key saved to: %s\n", configPath)
	return nil
}

func getConfigPath() string {
	if configFile != "" {
		return configFile
	}

	// If running in Docker container with mounted data volume, use it for persistence
	if _, err := os.Stat("/root/data"); err == nil {
		return "/root/data/.datumctl.yaml"
	}

	home, _ := os.UserHomeDir()
	return filepath.Join(home, ".datumctl.yaml")
}

func loadConfig() {
	if configFile != "" {
		viper.SetConfigFile(configFile)
	} else {
		// Search in /root/data first (for Docker persistence)
		if _, err := os.Stat("/root/data"); err == nil {
			viper.AddConfigPath("/root/data")
		}

		home, _ := os.UserHomeDir()
		viper.AddConfigPath(home)
		viper.SetConfigName(".datumctl")
		viper.SetConfigType("yaml")
	}

	viper.AutomaticEnv()
	viper.ReadInConfig()

	// Load from config if not set via flags
	// Note: We check against the default value defined in main.go
	if serverURL == DefaultServerURL && viper.IsSet("server") {
		serverURL = viper.GetString("server")
	}
	if token == "" && viper.IsSet("token") {
		token = viper.GetString("token")
	}
	if viper.IsSet("refresh_token") {
		// We don't have a global var for refresh_token in main package yet, strictly speaking.
		// But let's assume we might need it.
		// Actually, let's just leave it in viper for now, the client will read it.
	}
	if apiKey == "" && viper.IsSet("api_key") {
		apiKey = viper.GetString("api_key")
	}
}
