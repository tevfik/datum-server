package main

import (
	"fmt"
	"os"

	"github.com/olekukonko/tablewriter"
	"github.com/spf13/cobra"

	"datum-go/internal/cli/utils"
)

var (
	mqttCmd = &cobra.Command{
		Use:   "mqtt",
		Short: "MQTT Broker Management",
		Long:  `Manage and debug the internal MQTT broker.`,
	}

	mqttStatsCmd = &cobra.Command{
		Use:   "stats",
		Short: "Get broker statistics",
		RunE:  runMQTTStats,
	}

	mqttClientsCmd = &cobra.Command{
		Use:   "clients",
		Short: "List connected clients",
		RunE:  runMQTTClients,
	}

	mqttPublishCmd = &cobra.Command{
		Use:   "publish",
		Short: "Publish a message to a topic",
		RunE:  runMQTTPublish,
	}
)

var (
	mqttTopic   string
	mqttMessage string
	mqttRetain  bool
)

func init() {
	rootCmd.AddCommand(mqttCmd)
	mqttCmd.AddCommand(mqttStatsCmd)
	mqttCmd.AddCommand(mqttClientsCmd)
	mqttCmd.AddCommand(mqttPublishCmd)

	mqttPublishCmd.Flags().StringVarP(&mqttTopic, "topic", "t", "", "Topic to publish to")
	mqttPublishCmd.Flags().StringVarP(&mqttMessage, "message", "m", "", "Message content")
	mqttPublishCmd.Flags().BoolVarP(&mqttRetain, "retain", "r", false, "Retain message")
	mqttPublishCmd.MarkFlagRequired("topic")
	mqttPublishCmd.MarkFlagRequired("message")
}

func runMQTTStats(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	resp, err := client.Get("/admin/mqtt/stats")
	if err != nil {
		return err
	}

	if outputJSON {
		var result interface{}
		ParseResponse(resp, &result)
		return utils.PrintJSON(os.Stdout, result)
	}

	var stats map[string]interface{}
	if err := ParseResponse(resp, &stats); err != nil {
		return err
	}

	fmt.Println("\n📊 Broker Statistics")
	fmt.Println("━━━━━━━━━━━━━━━━━━━━━━━━━")
	for k, v := range stats {
		fmt.Printf("%-20s: %v\n", k, v)
	}
	return nil
}

func runMQTTClients(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	resp, err := client.Get("/admin/mqtt/clients")
	if err != nil {
		return err
	}

	if outputJSON {
		var result interface{}
		ParseResponse(resp, &result)
		return utils.PrintJSON(os.Stdout, result)
	}

	var result struct {
		Clients []struct {
			ID        string `json:"id"`
			IP        string `json:"ip"`
			Connected bool   `json:"connected"`
		} `json:"clients"`
	}

	if err := ParseResponse(resp, &result); err != nil {
		return err
	}

	// Use Header instead of SetHeader to match local version/wrapper
	table := tablewriter.NewWriter(os.Stdout)
	table.Header([]string{"Client ID", "IP Address", "Status"})

	for _, c := range result.Clients {
		status := "🔴 Disconnected"
		if c.Connected {
			status = "🟢 Connected"
		}
		table.Append([]string{c.ID, c.IP, status})
	}
	table.Render()
	return nil
}

func runMQTTPublish(cmd *cobra.Command, args []string) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)

	payload := map[string]interface{}{
		"topic":   mqttTopic,
		"message": mqttMessage,
		"retain":  mqttRetain,
	}

	resp, err := client.Post("/admin/mqtt/publish", payload)
	if err != nil {
		return err
	}

	if outputJSON {
		var result interface{}
		ParseResponse(resp, &result)
		utils.PrintJSON(os.Stdout, result)
	} else {
		// Consume body to avoid leak if not outputting JSON
		ParseResponse(resp, nil)
		fmt.Println("✅ Message published successfully")
	}
	return nil
}
