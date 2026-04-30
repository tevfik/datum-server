// Package main — generic helpers used by lightweight subcommands that map
// 1:1 onto a single API call.
package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"os"
	"strings"

	"datum-go/internal/cli/utils"
)

// runSimpleHTTP performs a single API call and renders the response.
//
// The helper transparently handles plain-text endpoints (e.g. /sys/ip,
// /sys/metrics) by streaming the raw body when the server does not return
// JSON; this keeps the leaf cobra commands one-liners.
func runSimpleHTTP(method, path string, body interface{}) error {
	loadConfig()
	client := NewAPIClient(serverURL, token, apiKey)
	resp, err := client.Request(method, path, body)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return fmt.Errorf("read response: %w", err)
	}
	if resp.StatusCode >= 400 {
		var apiErr APIError
		if json.Unmarshal(raw, &apiErr) == nil {
			msg := apiErr.Error
			if msg == "" {
				msg = apiErr.Message
			}
			if msg != "" {
				return fmt.Errorf("API error (%d): %s", resp.StatusCode, msg)
			}
		}
		return fmt.Errorf("API error (%d): %s", resp.StatusCode, string(raw))
	}
	if len(raw) == 0 {
		fmt.Println("✓ OK")
		return nil
	}
	var v interface{}
	if err := json.Unmarshal(raw, &v); err != nil {
		// Not JSON — render raw text.
		fmt.Println(strings.TrimRight(string(raw), "\n"))
		return nil
	}
	if outputJSON {
		return utils.PrintJSON(os.Stdout, v)
	}
	out, _ := json.MarshalIndent(v, "", "  ")
	fmt.Println(string(out))
	return nil
}

// joinQuery appends k=v pairs (URL-encoded) to a path. Empty values are
// skipped.
func joinQuery(path string, kv map[string]string) string {
	q := url.Values{}
	for k, v := range kv {
		if v != "" {
			q.Set(k, v)
		}
	}
	if len(q) == 0 {
		return path
	}
	sep := "?"
	if strings.Contains(path, "?") {
		sep = "&"
	}
	return path + sep + q.Encode()
}
