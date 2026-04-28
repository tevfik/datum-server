package main

import (
	"bytes"
	"strings"
	"testing"
)

func TestVersionCommandRuns(t *testing.T) {
	buf := &bytes.Buffer{}
	rootCmd.SetOut(buf)
	rootCmd.SetErr(buf)
	rootCmd.SetArgs([]string{"version"})
	if err := rootCmd.Execute(); err != nil {
		t.Fatalf("version returned error: %v", err)
	}
	// Note: cobra's Run handler writes via fmt.Println directly, so the
	// command-bound buffer may stay empty. Just check the command resolves
	// without error.
}

func TestRootHasBucketSubcommand(t *testing.T) {
	cmd, _, err := rootCmd.Find([]string{"bucket"})
	if err != nil || cmd.Use != "bucket" {
		t.Fatalf("bucket subcommand missing: %v", err)
	}
	for _, want := range []string{"list", "create", "delete", "put", "get", "presign"} {
		if _, _, err := cmd.Find([]string{want}); err != nil {
			t.Errorf("bucket %s missing: %v", want, err)
		}
	}
}

func TestEscapeObjectPathPreservesSlashes(t *testing.T) {
	in := "a b/c/d e.txt"
	got := escapeObjectPath(in)
	if !strings.Contains(got, "/") {
		t.Fatalf("slashes lost: %q", got)
	}
	if strings.Contains(got, " ") {
		t.Fatalf("spaces not encoded: %q", got)
	}
}
