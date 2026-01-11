package main

import (
	"encoding/json"
	"fmt"
	"io"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/gorilla/websocket"
	"github.com/spf13/cobra"
)

var (
	followLogs bool
	logLevel   string
	logLines   int
)

func init() {
	rootCmd.AddCommand(logsCmd)
	logsCmd.Flags().BoolVarP(&followLogs, "follow", "f", false, "Stream logs in real-time")
	logsCmd.Flags().StringVarP(&logLevel, "level", "l", "", "Filter by log level (debug, info, warn, error)")
	logsCmd.Flags().IntVarP(&logLines, "lines", "n", 100, "Number of recent log lines to show")
}

var logsCmd = &cobra.Command{
	Use:   "logs",
	Short: "View and stream server logs",
	Long: `View server logs or stream them in real-time.

Examples:
  # View recent logs
  datumctl logs

  # Stream logs in real-time (like tail -f)
  datumctl logs -f

  # Filter by level
  datumctl logs --level error

  # Show last 50 lines
  datumctl logs -n 50`,
	Run: func(cmd *cobra.Command, args []string) {
		if followLogs {
			streamLogs()
		} else {
			getLogs()
		}
	},
}

func getLogs() {
	client := NewAPIClient(serverURL, token, apiKey)

	endpoint := "/admin/logs"
	if logLevel != "" {
		endpoint += "?level=" + logLevel
	}

	resp, err := client.Get(endpoint)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}
	defer resp.Body.Close()

	body, _ := io.ReadAll(resp.Body)

	if outputJSON {
		fmt.Println(string(body))
		return
	}

	var result struct {
		Logs  []struct{ Raw string } `json:"logs"`
		Total int                    `json:"total"`
	}

	if err := json.Unmarshal(body, &result); err != nil {
		fmt.Fprintf(os.Stderr, "Error parsing response: %v\n", err)
		os.Exit(1)
	}

	if len(result.Logs) == 0 {
		fmt.Println("No logs found.")
		return
	}

	// Show last N lines
	start := 0
	if len(result.Logs) > logLines {
		start = len(result.Logs) - logLines
	}

	for _, log := range result.Logs[start:] {
		fmt.Println(log.Raw)
	}

	fmt.Printf("\n--- Showing %d of %d logs ---\n", len(result.Logs)-start, result.Total)
}

func streamLogs() {
	// Parse the server URL for WebSocket connection
	wsURL := strings.Replace(serverURL, "http://", "ws://", 1)
	wsURL = strings.Replace(wsURL, "https://", "wss://", 1)
	wsURL = wsURL + "/admin/logs/stream"

	if verbose {
		fmt.Printf("Connecting to %s\n", wsURL)
	}

	// Set up WebSocket connection with auth header
	header := make(map[string][]string)
	if token != "" {
		header["Authorization"] = []string{"Bearer " + token}
	} else if apiKey != "" {
		header["Authorization"] = []string{"Bearer " + apiKey}
	}

	conn, _, err := websocket.DefaultDialer.Dial(wsURL, header)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to connect: %v\n", err)
		fmt.Fprintf(os.Stderr, "Make sure you are authenticated and the server is running.\n")
		os.Exit(1)
	}
	defer conn.Close()

	fmt.Println("Connected. Streaming logs... (Ctrl+C to exit)")
	fmt.Println("---")

	// Handle graceful shutdown
	interrupt := make(chan os.Signal, 1)
	signal.Notify(interrupt, os.Interrupt, syscall.SIGTERM)

	done := make(chan struct{})

	// Read messages
	go func() {
		defer close(done)
		for {
			_, message, err := conn.ReadMessage()
			if err != nil {
				if verbose {
					fmt.Fprintf(os.Stderr, "\nConnection closed: %v\n", err)
				}
				return
			}

			// Try to parse as JSON for pretty printing
			var logEntry map[string]interface{}
			if err := json.Unmarshal(message, &logEntry); err == nil {
				// Check if this is a connection message
				if msgType, ok := logEntry["type"].(string); ok && msgType == "connected" {
					continue // Skip connection acknowledgment
				}

				// Filter by level if specified
				if logLevel != "" {
					if level, ok := logEntry["level"].(string); ok {
						if !strings.EqualFold(level, logLevel) {
							continue
						}
					}
				}
			}

			fmt.Println(string(message))
		}
	}()

	// Wait for interrupt
	select {
	case <-done:
		fmt.Println("\nConnection closed by server.")
	case <-interrupt:
		fmt.Println("\nDisconnecting...")

		// Send close message
		err := conn.WriteMessage(websocket.CloseMessage,
			websocket.FormatCloseMessage(websocket.CloseNormalClosure, ""))
		if err != nil {
			return
		}

		// Wait briefly for clean close
		select {
		case <-done:
		case <-time.After(time.Second):
		}
	}
}
