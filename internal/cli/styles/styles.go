// Package styles provides shared lipgloss styles and helpers for datumctl CLI output.
package styles

import (
	"fmt"
	"strings"

	"github.com/charmbracelet/lipgloss"
)

// Colors
var (
	Primary   = lipgloss.Color("#7C3AED") // Violet
	Secondary = lipgloss.Color("#06B6D4") // Cyan
	Success   = lipgloss.Color("#10B981") // Green
	Warning   = lipgloss.Color("#F59E0B") // Amber
	Danger    = lipgloss.Color("#EF4444") // Red
	Muted     = lipgloss.Color("#6B7280") // Gray
	White     = lipgloss.Color("#FAFAFA")
)

// Text styles
var (
	Bold        = lipgloss.NewStyle().Bold(true)
	Dim         = lipgloss.NewStyle().Foreground(Muted)
	Title       = lipgloss.NewStyle().Bold(true).Foreground(Primary)
	SubTitle    = lipgloss.NewStyle().Bold(true).Foreground(Secondary)
	SuccessText = lipgloss.NewStyle().Foreground(Success)
	WarningText = lipgloss.NewStyle().Foreground(Warning)
	DangerText  = lipgloss.NewStyle().Foreground(Danger)
	KeyStyle    = lipgloss.NewStyle().Foreground(Secondary).Bold(true)
	ValueStyle  = lipgloss.NewStyle().Foreground(White)
)

// Box styles
var (
	InfoBox = lipgloss.NewStyle().
		BorderStyle(lipgloss.RoundedBorder()).
		BorderForeground(Secondary).
		Padding(0, 1)

	SuccessBox = lipgloss.NewStyle().
			BorderStyle(lipgloss.RoundedBorder()).
			BorderForeground(Success).
			Padding(0, 1)

	WarningBox = lipgloss.NewStyle().
			BorderStyle(lipgloss.RoundedBorder()).
			BorderForeground(Warning).
			Padding(0, 1)

	DangerBox = lipgloss.NewStyle().
			BorderStyle(lipgloss.RoundedBorder()).
			BorderForeground(Danger).
			Padding(0, 1)

	Banner = lipgloss.NewStyle().
		Bold(true).
		Foreground(White).
		Background(Primary).
		Padding(0, 1).
		MarginBottom(1)
)

// KV renders a key-value line with styled key.
func KV(key, value string) string {
	return fmt.Sprintf("  %s  %s", KeyStyle.Render(key+":"), value)
}

// KVPadded renders a key-value line with right-padded key for alignment.
func KVPadded(key string, padTo int, value string) string {
	padded := key + ":" + strings.Repeat(" ", max(1, padTo-len(key)))
	return fmt.Sprintf("  %s%s", KeyStyle.Render(padded), value)
}

// Header renders a section header.
func Header(text string) string {
	return "\n" + Title.Render(text) + "\n"
}

// StatusIcon returns a colored status emoji.
func StatusIcon(status string) string {
	switch strings.ToLower(status) {
	case "active", "success", "completed", "online":
		return SuccessText.Render("●")
	case "pending", "waiting":
		return WarningText.Render("●")
	case "error", "failed", "suspended", "offline":
		return DangerText.Render("●")
	default:
		return Dim.Render("●")
	}
}

func max(a, b int) int {
	if a > b {
		return a
	}
	return b
}
