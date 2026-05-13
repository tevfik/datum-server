// Package main — interactive menu.
//
// The interactive menu is auto-generated from the cobra command tree so it
// always stays in lock-step with the regular CLI surface: every new
// subcommand or flag automatically becomes available in the menu without
// any duplicated wiring.
package main

import (
	"fmt"
	"regexp"
	"sort"
	"strconv"
	"strings"

	"github.com/AlecAivazis/survey/v2"
	"github.com/spf13/cobra"
	"github.com/spf13/pflag"

	"datum-go/internal/cli/styles"
)

var interactiveCmd = &cobra.Command{
	Use:     "interactive",
	Aliases: []string{"i", "menu"},
	Short:   "Interactive menu (auto-derived from the command tree)",
	Long: `Launch a guided menu that walks the full datumctl command tree.

The menu is generated dynamically from the registered cobra commands, so it
always reflects the complete CLI surface with no duplicated wiring.

Each leaf command prompts for its positional arguments and lets you opt-in
to any flags before executing.`,
	RunE: runInteractive,
}

func init() { rootCmd.AddCommand(interactiveCmd) }

// interactiveHidden lists commands that should not appear in the menu (the
// menu itself, completion helper, etc.).
var interactiveHidden = map[string]bool{
	"interactive": true,
	"completion":  true,
	"help":        true,
}

func runInteractive(cmd *cobra.Command, args []string) error {
	loadConfig()

	fmt.Println()
	fmt.Println(styles.Banner.Render(" 🎯 Datum IoT Platform — Interactive Mode "))
	if token == "" && apiKey == "" {
		fmt.Println("\n⚠️  Not authenticated. Use the “login” or “setup” entry below.")
	} else {
		fmt.Printf("\n✅ Connected to: %s\n", serverURL)
	}

	return commandLoop(rootCmd)
}

// commandLoop renders an interactive menu of subcommands of `parent`. It
// keeps prompting until the user picks Back / Exit.
func commandLoop(parent *cobra.Command) error {
	for {
		opts, mapping := buildMenuOptions(parent)
		if len(opts) == 0 {
			return nil
		}

		nav := "← Back"
		if parent == rootCmd {
			nav = "🚪 Exit"
		}
		opts = append(opts, nav)

		title := parent.CommandPath() + " — choose action"
		if parent == rootCmd {
			title = "Main menu"
		}

		var choice string
		if err := survey.AskOne(&survey.Select{
			Message:  title,
			Options:  opts,
			PageSize: 20,
		}, &choice); err != nil {
			return err
		}

		if choice == nav {
			if parent == rootCmd {
				fmt.Println("\n👋 Goodbye!")
			}
			return nil
		}

		sel := mapping[choice]
		if sel == nil {
			continue
		}

		var err error
		if sel.HasAvailableSubCommands() {
			err = commandLoop(sel)
		} else {
			err = runCommandInteractive(sel)
		}
		if err != nil {
			fmt.Printf("\n❌ %v\n", err)
		}
		fmt.Println()
	}
}

// buildMenuOptions returns the labelled options for parent's visible
// subcommands plus a label→command map. Folders (commands with children)
// are marked with 📁; leaves with ▶.
func buildMenuOptions(parent *cobra.Command) ([]string, map[string]*cobra.Command) {
	mapping := map[string]*cobra.Command{}
	subs := make([]*cobra.Command, 0, len(parent.Commands()))
	for _, c := range parent.Commands() {
		if c.Hidden || interactiveHidden[c.Name()] || !c.IsAvailableCommand() {
			continue
		}
		subs = append(subs, c)
	}
	sort.Slice(subs, func(i, j int) bool {
		// Folders first, then alphabetical.
		fi := subs[i].HasAvailableSubCommands()
		fj := subs[j].HasAvailableSubCommands()
		if fi != fj {
			return fi
		}
		return subs[i].Name() < subs[j].Name()
	})
	opts := make([]string, 0, len(subs))
	for _, c := range subs {
		marker := "▶"
		if c.HasAvailableSubCommands() {
			marker = "📁"
		}
		short := c.Short
		if short == "" {
			short = "(no description)"
		}
		label := fmt.Sprintf("%s %-22s — %s", marker, c.Name(), short)
		opts = append(opts, label)
		mapping[label] = c
	}
	return opts, mapping
}

// argRe matches both <required> and [optional] placeholders in a Use string.
var argRe = regexp.MustCompile(`<([^>]+)>|\[([^\]]+)\]`)

type argSpec struct {
	Name     string
	Required bool
	Variadic bool
}

func parsePositionalArgs(use string) []argSpec {
	matches := argRe.FindAllStringSubmatch(use, -1)
	specs := make([]argSpec, 0, len(matches))
	for _, m := range matches {
		name := m[1]
		req := name != ""
		if !req {
			name = m[2]
		}
		variadic := strings.HasSuffix(name, "...")
		name = strings.TrimSuffix(name, "...")
		specs = append(specs, argSpec{Name: name, Required: req, Variadic: variadic})
	}
	return specs
}

// runCommandInteractive prompts for positional args + selected flags then
// dispatches to the leaf cobra command.
func runCommandInteractive(cmd *cobra.Command) error {
	fmt.Printf("\n📌 %s\n", cmd.CommandPath())
	if d := strings.TrimSpace(cmd.Long); d != "" {
		fmt.Println(d)
	} else if cmd.Short != "" {
		fmt.Println(cmd.Short)
	}

	// Reset every defined local flag back to its declared default so values
	// from a previous invocation in the same menu session do not leak.
	cmd.Flags().VisitAll(func(f *pflag.Flag) {
		_ = f.Value.Set(f.DefValue)
		f.Changed = false
	})

	// Positional args.
	specs := parsePositionalArgs(cmd.Use)
	var posArgs []string
	for _, s := range specs {
		v, err := promptPositional(s)
		if err != nil {
			return err
		}
		if v == "" && !s.Required {
			break
		}
		if s.Variadic {
			posArgs = append(posArgs, strings.Fields(v)...)
			continue
		}
		posArgs = append(posArgs, v)
	}

	// Flags — multi-select then prompt for each picked.
	var flags []*pflag.Flag
	cmd.Flags().VisitAll(func(f *pflag.Flag) {
		if !f.Hidden {
			flags = append(flags, f)
		}
	})
	if len(flags) > 0 {
		labels := make([]string, 0, len(flags))
		labelToFlag := map[string]*pflag.Flag{}
		for _, f := range flags {
			label := fmt.Sprintf("--%s  (%s, default %q)  %s", f.Name, f.Value.Type(), f.DefValue, f.Usage)
			labels = append(labels, label)
			labelToFlag[label] = f
		}
		var picked []string
		if err := survey.AskOne(&survey.MultiSelect{
			Message:  "Set any flags? (space to toggle, enter to continue):",
			Options:  labels,
			PageSize: 12,
		}, &picked); err != nil {
			return err
		}
		for _, lbl := range picked {
			if err := promptAndSetFlag(labelToFlag[lbl]); err != nil {
				return err
			}
		}
	}

	// Validate against cmd.Args (e.g. ExactArgs(2)).
	if cmd.Args != nil {
		if err := cmd.Args(cmd, posArgs); err != nil {
			return fmt.Errorf("argument validation failed: %w", err)
		}
	}

	// Confirm + show invocation.
	invocation := cmd.CommandPath()
	if len(posArgs) > 0 {
		invocation += " " + strings.Join(posArgs, " ")
	}
	cmd.Flags().Visit(func(f *pflag.Flag) {
		invocation += fmt.Sprintf(" --%s=%s", f.Name, f.Value.String())
	})
	confirm := true
	if err := survey.AskOne(&survey.Confirm{
		Message: fmt.Sprintf("Run: %s ?", invocation),
		Default: true,
	}, &confirm); err != nil {
		return err
	}
	if !confirm {
		return nil
	}
	fmt.Printf("\n> %s\n\n", invocation)

	switch {
	case cmd.RunE != nil:
		return cmd.RunE(cmd, posArgs)
	case cmd.Run != nil:
		cmd.Run(cmd, posArgs)
		return nil
	default:
		return fmt.Errorf("command %s has no executable handler", cmd.Name())
	}
}

func promptPositional(s argSpec) (string, error) {
	tail := " (optional, blank to skip)"
	if s.Required {
		tail = " (required)"
	}
	if s.Variadic {
		tail += " (space-separated)"
	}
	q := &survey.Input{Message: s.Name + tail + ":"}
	var v string
	var err error
	if s.Required {
		err = survey.AskOne(q, &v, survey.WithValidator(survey.Required))
	} else {
		err = survey.AskOne(q, &v)
	}
	return strings.TrimSpace(v), err
}

func promptAndSetFlag(f *pflag.Flag) error {
	msg := fmt.Sprintf("--%s (%s) — %s", f.Name, f.Value.Type(), f.Usage)
	switch f.Value.Type() {
	case "bool":
		def, _ := strconv.ParseBool(f.DefValue)
		var b bool
		if err := survey.AskOne(&survey.Confirm{Message: msg, Default: def}, &b); err != nil {
			return err
		}
		return f.Value.Set(strconv.FormatBool(b))
	case "stringSlice", "stringArray":
		var s string
		def := strings.TrimSuffix(strings.TrimPrefix(f.DefValue, "["), "]")
		if err := survey.AskOne(&survey.Input{Message: msg + " (comma-separated)", Default: def}, &s); err != nil {
			return err
		}
		if s == "" {
			return nil
		}
		// Reset slice/array before appending fresh entries.
		_ = f.Value.Set("")
		f.Changed = false
		for _, p := range strings.Split(s, ",") {
			p = strings.TrimSpace(p)
			if p == "" {
				continue
			}
			if err := f.Value.Set(p); err != nil {
				return err
			}
		}
		return nil
	default:
		// Heuristic: hide secrets behind a Password prompt.
		if isSecretFlag(f.Name) {
			var s string
			if err := survey.AskOne(&survey.Password{Message: msg}, &s); err != nil {
				return err
			}
			if s == "" {
				return nil
			}
			return f.Value.Set(s)
		}
		var s string
		if err := survey.AskOne(&survey.Input{Message: msg, Default: f.DefValue}, &s); err != nil {
			return err
		}
		if s == "" || s == f.DefValue {
			return nil
		}
		return f.Value.Set(s)
	}
}

func isSecretFlag(name string) bool {
	n := strings.ToLower(name)
	return strings.Contains(n, "password") || strings.Contains(n, "secret") ||
		strings.Contains(n, "token") || strings.Contains(n, "api-key")
}
