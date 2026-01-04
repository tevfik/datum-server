package main

import (
	"fmt"
	"os"
	"time"

	"github.com/olekukonko/tablewriter"
	"github.com/spf13/cobra"
)

var keyCmd = &cobra.Command{
	Use:   "key",
	Short: "Manage User API Keys",
	Long:  "Create, list, and delete persistent User API keys (ak_...)",
}

var keyListCmd = &cobra.Command{
	Use:   "list",
	Short: "List your API keys",
	RunE:  runKeyList,
}

var keyCreateCmd = &cobra.Command{
	Use:   "create",
	Short: "Create a new API key",
	Long: `Create a new persistent User API key.
The key will be displayed once and cannot be retrieved later.`,
	Example: `  datumctl key create --name "VLC Laptop"`,
	RunE:    runKeyCreate,
}

var keyDeleteCmd = &cobra.Command{
	Use:   "delete [key-id]",
	Short: "Delete an API key",
	Args:  cobra.ExactArgs(1),
	RunE:  runKeyDelete,
}

var (
	keyName string
)

func init() {
	rootCmd.AddCommand(keyCmd)

	keyCmd.AddCommand(keyListCmd)
	keyCmd.AddCommand(keyCreateCmd)
	keyCmd.AddCommand(keyDeleteCmd)

	keyCreateCmd.Flags().StringVar(&keyName, "name", "", "Key name (required)")
	keyCreateCmd.MarkFlagRequired("name")
}

func runKeyList(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	resp, err := client.Get("/auth/keys")
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

	keysInterface, ok := result["keys"]
	if !ok {
		fmt.Println("No keys found.")
		return nil
	}
	keys, ok := keysInterface.([]interface{})
	if !ok || len(keys) == 0 {
		fmt.Println("No keys found.")
		return nil
	}

	table := tablewriter.NewWriter(os.Stdout)
	table.Header("ID", "Name", "Key Masked", "Created At")

	for _, kInterface := range keys {
		k, ok := kInterface.(map[string]interface{})
		if !ok {
			continue
		}
		id := getString(k, "id")
		name := getString(k, "name")
		keyMasked := getString(k, "key")
		created := getString(k, "created_at")

		if t, err := time.Parse(time.RFC3339, created); err == nil {
			created = t.Local().Format("2006-01-02 15:04:05")
		}

		table.Append(id, name, keyMasked, created)
	}
	table.Render()
	return nil
}

func runKeyCreate(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	req := map[string]string{
		"name": keyName,
	}

	resp, err := client.Post("/auth/keys", req)
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

	fmt.Printf("\n✅ API Key Created!\n\n")
	fmt.Printf("  ID:   %s\n", getString(result, "id"))
	fmt.Printf("  Name: %s\n", getString(result, "name"))
	fmt.Printf("\n  🔑 Key: %s\n", getString(result, "key"))
	fmt.Printf("  ⚠️  Save this key now! It won't be shown again.\n\n")

	return nil
}

func runKeyDelete(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)
	keyID := args[0]

	resp, err := client.Delete("/auth/keys/"+keyID, nil)
	if err != nil {
		return err
	}

	if err := ParseResponse(resp, nil); err != nil {
		return err
	}

	fmt.Printf("✅ Key '%s' deleted.\n", keyID)
	return nil
}
