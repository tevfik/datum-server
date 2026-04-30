// Package main — `datumctl auth …` subcommands covering the parts of the
// /auth/* surface that the legacy top-level commands (login, logout,
// register) do not address: identity, sessions, password operations,
// push tokens and OAuth providers.
package main

import (
	"fmt"

	"github.com/spf13/cobra"
)

var authCmd = &cobra.Command{
	Use:   "auth",
	Short: "Authentication helpers (sessions, password, push tokens, OAuth)",
}

var authMeCmd = &cobra.Command{
	Use:   "me",
	Short: "Show the currently authenticated user",
	RunE:  func(cmd *cobra.Command, args []string) error { return runSimpleHTTP("GET", "/auth/me", nil) },
}

var authSessionsCmd = &cobra.Command{
	Use:   "sessions",
	Short: "List active sessions for the current user",
	RunE:  func(cmd *cobra.Command, args []string) error { return runSimpleHTTP("GET", "/auth/sessions", nil) },
}

var authProvidersCmd = &cobra.Command{
	Use:   "providers",
	Short: "List configured OAuth providers (public)",
	RunE:  func(cmd *cobra.Command, args []string) error { return runSimpleHTTP("GET", "/auth/providers", nil) },
}

var (
	authRefreshTokenFlag string
	authRefreshCmd       = &cobra.Command{
		Use:   "refresh",
		Short: "Exchange a refresh token for a new access token",
		RunE: func(cmd *cobra.Command, args []string) error {
			body := map[string]string{"refresh_token": authRefreshTokenFlag}
			return runSimpleHTTP("POST", "/auth/refresh", body)
		},
	}
)

var (
	authChgOldPassword string
	authChgNewPassword string
	authPasswordCmd    = &cobra.Command{
		Use:   "change-password",
		Short: "Change the password of the currently authenticated user",
		RunE: func(cmd *cobra.Command, args []string) error {
			body := map[string]string{
				"old_password": authChgOldPassword,
				"new_password": authChgNewPassword,
			}
			return runSimpleHTTP("PUT", "/auth/password", body)
		},
	}
)

var (
	authForgotEmail string
	authForgotCmd   = &cobra.Command{
		Use:   "forgot-password",
		Short: "Trigger a password-reset email",
		RunE: func(cmd *cobra.Command, args []string) error {
			body := map[string]string{"email": authForgotEmail}
			return runSimpleHTTP("POST", "/auth/forgot-password", body)
		},
	}
)

var (
	authResetToken    string
	authResetPassword string
	authResetCmd      = &cobra.Command{
		Use:   "reset-password",
		Short: "Complete a password reset using the token from the email",
		RunE: func(cmd *cobra.Command, args []string) error {
			body := map[string]string{
				"token":        authResetToken,
				"new_password": authResetPassword,
			}
			return runSimpleHTTP("POST", "/auth/reset-password", body)
		},
	}
)

var authDeleteUserCmd = &cobra.Command{
	Use:   "delete-account",
	Short: "Delete the currently authenticated user (irreversible)",
	RunE: func(cmd *cobra.Command, args []string) error {
		fmt.Println("⚠️  This permanently deletes your account.")
		return runSimpleHTTP("DELETE", "/auth/user", nil)
	},
}

// Push tokens.
var authPushTokenCmd = &cobra.Command{
	Use:   "push-token",
	Short: "Manage device push notification tokens",
}

var authPushListCmd = &cobra.Command{
	Use:   "list",
	Short: "List push tokens registered for the current user",
	RunE:  func(cmd *cobra.Command, args []string) error { return runSimpleHTTP("GET", "/auth/push-tokens", nil) },
}

var (
	authPushPlatform string
	authPushToken    string
	authPushAddCmd   = &cobra.Command{
		Use:   "add",
		Short: "Register a new push token (platform = fcm|apns|ntfy)",
		RunE: func(cmd *cobra.Command, args []string) error {
			body := map[string]string{"platform": authPushPlatform, "token": authPushToken}
			return runSimpleHTTP("POST", "/auth/push-token", body)
		},
	}
)

var authPushDeleteCmd = &cobra.Command{
	Use:   "delete <token-id>",
	Short: "Delete a push token by its ID",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		return runSimpleHTTP("DELETE", "/auth/push-token/"+args[0], nil)
	},
}

func init() {
	rootCmd.AddCommand(authCmd)
	authCmd.AddCommand(authMeCmd, authSessionsCmd, authProvidersCmd, authRefreshCmd,
		authPasswordCmd, authForgotCmd, authResetCmd, authDeleteUserCmd, authPushTokenCmd)

	authRefreshCmd.Flags().StringVar(&authRefreshTokenFlag, "refresh-token", "", "Refresh token (defaults to the one stored in config)")

	authPasswordCmd.Flags().StringVar(&authChgOldPassword, "old-password", "", "Current password")
	authPasswordCmd.Flags().StringVar(&authChgNewPassword, "new-password", "", "New password (min 8 chars)")
	_ = authPasswordCmd.MarkFlagRequired("old-password")
	_ = authPasswordCmd.MarkFlagRequired("new-password")

	authForgotCmd.Flags().StringVar(&authForgotEmail, "email", "", "Email of the account")
	_ = authForgotCmd.MarkFlagRequired("email")

	authResetCmd.Flags().StringVar(&authResetToken, "token", "", "Reset token from the email")
	authResetCmd.Flags().StringVar(&authResetPassword, "new-password", "", "New password")
	_ = authResetCmd.MarkFlagRequired("token")
	_ = authResetCmd.MarkFlagRequired("new-password")

	authPushTokenCmd.AddCommand(authPushListCmd, authPushAddCmd, authPushDeleteCmd)
	authPushAddCmd.Flags().StringVar(&authPushPlatform, "platform", "", "Platform: fcm | apns | ntfy")
	authPushAddCmd.Flags().StringVar(&authPushToken, "token", "", "Push token value")
	_ = authPushAddCmd.MarkFlagRequired("platform")
	_ = authPushAddCmd.MarkFlagRequired("token")
}
