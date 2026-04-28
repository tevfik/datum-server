// Package main — `datumctl sys …` covering the public `/sys/*` endpoints.
// `datumctl status` and `datumctl setup` already exist as top-level
// shortcuts; this group exposes the remaining read-only inspection
// endpoints and the metrics endpoint.
package main

import (
	"fmt"
	"io"

	"github.com/spf13/cobra"
)

var sysCmd = &cobra.Command{
	Use:   "sys",
	Short: "Inspect server system endpoints (info, time, ip, status, metrics)",
}

var sysInfoCmd = &cobra.Command{
	Use:   "info",
	Short: "Public server information",
	RunE:  func(cmd *cobra.Command, args []string) error { return runSimpleHTTP("GET", "/sys/info", nil) },
}

var sysTimeCmd = &cobra.Command{
	Use:   "time",
	Short: "Server clock",
	RunE:  func(cmd *cobra.Command, args []string) error { return runSimpleHTTP("GET", "/sys/time", nil) },
}

var sysIPCmd = &cobra.Command{
	Use:   "ip",
	Short: "Show the public IP the server sees for this client",
	RunE:  func(cmd *cobra.Command, args []string) error { return runSimpleHTTP("GET", "/sys/ip", nil) },
}

var sysStatusCmd = &cobra.Command{
	Use:   "status",
	Short: "Initialization status (initialized, allow_register, …)",
	RunE:  func(cmd *cobra.Command, args []string) error { return runSimpleHTTP("GET", "/sys/status", nil) },
}

var sysMetricsCmd = &cobra.Command{
	Use:   "metrics",
	Short: "Prometheus-style metrics (raw text output)",
	RunE: func(cmd *cobra.Command, args []string) error {
		loadConfig()
		client := NewAPIClient(serverURL, token, apiKey)
		resp, err := client.Get("/sys/metrics")
		if err != nil {
			return err
		}
		defer resp.Body.Close()
		body, _ := io.ReadAll(resp.Body)
		if resp.StatusCode >= 400 {
			return fmt.Errorf("metrics returned %d: %s", resp.StatusCode, string(body))
		}
		fmt.Print(string(body))
		return nil
	},
}

func init() {
	rootCmd.AddCommand(sysCmd)
	sysCmd.AddCommand(sysInfoCmd, sysTimeCmd, sysIPCmd, sysStatusCmd, sysMetricsCmd)
}
