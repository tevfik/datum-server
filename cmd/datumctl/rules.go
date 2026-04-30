// Package main — `datumctl rules …` admin rule-engine commands.
package main

import (
	"encoding/json"
	"fmt"
	"os"

	"github.com/spf13/cobra"
)

var rulesCmd = &cobra.Command{
	Use:   "rules",
	Short: "Manage server-side automation rules (admin)",
}

var rulesListCmd = &cobra.Command{
	Use:   "list",
	Short: "List all rules",
	RunE:  func(cmd *cobra.Command, args []string) error { return runSimpleHTTP("GET", "/admin/rules", nil) },
}

var rulesGetCmd = &cobra.Command{
	Use:   "get <rule-id>",
	Short: "Show a rule by ID",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		return runSimpleHTTP("GET", "/admin/rules/"+args[0], nil)
	},
}

var (
	ruleFile       string
	ruleJSON       string
	rulesCreateCmd = &cobra.Command{
		Use:   "create",
		Short: "Create a new rule from a JSON file or inline JSON",
		RunE: func(cmd *cobra.Command, args []string) error {
			raw := []byte(ruleJSON)
			if ruleFile != "" {
				b, err := os.ReadFile(ruleFile)
				if err != nil {
					return fmt.Errorf("read %s: %w", ruleFile, err)
				}
				raw = b
			}
			if len(raw) == 0 {
				return fmt.Errorf("provide --file or --json")
			}
			var body interface{}
			if err := json.Unmarshal(raw, &body); err != nil {
				return fmt.Errorf("invalid JSON: %w", err)
			}
			return runSimpleHTTP("POST", "/admin/rules", body)
		},
	}
)

var rulesDeleteCmd = &cobra.Command{
	Use:   "delete <rule-id>",
	Short: "Delete a rule",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		return runSimpleHTTP("DELETE", "/admin/rules/"+args[0], nil)
	},
}

var rulesEnableCmd = &cobra.Command{
	Use:   "enable <rule-id>",
	Short: "Enable a rule",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		return runSimpleHTTP("PUT", "/admin/rules/"+args[0]+"/enable", nil)
	},
}

var rulesDisableCmd = &cobra.Command{
	Use:   "disable <rule-id>",
	Short: "Disable a rule",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		return runSimpleHTTP("PUT", "/admin/rules/"+args[0]+"/disable", nil)
	},
}

func init() {
	rootCmd.AddCommand(rulesCmd)
	rulesCmd.AddCommand(rulesListCmd, rulesGetCmd, rulesCreateCmd, rulesDeleteCmd, rulesEnableCmd, rulesDisableCmd)
	rulesCreateCmd.Flags().StringVar(&ruleFile, "file", "", "Path to a JSON file describing the rule")
	rulesCreateCmd.Flags().StringVar(&ruleJSON, "json", "", "Inline JSON describing the rule")
}
