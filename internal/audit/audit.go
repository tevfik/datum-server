package audit

import (
	"time"

	"datum-go/internal/logger"
)

// Action describes what was done.
type Action string

const (
	ActionUserCreated       Action = "user.created"
	ActionUserDeleted       Action = "user.deleted"
	ActionUserLogin         Action = "user.login"
	ActionUserPasswordReset Action = "user.password_reset"
	ActionUserStatusUpdate  Action = "user.status_update"

	ActionDeviceCreated    Action = "device.created"
	ActionDeviceDeleted    Action = "device.deleted"
	ActionDeviceKeyRotated Action = "device.key_rotated"
	ActionDeviceKeyRevoked Action = "device.key_revoked"
	ActionDeviceConfigPush Action = "device.config_push"

	ActionDataAccess   Action = "data.access"
	ActionDataExport   Action = "data.export"
	ActionDBReset      Action = "db.reset"
	ActionDBCleanup    Action = "db.cleanup"
	ActionConfigUpdate Action = "config.update"

	ActionAdminDataAccess Action = "admin.data_access"
	ActionFirmwareUpload  Action = "firmware.upload"
)

// Entry is a single audit log record.
type Entry struct {
	Timestamp time.Time              `json:"timestamp"`
	Action    Action                 `json:"action"`
	ActorID   string                 `json:"actor_id"`
	ActorRole string                 `json:"actor_role,omitempty"`
	TargetID  string                 `json:"target_id,omitempty"`
	IP        string                 `json:"ip,omitempty"`
	Details   map[string]interface{} `json:"details,omitempty"`
}

// Log emits an audit entry via structured logging.
func Log(entry Entry) {
	entry.Timestamp = time.Now()
	log := logger.GetLogger()
	evt := log.Warn().
		Str("audit_action", string(entry.Action)).
		Str("actor_id", entry.ActorID).
		Time("audit_ts", entry.Timestamp)

	if entry.ActorRole != "" {
		evt = evt.Str("actor_role", entry.ActorRole)
	}
	if entry.TargetID != "" {
		evt = evt.Str("target_id", entry.TargetID)
	}
	if entry.IP != "" {
		evt = evt.Str("client_ip", entry.IP)
	}
	if len(entry.Details) > 0 {
		evt = evt.Interface("details", entry.Details)
	}
	evt.Msg("AUDIT")
}
