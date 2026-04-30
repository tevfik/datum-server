// Package main — `datumctl notify …` ntfy-protocol notification commands.
package main

import (
	"bufio"
	"bytes"
	"crypto/tls"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"github.com/spf13/cobra"
)

var notifyCmd = &cobra.Command{
	Use:   "notify",
	Short: "Publish or subscribe to ntfy-compatible notification topics",
}

var (
	notifyTitle    string
	notifyPriority string
	notifyTags     string
	notifyMessage  string

	notifyPubCmd = &cobra.Command{
		Use:   "publish <topic>",
		Short: "Publish a message to a topic",
		Args:  cobra.ExactArgs(1),
		Example: `  datumctl notify publish alerts --message "Pump #2 offline" --priority high
  echo "raw text" | datumctl notify publish alerts`,
		RunE: runNotifyPublish,
	}
)

var (
	notifyFollow bool
	notifyFormat string
	notifySubCmd = &cobra.Command{
		Use:   "subscribe <topic>",
		Short: "Subscribe to a topic and stream messages to stdout",
		Args:  cobra.ExactArgs(1),
		RunE:  runNotifySubscribe,
	}
)

func init() {
	rootCmd.AddCommand(notifyCmd)
	notifyCmd.AddCommand(notifyPubCmd, notifySubCmd)

	notifyPubCmd.Flags().StringVar(&notifyMessage, "message", "", "Message body (defaults to stdin if omitted)")
	notifyPubCmd.Flags().StringVar(&notifyTitle, "title", "", "Notification title (Title header)")
	notifyPubCmd.Flags().StringVar(&notifyPriority, "priority", "", "Priority: min|low|default|high|max")
	notifyPubCmd.Flags().StringVar(&notifyTags, "tags", "", "Comma-separated tag list")

	notifySubCmd.Flags().BoolVarP(&notifyFollow, "follow", "f", true, "Stay connected and stream new messages")
	notifySubCmd.Flags().StringVar(&notifyFormat, "format", "json", "Stream format: json | sse | raw")
}

func runNotifyPublish(cmd *cobra.Command, args []string) error {
	loadConfig()
	topic := args[0]

	body := notifyMessage
	if body == "" {
		// Read from stdin (non-blocking-ish: only if data piped).
		buf, err := io.ReadAll(cmd.InOrStdin())
		if err == nil {
			body = strings.TrimRight(string(buf), "\n")
		}
	}

	url := serverURL + "/notify/" + topic
	req, err := http.NewRequest("POST", url, bytes.NewBufferString(body))
	if err != nil {
		return err
	}
	req.Header.Set("Content-Type", "text/plain")
	if notifyTitle != "" {
		req.Header.Set("Title", notifyTitle)
	}
	if notifyPriority != "" {
		req.Header.Set("Priority", notifyPriority)
	}
	if notifyTags != "" {
		req.Header.Set("Tags", notifyTags)
	}
	if token != "" {
		req.Header.Set("Authorization", "Bearer "+token)
	} else if apiKey != "" {
		req.Header.Set("Authorization", "Bearer "+apiKey)
	}

	client := &http.Client{
		Timeout:   15 * time.Second,
		Transport: &http.Transport{TLSClientConfig: &tls.Config{InsecureSkipVerify: insecure}},
	}
	resp, err := client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	respBody, _ := io.ReadAll(resp.Body)
	if resp.StatusCode >= 400 {
		return fmt.Errorf("publish failed (%d): %s", resp.StatusCode, string(respBody))
	}
	if outputJSON {
		fmt.Println(string(respBody))
		return nil
	}
	fmt.Printf("✓ published to /notify/%s (%d bytes)\n", topic, len(body))
	if len(respBody) > 0 {
		fmt.Println(string(respBody))
	}
	return nil
}

func runNotifySubscribe(cmd *cobra.Command, args []string) error {
	loadConfig()
	topic := args[0]

	suffix := "/json"
	switch strings.ToLower(notifyFormat) {
	case "sse":
		suffix = "/sse"
	case "raw":
		suffix = "/raw"
	case "json":
		suffix = "/json"
	default:
		return fmt.Errorf("unknown format %q (want json|sse|raw)", notifyFormat)
	}

	url := serverURL + "/notify/" + topic + suffix
	req, err := http.NewRequest("GET", url, nil)
	if err != nil {
		return err
	}
	if token != "" {
		req.Header.Set("Authorization", "Bearer "+token)
	} else if apiKey != "" {
		req.Header.Set("Authorization", "Bearer "+apiKey)
	}

	// No timeout for streaming subscriptions.
	client := &http.Client{
		Transport: &http.Transport{TLSClientConfig: &tls.Config{InsecureSkipVerify: insecure}},
	}
	resp, err := client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	if resp.StatusCode >= 400 {
		body, _ := io.ReadAll(resp.Body)
		return fmt.Errorf("subscribe failed (%d): %s", resp.StatusCode, string(body))
	}

	fmt.Fprintf(cmd.ErrOrStderr(), "📡 subscribed to /notify/%s%s — Ctrl+C to exit\n", topic, suffix)

	scanner := bufio.NewScanner(resp.Body)
	scanner.Buffer(make([]byte, 0, 64*1024), 1024*1024)
	for scanner.Scan() {
		fmt.Println(scanner.Text())
		if !notifyFollow {
			return nil
		}
	}
	return scanner.Err()
}
