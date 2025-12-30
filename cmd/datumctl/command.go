package main

import (
	"encoding/json"
	"fmt"
	"os"
	"strings"
	"time"

	"github.com/olekukonko/tablewriter"
	"github.com/spf13/cobra"
)

var commandCmd = &cobra.Command{
	Use:   "command",
	Short: "Send and manage device commands",
	Long: `Send commands to devices and monitor command execution status.

Commands are queued on the server and delivered to devices when they poll.
Devices can acknowledge commands and report execution results.`,
}

var commandSendCmd = &cobra.Command{
	Use:   "send <device-id> <action>",
	Short: "Send a command to a device",
	Long: `Send a command to a device for execution.

The command will be queued and delivered when the device polls for commands.
You can specify parameters using --param flags.`,
	Args: cobra.ExactArgs(2),
	Example: `  # Send a simple command
  datumctl command send device-001 reboot

  # Send command with parameters
  datumctl command send device-001 set-config --param key=interval --param value=60

  # Send command with JSON parameters
  datumctl command send device-001 update --param config='{"interval":60,"mode":"auto"}'`,
	RunE: runCommandSend,
}

var commandListCmd = &cobra.Command{
	Use:   "list <device-id>",
	Short: "List commands for a device",
	Long:  "List all pending and historical commands for a device.",
	Args:  cobra.ExactArgs(1),
	Example: `  # List all commands
  datumctl command list device-001

  # List in JSON format
  datumctl command list device-001 --json`,
	RunE: runCommandList,
}

var commandGetCmd = &cobra.Command{
	Use:   "get <device-id> <command-id>",
	Short: "Get command details",
	Long:  "Get detailed information about a specific command including status and execution results.",
	Args:  cobra.ExactArgs(2),
	Example: `  datumctl command get device-001 cmd_abc123
  datumctl command get device-001 cmd_abc123 --json`,
	RunE: runCommandGet,
}

var commandHistoryCmd = &cobra.Command{
	Use:   "history <device-id>",
	Short: "Show command execution history",
	Long:  "Display the history of all commands sent to a device with their execution status.",
	Args:  cobra.ExactArgs(1),
	Example: `  # Show command history
  datumctl command history device-001

  # Show last 10 commands
  datumctl command history device-001 --limit 10

  # Show only failed commands
  datumctl command history device-001 --status failed`,
	RunE: runCommandHistory,
}

var commandCancelCmd = &cobra.Command{
	Use:     "cancel <device-id> <command-id>",
	Short:   "Cancel a pending command",
	Long:    "Cancel a pending command that has not been executed yet.",
	Args:    cobra.ExactArgs(2),
	Example: `  datumctl command cancel device-001 cmd_abc123`,
	RunE:    runCommandCancel,
}

// Flags
var (
	commandParams    []string
	commandLimit     int
	commandStatus    string
	commandWait      bool
	commandJSON      string
	commandExpiresIn int
)

func init() {
	rootCmd.AddCommand(commandCmd)

	commandCmd.AddCommand(commandSendCmd)
	commandCmd.AddCommand(commandListCmd)
	commandCmd.AddCommand(commandGetCmd)
	commandCmd.AddCommand(commandHistoryCmd)
	commandCmd.AddCommand(commandCancelCmd)

	// Send command flags
	commandSendCmd.Flags().StringArrayVarP(&commandParams, "param", "p", []string{}, "Command parameters (key=value)")
	commandSendCmd.Flags().StringVar(&commandJSON, "params-json", "", "Parameters as JSON string")
	commandSendCmd.Flags().BoolVarP(&commandWait, "wait", "w", false, "Wait for command execution and show result")
	commandSendCmd.Flags().IntVar(&commandExpiresIn, "expires-in", 0, "Command expiration time in seconds (default 86400)")

	// History flags
	commandHistoryCmd.Flags().IntVarP(&commandLimit, "limit", "l", 0, "Limit number of results")
	commandHistoryCmd.Flags().StringVar(&commandStatus, "status", "", "Filter by status (pending, delivered, success, failed)")
}

func runCommandSend(cmd *cobra.Command, args []string) error {
	loadConfig()
	deviceID := args[0]
	action := args[1]

	// Parse parameters
	params := make(map[string]interface{})

	// If JSON params provided
	if commandJSON != "" {
		if err := json.Unmarshal([]byte(commandJSON), &params); err != nil {
			return fmt.Errorf("invalid JSON parameters: %w", err)
		}
	}

	// Add individual params (override JSON if both provided)
	for _, p := range commandParams {
		parts := strings.SplitN(p, "=", 2)
		if len(parts) != 2 {
			return fmt.Errorf("invalid parameter format '%s', use key=value", p)
		}
		key := parts[0]
		value := parts[1]

		// Try to parse as JSON for complex values
		var jsonValue interface{}
		if err := json.Unmarshal([]byte(value), &jsonValue); err == nil {
			params[key] = jsonValue
		} else {
			params[key] = value
		}
	}

	// Build request
	requestBody := map[string]interface{}{
		"action": action,
	}
	if len(params) > 0 {
		requestBody["params"] = params
	}
	if commandExpiresIn > 0 {
		requestBody["expires_in"] = commandExpiresIn
	}

	client := NewAPIClient(serverURL, token, apiKey)
	resp, err := client.Post(fmt.Sprintf("/devices/%s/commands", deviceID), requestBody)
	if err != nil {
		return err
	}

	var result map[string]interface{}
	if err := ParseResponse(resp, &result); err != nil {
		return err
	}

	if outputJSON {
		return printJSON(result)
	}

	fmt.Printf("✓ Command sent successfully\n\n")
	fmt.Printf("📤 Command ID: %s\n", result["command_id"])
	fmt.Printf("📊 Status: %s\n", result["status"])
	fmt.Printf("💬 Message: %s\n", result["message"])
	if expiresAt, ok := result["expires_at"]; ok {
		fmt.Printf("⏰ Expires: %s\n", expiresAt)
	}

	// Wait for execution if requested
	if commandWait {
		commandID := result["command_id"].(string)
		fmt.Println("\n⏳ Waiting for command execution...")

		// Poll for result (max 30 seconds)
		for i := 0; i < 30; i++ {
			time.Sleep(1 * time.Second)

			statusResp, err := client.Get(fmt.Sprintf("/devices/%s/commands/%s", deviceID, commandID))
			if err != nil {
				continue
			}

			var statusResult map[string]interface{}
			if err := ParseResponse(statusResp, &statusResult); err != nil {
				continue
			}

			status := statusResult["status"].(string)
			if status == "success" || status == "failed" {
				fmt.Printf("\n✓ Command %s\n", status)
				if result, ok := statusResult["result"]; ok {
					fmt.Printf("📋 Result: %v\n", result)
				}
				if msg, ok := statusResult["error"]; ok && status == "failed" {
					fmt.Printf("❌ Error: %v\n", msg)
				}
				break
			}

			fmt.Print(".")
		}
	}

	return nil
}

func runCommandList(cmd *cobra.Command, args []string) error {
	loadConfig()
	deviceID := args[0]

	client := NewAPIClient(serverURL, token, apiKey)
	resp, err := client.Get(fmt.Sprintf("/devices/%s/commands", deviceID))
	if err != nil {
		return err
	}

	var result map[string]interface{}
	if err := ParseResponse(resp, &result); err != nil {
		return err
	}

	if outputJSON {
		return printJSON(result)
	}

	commands, ok := result["commands"].([]interface{})
	if !ok || len(commands) == 0 {
		fmt.Println("\n📭 No commands found for this device")
		fmt.Println("\nSend a command:")
		fmt.Printf("  datumctl command send %s <action>\n", deviceID)
		return nil
	}

	// Display as table
	table := tablewriter.NewWriter(os.Stdout)
	table.Header("Command ID", "Action", "Status", "Created", "Params")

	for _, cmdInterface := range commands {
		cmdMap := cmdInterface.(map[string]interface{})
		commandID := cmdMap["id"].(string)
		action := cmdMap["action"].(string)
		status := cmdMap["status"].(string)
		createdAt := cmdMap["created_at"].(string)

		// Format params
		paramsStr := ""
		if params, ok := cmdMap["params"].(map[string]interface{}); ok && len(params) > 0 {
			paramsJSON, _ := json.Marshal(params)
			paramsStr = string(paramsJSON)
			if len(paramsStr) > 30 {
				paramsStr = paramsStr[:27] + "..."
			}
		}

		// Parse time
		t, _ := time.Parse(time.RFC3339, createdAt)
		createdStr := t.Format("Jan 02 15:04")

		// Status emoji
		statusEmoji := "⏳"
		switch status {
		case "success":
			statusEmoji = "✅"
		case "failed":
			statusEmoji = "❌"
		case "delivered":
			statusEmoji = "📤"
		}

		table.Append(
			commandID,
			action,
			fmt.Sprintf("%s %s", statusEmoji, status),
			createdStr,
			paramsStr,
		)
	}

	fmt.Printf("\n📋 Commands for device: %s\n\n", deviceID)
	table.Render()
	fmt.Printf("\nTotal: %d commands\n", len(commands))

	return nil
}

func runCommandGet(cmd *cobra.Command, args []string) error {
	loadConfig()
	deviceID := args[0]
	commandID := args[1]

	client := NewAPIClient(serverURL, token, apiKey)
	resp, err := client.Get(fmt.Sprintf("/devices/%s/commands/%s", deviceID, commandID))
	if err != nil {
		return err
	}

	var result map[string]interface{}
	if err := ParseResponse(resp, &result); err != nil {
		return err
	}

	if outputJSON {
		return printJSON(result)
	}

	// Display command details
	fmt.Printf("\n📋 Command Details\n")
	fmt.Printf("═══════════════════════════════════════════\n\n")
	fmt.Printf("🆔 Command ID: %s\n", result["id"])
	fmt.Printf("📱 Device ID:  %s\n", result["device_id"])
	fmt.Printf("⚡ Action:     %s\n", result["action"])
	fmt.Printf("📊 Status:     %s\n", result["status"])

	if params, ok := result["params"].(map[string]interface{}); ok && len(params) > 0 {
		fmt.Printf("\n📝 Parameters:\n")
		paramsJSON, _ := json.MarshalIndent(params, "  ", "  ")
		fmt.Printf("  %s\n", string(paramsJSON))
	}

	if created, ok := result["created_at"].(string); ok {
		t, _ := time.Parse(time.RFC3339, created)
		fmt.Printf("\n🕐 Created:    %s\n", t.Format("2006-01-02 15:04:05"))
	}

	if delivered, ok := result["delivered_at"].(string); ok && delivered != "" {
		t, _ := time.Parse(time.RFC3339, delivered)
		fmt.Printf("📤 Delivered:  %s\n", t.Format("2006-01-02 15:04:05"))
	}

	if completed, ok := result["completed_at"].(string); ok && completed != "" {
		t, _ := time.Parse(time.RFC3339, completed)
		fmt.Printf("✅ Completed:  %s\n", t.Format("2006-01-02 15:04:05"))
	}

	if resultData, ok := result["result"]; ok && resultData != nil {
		fmt.Printf("\n📋 Result:\n")
		resultJSON, _ := json.MarshalIndent(resultData, "  ", "  ")
		fmt.Printf("  %s\n", string(resultJSON))
	}

	if errorMsg, ok := result["error"].(string); ok && errorMsg != "" {
		fmt.Printf("\n❌ Error: %s\n", errorMsg)
	}

	return nil
}

func runCommandHistory(cmd *cobra.Command, args []string) error {
	loadConfig()
	deviceID := args[0]

	// Build query parameters
	queryParams := ""
	if commandLimit > 0 {
		queryParams += fmt.Sprintf("?limit=%d", commandLimit)
	}
	if commandStatus != "" {
		if queryParams == "" {
			queryParams += "?"
		} else {
			queryParams += "&"
		}
		queryParams += fmt.Sprintf("status=%s", commandStatus)
	}

	client := NewAPIClient(serverURL, token, apiKey)
	resp, err := client.Get(fmt.Sprintf("/devices/%s/commands%s", deviceID, queryParams))
	if err != nil {
		return err
	}

	var result map[string]interface{}
	if err := ParseResponse(resp, &result); err != nil {
		return err
	}

	if outputJSON {
		return printJSON(result)
	}

	commands, ok := result["commands"].([]interface{})
	if !ok || len(commands) == 0 {
		fmt.Println("\n📭 No command history found")
		return nil
	}

	// Statistics
	statusCounts := make(map[string]int)
	for _, cmdInterface := range commands {
		cmdMap := cmdInterface.(map[string]interface{})
		status := cmdMap["status"].(string)
		statusCounts[status]++
	}

	fmt.Printf("\n📊 Command History for device: %s\n", deviceID)
	fmt.Printf("═══════════════════════════════════════════\n\n")
	fmt.Printf("Total Commands: %d\n", len(commands))
	if len(statusCounts) > 0 {
		fmt.Printf("\nStatus Breakdown:\n")
		for status, count := range statusCounts {
			emoji := "⏳"
			switch status {
			case "success":
				emoji = "✅"
			case "failed":
				emoji = "❌"
			case "delivered":
				emoji = "📤"
			case "pending":
				emoji = "⏳"
			}
			fmt.Printf("  %s %s: %d\n", emoji, status, count)
		}
	}

	fmt.Println("\nRecent Commands:")
	fmt.Println("───────────────────────────────────────────")

	// Show command list
	for i, cmdInterface := range commands {
		if commandLimit > 0 && i >= commandLimit {
			break
		}

		cmdMap := cmdInterface.(map[string]interface{})
		commandID := cmdMap["id"].(string)
		action := cmdMap["action"].(string)
		status := cmdMap["status"].(string)
		createdAt := cmdMap["created_at"].(string)

		t, _ := time.Parse(time.RFC3339, createdAt)

		statusEmoji := "⏳"
		switch status {
		case "success":
			statusEmoji = "✅"
		case "failed":
			statusEmoji = "❌"
		case "delivered":
			statusEmoji = "📤"
		}

		fmt.Printf("\n%s [%s] %s\n", statusEmoji, t.Format("Jan 02 15:04"), action)
		fmt.Printf("   ID: %s\n", commandID)

		if params, ok := cmdMap["params"].(map[string]interface{}); ok && len(params) > 0 {
			paramsJSON, _ := json.Marshal(params)
			fmt.Printf("   Params: %s\n", string(paramsJSON))
		}
	}

	return nil
}

func runCommandCancel(cmd *cobra.Command, args []string) error {
	loadConfig()
	deviceID := args[0]
	commandID := args[1]

	client := NewAPIClient(serverURL, token, apiKey)
	resp, err := client.Delete(fmt.Sprintf("/devices/%s/commands/%s", deviceID, commandID), nil)
	if err != nil {
		return err
	}

	var result map[string]interface{}
	if err := ParseResponse(resp, &result); err != nil {
		return err
	}

	if outputJSON {
		return printJSON(result)
	}

	fmt.Printf("✓ Command cancelled successfully\n")
	fmt.Printf("📋 Command ID: %s\n", commandID)

	return nil
}
