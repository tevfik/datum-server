package main

import (
	"encoding/json"
	"fmt"
	"os"
	"strings"

	"github.com/olekukonko/tablewriter"
	"github.com/spf13/cobra"

	"datum-go/internal/cli/utils"
)

var dbCmd = &cobra.Command{
	Use:   "db",
	Short: "Document database operations",
	Long: `Manage user-scoped document collections.

Collections are JSON document stores that can be used to persist
user data such as todos, settings, profiles, etc.`,
}

var dbListCmd = &cobra.Command{
	Use:   "list <collection>",
	Short: "List documents in a collection",
	Args:  cobra.ExactArgs(1),
	RunE:  runDBList,
}

var dbGetCmd = &cobra.Command{
	Use:   "get <collection> <id>",
	Short: "Get a specific document",
	Args:  cobra.ExactArgs(2),
	RunE:  runDBGet,
}

var dbCreateCmd = &cobra.Command{
	Use:   "create <collection> <json>",
	Short: "Create a new document",
	Long: `Create a new document in the specified collection.

Example:
  datumctl db create todos '{"title":"Buy milk","done":false}'`,
	Args: cobra.ExactArgs(2),
	RunE: runDBCreate,
}

var dbUpdateCmd = &cobra.Command{
	Use:   "update <collection> <id> <json>",
	Short: "Update an existing document",
	Args:  cobra.ExactArgs(3),
	RunE:  runDBUpdate,
}

var dbDeleteCmd = &cobra.Command{
	Use:   "delete <collection> <id>",
	Short: "Delete a document",
	Args:  cobra.ExactArgs(2),
	RunE:  runDBDelete,
}

var dbCollectionsCmd = &cobra.Command{
	Use:   "collections",
	Short: "List all collections (admin only)",
	Long:  `List all collections across all users. Requires admin privileges.`,
	RunE:  runDBCollections,
}

func init() {
	rootCmd.AddCommand(dbCmd)
	dbCmd.AddCommand(dbListCmd)
	dbCmd.AddCommand(dbGetCmd)
	dbCmd.AddCommand(dbCreateCmd)
	dbCmd.AddCommand(dbUpdateCmd)
	dbCmd.AddCommand(dbDeleteCmd)
	dbCmd.AddCommand(dbCollectionsCmd)
}

func runDBList(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)
	collection := args[0]

	resp, err := client.Get("/auth/db/" + collection)
	if err != nil {
		return err
	}

	var docs []map[string]interface{}
	if err := ParseResponse(resp, &docs); err != nil {
		return err
	}

	if outputJSON {
		return utils.PrintJSON(os.Stdout, docs)
	}

	if len(docs) == 0 {
		fmt.Printf("No documents in collection '%s'\n", collection)
		return nil
	}

	fmt.Printf("\n📂 Collection: %s (%d documents)\n\n", collection, len(docs))

	table := tablewriter.NewWriter(os.Stdout)
	table.Header("ID", "Created", "Preview")

	for _, doc := range docs {
		id := fmt.Sprintf("%v", doc["id"])
		created := ""
		if c, ok := doc["_created_at"].(string); ok && len(c) >= 10 {
			created = c[:10] // Date only
		}
		// Preview: first 50 chars of JSON minus id and meta fields
		preview := docPreview(doc, 50)
		table.Append(id, created, preview)
	}

	table.Render()
	return nil
}

func runDBGet(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)
	collection := args[0]
	docID := args[1]

	resp, err := client.Get("/auth/db/" + collection + "/" + docID)
	if err != nil {
		return err
	}

	var doc map[string]interface{}
	if err := ParseResponse(resp, &doc); err != nil {
		return err
	}

	return utils.PrintJSON(os.Stdout, doc)
}

func runDBCreate(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)
	collection := args[0]
	jsonData := args[1]

	var doc map[string]interface{}
	if err := json.Unmarshal([]byte(jsonData), &doc); err != nil {
		return fmt.Errorf("invalid JSON: %w", err)
	}

	resp, err := client.Post("/auth/db/"+collection, doc)
	if err != nil {
		return err
	}

	var result map[string]interface{}
	if err := ParseResponse(resp, &result); err != nil {
		return err
	}

	if id, ok := result["id"].(string); ok {
		fmt.Printf("✅ Document created with ID: %s\n", id)
	} else {
		fmt.Println("✅ Document created")
	}

	return nil
}

func runDBUpdate(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)
	collection := args[0]
	docID := args[1]
	jsonData := args[2]

	var doc map[string]interface{}
	if err := json.Unmarshal([]byte(jsonData), &doc); err != nil {
		return fmt.Errorf("invalid JSON: %w", err)
	}

	resp, err := client.Put("/auth/db/"+collection+"/"+docID, doc)
	if err != nil {
		return err
	}

	if err := ParseResponse(resp, nil); err != nil {
		return err
	}

	fmt.Println("✅ Document updated")
	return nil
}

func runDBDelete(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)
	collection := args[0]
	docID := args[1]

	resp, err := client.Delete("/auth/db/"+collection+"/"+docID, nil)
	if err != nil {
		return err
	}

	if err := ParseResponse(resp, nil); err != nil {
		return err
	}

	fmt.Println("✅ Document deleted")
	return nil
}

func runDBCollections(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	resp, err := client.Get("/sys/db/collections")
	if err != nil {
		return err
	}

	var result map[string]interface{}
	if err := ParseResponse(resp, &result); err != nil {
		return err
	}

	return utils.PrintJSON(os.Stdout, result)
}

// docPreview returns a shortened preview of the document content
func docPreview(doc map[string]interface{}, maxLen int) string {
	// Copy and remove meta fields
	preview := make(map[string]interface{})
	for k, v := range doc {
		if !strings.HasPrefix(k, "_") && k != "id" {
			preview[k] = v
		}
	}

	data, _ := json.Marshal(preview)
	s := string(data)
	if len(s) > maxLen {
		return s[:maxLen] + "..."
	}
	return s
}
