package main

import (
	"fmt"
	"os"
	"strings"

	"github.com/spf13/cobra"

	"datum-go/internal/cli/styles"
	"datum-go/internal/cli/utils"
)

var serverConfigCmd = &cobra.Command{
	Use:   "server-config",
	Short: "View and manage server configuration",
	Long:  "View, validate, and update the remote Datum server configuration.",
}

var serverConfigGetCmd = &cobra.Command{
	Use:   "get",
	Short: "Get current server configuration",
	RunE:  runServerConfigGet,
}

var serverConfigValidateCmd = &cobra.Command{
	Use:   "validate",
	Short: "Validate server connectivity and configuration",
	Long:  "Run a series of connectivity and configuration checks against the server.",
	RunE:  runServerConfigValidate,
}

var serverConfigRetentionCmd = &cobra.Command{
	Use:   "retention <days>",
	Short: "Update data retention policy",
	Args:  cobra.ExactArgs(1),
	RunE:  runServerConfigRetention,
}

func init() {
	rootCmd.AddCommand(serverConfigCmd)
	serverConfigCmd.AddCommand(serverConfigGetCmd)
	serverConfigCmd.AddCommand(serverConfigValidateCmd)
	serverConfigCmd.AddCommand(serverConfigRetentionCmd)
}

func runServerConfigGet(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	resp, err := client.Get("/admin/config")
	if err != nil {
		return fmt.Errorf("failed to get server config: %w", err)
	}

	var config map[string]interface{}
	if err := ParseResponse(resp, &config); err != nil {
		return err
	}

	if outputJSON {
		return utils.PrintJSON(os.Stdout, config)
	}

	fmt.Println(styles.Header("⚙️  Server Configuration"))

	for key, val := range config {
		switch v := val.(type) {
		case map[string]interface{}:
			fmt.Println(styles.KVPadded(key, 25, ""))
			for k2, v2 := range v {
				fmt.Println(styles.KVPadded("  "+k2, 25, fmt.Sprintf("%v", v2)))
			}
		default:
			fmt.Println(styles.KVPadded(key, 25, fmt.Sprintf("%v", v)))
		}
	}
	fmt.Println()

	return nil
}

func runServerConfigValidate(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	type check struct {
		name string
		path string
	}

	checks := []check{
		{"Health endpoint", "/health"},
		{"Readiness endpoint", "/ready"},
		{"System status", "/sys/status"},
		{"Metrics endpoint", "/sys/metrics"},
	}

	fmt.Println(styles.Header("🔍 Server Configuration Validation"))
	fmt.Println(styles.KVPadded("Server", 12, serverURL))
	fmt.Println()

	allOK := true
	for _, c := range checks {
		resp, err := client.Get(c.path)
		icon := "✅"
		detail := "OK"
		if err != nil {
			icon = "❌"
			detail = err.Error()
			allOK = false
		} else if resp.StatusCode >= 400 {
			icon = "⚠️"
			detail = fmt.Sprintf("HTTP %d", resp.StatusCode)
			if resp.StatusCode == 401 || resp.StatusCode == 403 {
				detail += " (auth required)"
			}
			allOK = false
		} else {
			resp.Body.Close()
		}
		fmt.Printf("  %s %-25s %s\n", icon, c.name, detail)
	}

	fmt.Println()

	// Check MQTT
	fmt.Printf("  ℹ️  %-25s %s\n", "MQTT endpoint", "tcp://"+strings.TrimPrefix(strings.TrimPrefix(serverURL, "https://"), "http://")+":1883")
	fmt.Printf("  ℹ️  %-25s %s\n", "MQTT WS endpoint", "ws://"+strings.TrimPrefix(strings.TrimPrefix(serverURL, "https://"), "http://")+":1884")

	if allOK {
		fmt.Println("\n✅ All checks passed")
	} else {
		fmt.Println("\n⚠️  Some checks failed")
	}

	return nil
}

func runServerConfigRetention(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	body := map[string]interface{}{"days": args[0]}
	resp, err := client.Put("/admin/config/retention", body)
	if err != nil {
		return fmt.Errorf("failed to update retention: %w", err)
	}

	var result map[string]interface{}
	if err := ParseResponse(resp, &result); err != nil {
		return err
	}

	fmt.Println("✅ Retention policy updated")
	return nil
}
