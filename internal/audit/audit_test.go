package audit

import (
	"testing"
)

func TestLogDoesNotPanic(t *testing.T) {
	// Basic smoke test: ensure Log does not panic with various inputs.
	Log(Entry{
		Action:  ActionUserCreated,
		ActorID: "user-1",
	})

	Log(Entry{
		Action:    ActionAdminDataAccess,
		ActorID:   "admin-1",
		ActorRole: "admin",
		TargetID:  "device-1",
		IP:        "192.168.1.1",
		Details: map[string]interface{}{
			"reason": "support request",
		},
	})

	Log(Entry{
		Action:  ActionDBReset,
		ActorID: "admin-2",
	})
}
