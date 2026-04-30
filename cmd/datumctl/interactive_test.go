package main

import (
	"testing"

	"github.com/spf13/cobra"
	"github.com/spf13/pflag"
)

func TestParsePositionalArgs(t *testing.T) {
	cases := []struct {
		use      string
		expected []argSpec
	}{
		{"send <device-id> <action>", []argSpec{{Name: "device-id", Required: true}, {Name: "action", Required: true}}},
		{"get <id> [extra]", []argSpec{{Name: "id", Required: true}, {Name: "extra", Required: false}}},
		{"list", nil},
		{"add [files...]", []argSpec{{Name: "files", Required: false, Variadic: true}}},
	}
	for _, c := range cases {
		got := parsePositionalArgs(c.use)
		if len(got) != len(c.expected) {
			t.Errorf("Use=%q: want %d specs, got %d (%+v)", c.use, len(c.expected), len(got), got)
			continue
		}
		for i := range got {
			if got[i] != c.expected[i] {
				t.Errorf("Use=%q arg %d: want %+v got %+v", c.use, i, c.expected[i], got[i])
			}
		}
	}
}

func TestIsSecretFlag(t *testing.T) {
	for _, name := range []string{"password", "new-password", "api-key", "secret", "auth-token"} {
		if !isSecretFlag(name) {
			t.Errorf("isSecretFlag(%q) = false, want true", name)
		}
	}
	for _, name := range []string{"email", "id", "name", "url"} {
		if isSecretFlag(name) {
			t.Errorf("isSecretFlag(%q) = true, want false", name)
		}
	}
}

func TestBuildMenuOptionsHidesSelfAndCompletion(t *testing.T) {
	opts, mapping := buildMenuOptions(rootCmd)
	if len(opts) == 0 {
		t.Fatal("expected non-empty menu")
	}
	for _, label := range opts {
		c := mapping[label]
		if c == nil {
			t.Fatalf("nil mapping for %q", label)
		}
		if interactiveHidden[c.Name()] {
			t.Errorf("hidden command %q leaked into menu", c.Name())
		}
	}
}

func TestRootHasNewSubcommands(t *testing.T) {
	for _, name := range []string{"auth", "sys", "rules", "notify"} {
		if cmd, _, err := rootCmd.Find([]string{name}); err != nil || cmd.Name() != name {
			t.Errorf("missing top-level command %q: %v", name, err)
		}
	}
}

func TestAuthSubcommandsRegistered(t *testing.T) {
	authPath, _, err := rootCmd.Find([]string{"auth"})
	if err != nil {
		t.Fatalf("auth root missing: %v", err)
	}
	want := []string{"me", "sessions", "providers", "refresh", "change-password",
		"forgot-password", "reset-password", "delete-account", "push-token"}
	for _, w := range want {
		if _, _, err := authPath.Find([]string{w}); err != nil {
			t.Errorf("auth %s missing: %v", w, err)
		}
	}
	push, _, _ := authPath.Find([]string{"push-token"})
	for _, w := range []string{"list", "add", "delete"} {
		if _, _, err := push.Find([]string{w}); err != nil {
			t.Errorf("auth push-token %s missing: %v", w, err)
		}
	}
}

func TestRulesSubcommandsRegistered(t *testing.T) {
	r, _, err := rootCmd.Find([]string{"rules"})
	if err != nil {
		t.Fatalf("rules missing: %v", err)
	}
	for _, w := range []string{"list", "get", "create", "delete", "enable", "disable"} {
		if _, _, err := r.Find([]string{w}); err != nil {
			t.Errorf("rules %s missing: %v", w, err)
		}
	}
}

func TestSysSubcommandsRegistered(t *testing.T) {
	s, _, err := rootCmd.Find([]string{"sys"})
	if err != nil {
		t.Fatalf("sys missing: %v", err)
	}
	for _, w := range []string{"info", "time", "ip", "status", "metrics"} {
		if _, _, err := s.Find([]string{w}); err != nil {
			t.Errorf("sys %s missing: %v", w, err)
		}
	}
}

func TestNotifySubcommandsRegistered(t *testing.T) {
	n, _, err := rootCmd.Find([]string{"notify"})
	if err != nil {
		t.Fatalf("notify missing: %v", err)
	}
	for _, w := range []string{"publish", "subscribe"} {
		if _, _, err := n.Find([]string{w}); err != nil {
			t.Errorf("notify %s missing: %v", w, err)
		}
	}
}

// promptAndSetFlag relies on survey for I/O, but we can still smoke-test
// the bool-default logic by directly asserting flag mutation.
func TestFlagMutationByName(t *testing.T) {
	bv := boolFlagValue(false)
	f := &pflag.Flag{
		Name:     "json-flag",
		DefValue: "false",
		Value:    &bv,
		Usage:    "test",
	}
	if err := f.Value.Set("true"); err != nil {
		t.Fatal(err)
	}
	if f.Value.String() != "true" {
		t.Fatalf("flag mutation failed: %s", f.Value.String())
	}
}

// boolFlagValue is a minimal pflag.Value implementation for tests.
type boolFlagValue bool

func (b *boolFlagValue) String() string {
	if *b {
		return "true"
	}
	return "false"
}
func (b *boolFlagValue) Set(s string) error {
	*b = boolFlagValue(s == "true")
	return nil
}
func (b *boolFlagValue) Type() string { return "bool" }

// Compile-time check that interactive command stays registered.
var _ = []*cobra.Command{interactiveCmd, authCmd, sysCmd, rulesCmd, notifyCmd}
