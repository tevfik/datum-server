package main

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"
)

var (
	// Build time variables
	DefaultServerURL = "http://localhost:8000"

	// Global flags
	serverURL  string
	apiKey     string
	token      string
	outputJSON bool
	configFile string
	verbose    bool
)

var rootCmd = &cobra.Command{
	Use:   "datumctl",
	Short: "Datum IoT Platform CLI",
	Long: `datumctl - Command-line interface for Datum IoT Platform

Interactive Mode (Recommended):
  datumctl interactive    # Launch interactive menu with guided options
  datumctl i              # Short alias

Direct Commands:
  # Login and save credentials
  datumctl login --email admin@example.com

  # List all devices
  datumctl device list

  # Get device data
  datumctl data get --device mydevice --last 1h

For more information, visit: https://github.com/yourusername/datum-server`,
}

func init() {
	// Load config before command execution
	cobra.OnInitialize(loadConfig)

	// Disable auto-completion command (not needed for interactive mode)
	rootCmd.CompletionOptions.DisableDefaultCmd = true

	// Global flags
	rootCmd.PersistentFlags().StringVar(&serverURL, "server", DefaultServerURL, "Datum server URL")
	rootCmd.PersistentFlags().StringVar(&apiKey, "api-key", "", "API key for authentication")
	rootCmd.PersistentFlags().StringVar(&token, "token", "", "JWT token for authentication")
	rootCmd.PersistentFlags().BoolVar(&outputJSON, "json", false, "Output in JSON format")
	rootCmd.PersistentFlags().StringVar(&configFile, "config", "", "Config file (default: $HOME/.datumctl.yaml or /root/data/.datumctl.yaml)")
	rootCmd.PersistentFlags().BoolVarP(&verbose, "verbose", "v", false, "Verbose output")
}

func main() {
	if err := rootCmd.Execute(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}
