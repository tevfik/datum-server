// Bucket subcommand for datumctl. Provides CRUD over the /storage REST API.
package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"path/filepath"

	"github.com/spf13/cobra"
)

var (
	bucketContentType   string
	bucketPresignMethod string
	bucketPresignSecs   int
)

var bucketCmd = &cobra.Command{
	Use:   "bucket",
	Short: "Manage object storage buckets",
	Long:  "Create, list, upload, download, delete, and presign objects in datum-server buckets.",
}

var bucketListCmd = &cobra.Command{
	Use:   "list [bucket]",
	Short: "List buckets, or list objects in a bucket",
	Args:  cobra.MaximumNArgs(1),
	RunE:  runBucketList,
}

var bucketCreateCmd = &cobra.Command{
	Use:   "create <bucket>",
	Short: "Create (or ensure) a bucket",
	Args:  cobra.ExactArgs(1),
	RunE:  runBucketCreate,
}

var bucketDeleteCmd = &cobra.Command{
	Use:   "delete <bucket> [path]",
	Short: "Delete a bucket (must be empty) or a single object",
	Args:  cobra.RangeArgs(1, 2),
	RunE:  runBucketDelete,
}

var bucketPutCmd = &cobra.Command{
	Use:   "put <bucket> <path> <local-file>",
	Short: "Upload a file to a bucket path",
	Args:  cobra.ExactArgs(3),
	RunE:  runBucketPut,
}

var bucketGetCmd = &cobra.Command{
	Use:   "get <bucket> <path> [output-file]",
	Short: "Download an object (writes to stdout when output-file is omitted)",
	Args:  cobra.RangeArgs(2, 3),
	RunE:  runBucketGet,
}

var bucketPresignCmd = &cobra.Command{
	Use:   "presign <bucket> <path>",
	Short: "Generate a presigned URL for an object",
	Args:  cobra.ExactArgs(2),
	RunE:  runBucketPresign,
}

func init() {
	rootCmd.AddCommand(bucketCmd)
	bucketCmd.AddCommand(bucketListCmd, bucketCreateCmd, bucketDeleteCmd,
		bucketPutCmd, bucketGetCmd, bucketPresignCmd)

	bucketPutCmd.Flags().StringVar(&bucketContentType, "content-type", "", "Content-Type header (auto-detected when empty)")
	bucketPresignCmd.Flags().StringVar(&bucketPresignMethod, "method", "GET", "HTTP method for the presigned URL")
	bucketPresignCmd.Flags().IntVar(&bucketPresignSecs, "expires", 900, "URL validity in seconds")
}

func runBucketList(cmd *cobra.Command, args []string) error {
	loadConfig()
	c := NewAPIClient(serverURL, token, apiKey)
	path := "/storage"
	if len(args) == 1 {
		path = "/storage/" + url.PathEscape(args[0])
	}
	resp, err := c.Request("GET", path, nil)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	return printJSONResponse(resp)
}

func runBucketCreate(cmd *cobra.Command, args []string) error {
	loadConfig()
	c := NewAPIClient(serverURL, token, apiKey)
	resp, err := c.Request("POST", "/storage/"+url.PathEscape(args[0]), nil)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	return printJSONResponse(resp)
}

func runBucketDelete(cmd *cobra.Command, args []string) error {
	loadConfig()
	c := NewAPIClient(serverURL, token, apiKey)
	path := "/storage/" + url.PathEscape(args[0])
	if len(args) == 2 {
		path += "/" + escapeObjectPath(args[1])
	}
	resp, err := c.Request("DELETE", path, nil)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	if resp.StatusCode >= 400 {
		return printJSONResponse(resp)
	}
	fmt.Println("deleted")
	return nil
}

func runBucketPut(cmd *cobra.Command, args []string) error {
	loadConfig()
	bucket, objPath, localFile := args[0], args[1], args[2]
	f, err := os.Open(localFile)
	if err != nil {
		return err
	}
	defer f.Close()

	c := NewAPIClient(serverURL, token, apiKey)
	fullURL := c.BaseURL + "/storage/" + url.PathEscape(bucket) + "/" + escapeObjectPath(objPath)
	req, err := http.NewRequest("PUT", fullURL, f)
	if err != nil {
		return err
	}
	if bucketContentType != "" {
		req.Header.Set("Content-Type", bucketContentType)
	}
	if c.Token != "" {
		req.Header.Set("Authorization", "Bearer "+c.Token)
	} else if c.APIKey != "" {
		req.Header.Set("Authorization", "Bearer "+c.APIKey)
	}
	resp, err := c.HTTPClient.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	return printJSONResponse(resp)
}

func runBucketGet(cmd *cobra.Command, args []string) error {
	loadConfig()
	bucket, objPath := args[0], args[1]
	c := NewAPIClient(serverURL, token, apiKey)
	resp, err := c.Request("GET", "/storage/"+url.PathEscape(bucket)+"/"+escapeObjectPath(objPath), nil)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	if resp.StatusCode >= 400 {
		return printJSONResponse(resp)
	}
	out := io.Writer(os.Stdout)
	if len(args) == 3 {
		f, err := os.Create(args[2])
		if err != nil {
			return err
		}
		defer f.Close()
		out = f
	}
	_, err = io.Copy(out, resp.Body)
	return err
}

func runBucketPresign(cmd *cobra.Command, args []string) error {
	loadConfig()
	c := NewAPIClient(serverURL, token, apiKey)
	body := map[string]any{
		"path":         args[1],
		"method":       bucketPresignMethod,
		"expires_secs": bucketPresignSecs,
	}
	resp, err := c.Request("POST", "/storage/"+url.PathEscape(args[0])+"/presign", body)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	return printJSONResponse(resp)
}

// escapeObjectPath encodes each path segment but preserves '/' separators.
func escapeObjectPath(p string) string {
	parts := splitPath(p)
	for i, s := range parts {
		parts[i] = url.PathEscape(s)
	}
	return joinPath(parts)
}

func splitPath(p string) []string {
	p = filepath.ToSlash(p)
	var out []string
	cur := ""
	for _, r := range p {
		if r == '/' {
			if cur != "" {
				out = append(out, cur)
			}
			cur = ""
			continue
		}
		cur += string(r)
	}
	if cur != "" {
		out = append(out, cur)
	}
	return out
}

func joinPath(parts []string) string {
	res := ""
	for i, s := range parts {
		if i > 0 {
			res += "/"
		}
		res += s
	}
	return res
}

// printJSONResponse renders the response body either as raw JSON when
// --json is set, or as pretty-printed JSON otherwise.
func printJSONResponse(resp *http.Response) error {
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return err
	}
	if outputJSON || len(body) == 0 {
		_, _ = os.Stdout.Write(body)
		if len(body) > 0 && body[len(body)-1] != '\n' {
			fmt.Println()
		}
		if resp.StatusCode >= 400 {
			return fmt.Errorf("HTTP %d", resp.StatusCode)
		}
		return nil
	}
	var pretty any
	if err := json.Unmarshal(body, &pretty); err == nil {
		enc := json.NewEncoder(os.Stdout)
		enc.SetIndent("", "  ")
		_ = enc.Encode(pretty)
	} else {
		_, _ = os.Stdout.Write(body)
		fmt.Println()
	}
	if resp.StatusCode >= 400 {
		return fmt.Errorf("HTTP %d", resp.StatusCode)
	}
	return nil
}
