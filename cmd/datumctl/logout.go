package main

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"
	"github.com/spf13/viper"
)

var logoutCmd = &cobra.Command{
	Use:   "logout",
	Short: "Log out and clear credentials",
	Long:  `Remove the saved authentication token and API key from the configuration file.`,
	RunE:  runLogout,
}

func init() {
	rootCmd.AddCommand(logoutCmd)
}

func runLogout(cmd *cobra.Command, args []string) error {
	configPath := getConfigPath()

	// Check if config exists
	if _, err := os.Stat(configPath); os.IsNotExist(err) {
		fmt.Println("Already logged out (no config file found).")
		return nil
	}

	// Clear credentials in Viper
	viper.Set("token", "")
	viper.Set("api_key", "")

	// Write back to file (effectively removing keys if they were there, or setting to empty)
	// Better approach: Read config, delete keys, write back.
	// Or just remove the file if it only contains auth?
	// Usually config might contain other things like 'server'. We should preserve 'server'.

	// Since we are using Viper and we set them to empty, WriteConfigAs will write them as empty strings.
	// To actually remove them, we might need to manipulate the map.
	// But empty string is effectively logged out for our checks.

	if err := viper.WriteConfigAs(configPath); err != nil {
		return fmt.Errorf("failed to update config file: %w", err)
	}

	fmt.Println("✅ Logged out successfully.")
	fmt.Printf("Cleared credentials from %s\n", configPath)
	return nil
}
