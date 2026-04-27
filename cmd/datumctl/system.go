package main

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"

	"datum-go/internal/cli/styles"
	"datum-go/internal/cli/utils"
)

var Version = "1.1.0"

var statusCmd = &cobra.Command{
	Use:   "status",
	Short: "Check server status",
	Long:  "Check if the Datum server is reachable and get system information",
	RunE:  runStatus,
}

var versionCmd = &cobra.Command{
	Use:   "version",
	Short: "Show version information",
	Run: func(cmd *cobra.Command, args []string) {
		fmt.Printf("datumctl version %s\n", Version)
		fmt.Println("Datum IoT Platform CLI")
	},
}

var configCmd = &cobra.Command{
	Use:   "config",
	Short: "Manage configuration",
	Long:  "View or modify datumctl configuration",
}

var configShowCmd = &cobra.Command{
	Use:   "show",
	Short: "Show current configuration",
	Run:   runConfigShow,
}

func init() {
	rootCmd.AddCommand(statusCmd)
	rootCmd.AddCommand(versionCmd)
	rootCmd.AddCommand(configCmd)

	configCmd.AddCommand(configShowCmd)
}

func runStatus(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	resp, err := client.Get("/health")
	if err != nil {
		fmt.Printf("❌ Server unreachable: %v\n", err)
		return err
	}

	var health map[string]interface{}
	if err := ParseResponse(resp, &health); err != nil {
		return err
	}

	if outputJSON {
		return utils.PrintJSON(os.Stdout, health)
	}

	fmt.Println(styles.Header("✅ Server Status"))
	fmt.Println(styles.KVPadded("URL", 10, serverURL))
	fmt.Println(styles.KVPadded("Status", 10, styles.StatusIcon(utils.GetString(health, "status"))+" "+utils.GetString(health, "status")))
	fmt.Println(styles.KVPadded("Version", 10, utils.GetString(health, "version")))
	fmt.Println(styles.KVPadded("Uptime", 10, utils.GetString(health, "uptime")))
	fmt.Println()

	return nil
}

func runConfigShow(cmd *cobra.Command, args []string) {
	loadConfig()

	fmt.Println(styles.Header("⚙️  Configuration"))
	fmt.Println(styles.KVPadded("Config file", 13, getConfigPath()))
	fmt.Println(styles.KVPadded("Server", 13, serverURL))

	if token != "" {
		fmt.Printf("  Token:       %s...\n", token[:min(20, len(token))])
	} else {
		fmt.Printf("  Token:       (not set)\n")
	}

	if apiKey != "" {
		fmt.Printf("  API Key:     %s...\n", apiKey[:min(20, len(apiKey))])
	} else {
		fmt.Printf("  API Key:     (not set)\n")
	}

	fmt.Println()

	// Show helpful info
	if token == "" && apiKey == "" {
		fmt.Println("💡 Not authenticated. Login with:")
		fmt.Println("   datumctl login --email your@email.com")
		fmt.Println("   OR")
		fmt.Println("   datumctl setup    (for first-time setup)")
		fmt.Println()
	} else if token != "" {
		fmt.Println("💡 Logged in with user token (recommended for CLI usage)")
		fmt.Println()
	} else if apiKey != "" {
		fmt.Println("💡 Using device API key")
		fmt.Println("   Note: Device API keys have limited permissions")
		fmt.Println()
	}

	fmt.Println("ℹ️  API Keys vs Tokens:")
	fmt.Println("   • Tokens: For users (login with email/password)")
	fmt.Println("   • API Keys: For IoT devices (created with 'device create')")
	fmt.Println()
	fmt.Println("   To save a device API key:")
	fmt.Println("   datumctl login --api-key YOUR_DEVICE_API_KEY")
	fmt.Println()
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}
