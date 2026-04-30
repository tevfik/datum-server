package main

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"

	"datum-go/internal/cli/utils"
)

func runUpdateUser(cmd *cobra.Command, args []string) error {
	loadConfig()

	identifier := args[0] // Can be ID or Email

	// Check admin authentication
	if token == "" && apiKey == "" {
		return fmt.Errorf("admin authentication required. Please login first")
	}

	if adminRole == "" && adminStatus == "" {
		return fmt.Errorf("must specify at least one of --role or --status")
	}

	client := NewAPIClient(serverURL, token, apiKey)

	// If identifier looks like an email, try to resolve it to an ID first
	userID := identifier
	if utils.IdentifierContainsEmail(identifier) {
		// List users to find the ID
		resp, err := client.Get("/admin/users")
		if err != nil {
			return fmt.Errorf("failed to list users for resolution: %w", err)
		}

		var response map[string]interface{}
		if err := ParseResponse(resp, &response); err != nil {
			return err
		}

		found := false
		if users, ok := response["users"].([]interface{}); ok {
			for _, u := range users {
				user := u.(map[string]interface{})
				if utils.GetString(user, "email") == identifier {
					userID = utils.GetString(user, "id")
					found = true
					break
				}
			}
		}

		if !found {
			return fmt.Errorf("user not found with email: %s", identifier)
		}
	}

	// Prepare payload
	payload := map[string]interface{}{}
	if adminRole != "" {
		if adminRole != "admin" && adminRole != "user" {
			return fmt.Errorf("invalid role: %s (must be 'admin' or 'user')", adminRole)
		}
		payload["role"] = adminRole
	}
	if adminStatus != "" {
		if adminStatus != "active" && adminStatus != "suspended" {
			return fmt.Errorf("invalid status: %s (must be 'active' or 'suspended')", adminStatus)
		}
		payload["status"] = adminStatus
	}

	// Make API call
	resp, err := client.Put("/admin/users/"+userID, payload)
	if err != nil {
		return fmt.Errorf("failed to update user: %w", err)
	}

	var response map[string]interface{}
	if err := ParseResponse(resp, &response); err != nil {
		return err
	}

	if outputJSON {
		return utils.PrintJSON(os.Stdout, response)
	}

	fmt.Printf("✅ User updated successfully\n")
	if adminRole != "" {
		fmt.Printf("  New Role: %s\n", adminRole)
	}
	if adminStatus != "" {
		fmt.Printf("  New Status: %s\n", adminStatus)
	}

	return nil
}
