package storage

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// ============ Command Tests ============

func TestCreateCommand(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create test device
	device := &Device{
		ID:        "dev_test_cmd",
		UserID:    "user_test",
		Name:      "Test Device",
		Type:      "sensor",
		APIKey:    "sk_test_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	err := storage.CreateDevice(device)
	require.NoError(t, err)

	// Create command
	cmd := &Command{
		ID:       "cmd_001",
		DeviceID: "dev_test_cmd",
		Action:   "reboot",
		Params: map[string]interface{}{
			"delay": 5,
		},
		Status:    "pending",
		CreatedAt: time.Now(),
	}

	err = storage.CreateCommand(cmd)
	require.NoError(t, err)

	// Verify command was created
	commands, err := storage.GetPendingCommands("dev_test_cmd")
	require.NoError(t, err)
	assert.Len(t, commands, 1)
	assert.Equal(t, "cmd_001", commands[0].ID)
	assert.Equal(t, "reboot", commands[0].Action)
	assert.Equal(t, "pending", commands[0].Status)
}

func TestGetPendingCommands(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create device
	device := &Device{
		ID:        "dev_pending_cmd",
		UserID:    "user_test",
		Name:      "Command Device",
		Type:      "actuator",
		APIKey:    "sk_cmd_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	// Create multiple commands
	for i := 1; i <= 3; i++ {
		cmd := &Command{
			ID:        "cmd_" + string(rune('0'+i)),
			DeviceID:  "dev_pending_cmd",
			Action:    "action_" + string(rune('0'+i)),
			Status:    "pending",
			CreatedAt: time.Now(),
		}
		storage.CreateCommand(cmd)
	}

	// Get pending commands
	commands, err := storage.GetPendingCommands("dev_pending_cmd")
	require.NoError(t, err)
	assert.Len(t, commands, 3)
}

func TestGetPendingCommandsEmpty(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Get commands for non-existent device
	commands, err := storage.GetPendingCommands("dev_nonexistent")
	require.NoError(t, err)
	assert.Empty(t, commands)
}

func TestAcknowledgeCommand(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create device and command
	device := &Device{
		ID:        "dev_ack_cmd",
		UserID:    "user_test",
		Name:      "Ack Device",
		Type:      "actuator",
		APIKey:    "sk_ack_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	cmd := &Command{
		ID:        "cmd_ack_001",
		DeviceID:  "dev_ack_cmd",
		Action:    "update_config",
		Status:    "pending",
		CreatedAt: time.Now(),
	}
	storage.CreateCommand(cmd)

	// Acknowledge command
	result := map[string]interface{}{
		"success": true,
		"message": "Config updated",
	}
	err := storage.AcknowledgeCommand("cmd_ack_001", result)
	require.NoError(t, err)

	// Verify command is no longer pending
	commands, err := storage.GetPendingCommands("dev_ack_cmd")
	require.NoError(t, err)
	assert.Empty(t, commands, "Acknowledged command should not be in pending list")
}

func TestAcknowledgeCommandNotFound(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	result := map[string]interface{}{"success": true}
	err := storage.AcknowledgeCommand("cmd_nonexistent", result)
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "command not found")
}

func TestGetPendingCommandCount(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create device
	device := &Device{
		ID:        "dev_count_cmd",
		UserID:    "user_test",
		Name:      "Count Device",
		Type:      "sensor",
		APIKey:    "sk_count_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	// Initially should be 0
	count := storage.GetPendingCommandCount("dev_count_cmd")
	assert.Equal(t, 0, count)

	// Add commands
	for i := 1; i <= 5; i++ {
		cmd := &Command{
			ID:        "cmd_count_" + string(rune('0'+i)),
			DeviceID:  "dev_count_cmd",
			Action:    "test_action",
			Status:    "pending",
			CreatedAt: time.Now(),
		}
		storage.CreateCommand(cmd)
	}

	// Should now be 5
	count = storage.GetPendingCommandCount("dev_count_cmd")
	assert.Equal(t, 5, count)

	// Acknowledge one command
	storage.AcknowledgeCommand("cmd_count_1", map[string]interface{}{"status": "ok"})

	// Should now be 4
	count = storage.GetPendingCommandCount("dev_count_cmd")
	assert.Equal(t, 4, count)
}

func TestCommandLifecycle(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Setup device
	device := &Device{
		ID:        "dev_lifecycle",
		UserID:    "user_test",
		Name:      "Lifecycle Device",
		Type:      "actuator",
		APIKey:    "sk_lifecycle_key",
		Status:    "active",
		CreatedAt: time.Now(),
	}
	storage.CreateDevice(device)

	// 1. Create command
	cmd := &Command{
		ID:       "cmd_lifecycle_001",
		DeviceID: "dev_lifecycle",
		Action:   "firmware_update",
		Params: map[string]interface{}{
			"version": "2.0.0",
			"url":     "https://update.example.com/fw-2.0.0.bin",
		},
		Status:    "pending",
		CreatedAt: time.Now(),
	}
	err := storage.CreateCommand(cmd)
	require.NoError(t, err)

	// 2. Verify pending
	count := storage.GetPendingCommandCount("dev_lifecycle")
	assert.Equal(t, 1, count)

	// 3. Device fetches commands
	commands, err := storage.GetPendingCommands("dev_lifecycle")
	require.NoError(t, err)
	assert.Len(t, commands, 1)
	assert.Equal(t, "firmware_update", commands[0].Action)

	// 4. Device acknowledges
	result := map[string]interface{}{
		"success":        true,
		"old_version":    "1.0.0",
		"new_version":    "2.0.0",
		"restart_needed": true,
	}
	err = storage.AcknowledgeCommand("cmd_lifecycle_001", result)
	require.NoError(t, err)

	// 5. Verify no longer pending
	count = storage.GetPendingCommandCount("dev_lifecycle")
	assert.Equal(t, 0, count)
}

func TestMultipleDeviceCommands(t *testing.T) {
	storage, cleanup := createTestStorage(t)
	defer cleanup()

	// Create two devices
	for i := 1; i <= 2; i++ {
		device := &Device{
			ID:        "dev_multi_" + string(rune('0'+i)),
			UserID:    "user_test",
			Name:      "Device " + string(rune('0'+i)),
			Type:      "sensor",
			APIKey:    "sk_multi_" + string(rune('0'+i)),
			Status:    "active",
			CreatedAt: time.Now(),
		}
		storage.CreateDevice(device)

		// Add commands to each device
		for j := 1; j <= 3; j++ {
			cmd := &Command{
				ID:        "cmd_multi_" + string(rune('0'+i)) + "_" + string(rune('0'+j)),
				DeviceID:  "dev_multi_" + string(rune('0'+i)),
				Action:    "action_" + string(rune('0'+j)),
				Status:    "pending",
				CreatedAt: time.Now(),
			}
			storage.CreateCommand(cmd)
		}
	}

	// Verify each device has its own commands
	commands1, _ := storage.GetPendingCommands("dev_multi_1")
	commands2, _ := storage.GetPendingCommands("dev_multi_2")

	assert.Len(t, commands1, 3)
	assert.Len(t, commands2, 3)

	// Verify commands don't cross devices
	for _, cmd := range commands1 {
		assert.Equal(t, "dev_multi_1", cmd.DeviceID)
	}
	for _, cmd := range commands2 {
		assert.Equal(t, "dev_multi_2", cmd.DeviceID)
	}
}
