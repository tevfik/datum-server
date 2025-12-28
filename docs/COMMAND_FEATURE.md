# Command Management Feature - datumctl

**Added**: December 28, 2024  
**Module**: `cmd/datumctl/command.go` (490 lines)

## Overview

Added comprehensive command management to datumctl, enabling users to send commands to IoT devices and monitor their execution status. This closes a significant gap where the server had full command infrastructure but no CLI interface.

## Features Implemented

### 1. Send Commands (`command send`)
Send commands to devices with parameters:

```bash
# Simple command
datumctl command send device-001 reboot

# With parameters
datumctl command send device-001 set-config --param interval=60 --param mode=auto

# With JSON parameters
datumctl command send device-001 update --params-json '{"config":{"interval":60}}'

# Wait for execution result
datumctl command send device-001 reboot --wait
```

**Features**:
- Key-value parameter syntax (`--param key=value`)
- JSON parameter support (`--params-json`)
- Async execution (returns command ID immediately)
- Wait mode (`--wait`) polls for execution result
- JSON output support (`--json`)

### 2. List Commands (`command list`)
View all commands for a device:

```bash
datumctl command list device-001
datumctl command list device-001 --json
```

**Output**:
```
📋 Commands for device: device-001

┌─────────────┬────────────┬──────────────┬─────────────┬──────────────┐
│ Command ID  │ Action     │ Status       │ Created     │ Params       │
├─────────────┼────────────┼──────────────┼─────────────┼──────────────┤
│ cmd_abc123  │ reboot     │ ✅ success   │ Jan 02 15:04│              │
│ cmd_def456  │ set-config │ ⏳ pending   │ Jan 02 15:05│ {"interval":60}│
│ cmd_ghi789  │ update     │ ❌ failed    │ Jan 02 15:06│ {...}        │
└─────────────┴────────────┴──────────────┴─────────────┴──────────────┘

Total: 3 commands
```

**Features**:
- Table view with status emojis
- Parameter preview (truncated if long)
- Sortedby creation time
- JSON output option

### 3. Get Command Details (`command get`)
View detailed information about a specific command:

```bash
datumctl command get device-001 cmd_abc123
```

**Output**:
```
📋 Command Details
═══════════════════════════════════════════

🆔 Command ID: cmd_abc123
📱 Device ID:  device-001
⚡ Action:     set-config
📊 Status:     success

📝 Parameters:
  {
    "interval": 60,
    "mode": "auto"
  }

🕐 Created:    2025-12-28 15:04:23
📤 Delivered:  2025-12-28 15:04:25
✅ Completed:  2025-12-28 15:04:27

📋 Result:
  {
    "success": true,
    "config_updated": true
  }
```

**Features**:
- Complete command lifecycle timeline
- Formatted parameter display
- Execution results
- Error messages (if failed)

### 4. Command History (`command history`)
View command execution history with statistics:

```bash
# Full history
datumctl command history device-001

# Last 10 commands
datumctl command history device-001 --limit 10

# Filter by status
datumctl command history device-001 --status failed
```

**Output**:
```
📊 Command History for device: device-001
═══════════════════════════════════════════

Total Commands: 15

Status Breakdown:
  ✅ success: 12
  ❌ failed: 2
  ⏳ pending: 1

Recent Commands:
───────────────────────────────────────────

✅ [Jan 02 15:04] reboot
   ID: cmd_abc123

⏳ [Jan 02 15:05] set-config
   ID: cmd_def456
   Params: {"interval":60}

❌ [Jan 02 15:06] update
   ID: cmd_ghi789
   Params: {"firmware":"1.2.0"}
```

**Features**:
- Status breakdown statistics
- Chronological listing
- Limit and filter options
- Parameter display

### 5. Cancel Command (`command cancel`)
Cancel a pending command before execution:

```bash
datumctl command cancel device-001 cmd_abc123
```

**Output**:
```
✓ Command cancelled successfully
📋 Command ID: cmd_abc123
```

## Command States

Commands progress through these states:

1. **pending** ⏳ - Queued, waiting for device to poll
2. **delivered** 📤 - Sent to device, awaiting execution
3. **success** ✅ - Executed successfully
4. **failed** ❌ - Execution failed

## Integration with Server

Commands use existing server endpoints:

- `POST /devices/:device_id/commands` - Send command
- `GET /devices/:device_id/commands` - List commands
- `GET /devices/:device_id/commands/:command_id` - Get command details
- `DELETE /devices/:device_id/commands/:command_id` - Cancel command

## Use Cases

### 1. Remote Device Control
```bash
# Reboot device
datumctl command send sensor-01 reboot

# Update configuration
datumctl command send sensor-01 set-interval --param seconds=300

# Trigger firmware update
datumctl command send sensor-01 ota-update --param version=1.2.0 --param url=https://...
```

### 2. Configuration Management
```bash
# Set WiFi credentials
datumctl command send device-001 set-wifi \
  --param ssid=MyNetwork \
  --param password=SecurePass123

# Update sampling rate
datumctl command send sensor-farm-* set-config --param interval=60
```

### 3. Diagnostics
```bash
# Get device diagnostics
datumctl command send device-001 diagnostics --wait

# Clear device cache
datumctl command send device-001 clear-cache

# Reset to factory defaults
datumctl command send device-001 factory-reset
```

### 4. Monitoring
```bash
# Check command execution status
datumctl command list device-001

# Review failed commands
datumctl command history device-001 --status failed

# Get detailed failure info
datumctl command get device-001 cmd_failed_123
```

## Complete datumctl Command List

With the addition of command management, datumctl now provides:

```
Authentication:
  ✓ setup          - Initialize system
  ✓ login          - User login

Devices:
  ✓ device create  - Create device
  ✓ device list    - List devices
  ✓ device get     - Get device details
  ✓ device update  - Update device properties
  ✓ device delete  - Delete device

Provisioning (WiFi AP):
  ✓ provision register  - Register device
  ✓ provision list      - List requests
  ✓ provision status    - Get status
  ✓ provision cancel    - Cancel request

Commands (NEW):
  ✓ command send     - Send command to device
  ✓ command list     - List device commands
  ✓ command get      - Get command details
  ✓ command history  - Show execution history
  ✓ command cancel   - Cancel pending command

Data:
  ✓ data get      - Query device data
  ✓ data post     - Send data
  ✓ data stats    - Data statistics

Admin:
  ✓ admin create-user     - Create user
  ✓ admin list-users      - List users
  ✓ admin delete-user     - Delete user
  ✓ admin reset-password  - Reset password
  ✓ admin reset-system    - Reset system
  ✓ admin stats           - System statistics
  ✓ admin get-config      - System configuration

System:
  ✓ status    - System status
  ✓ version   - Version info
  ✓ config    - Configuration management
```

## Implementation Details

**File**: `cmd/datumctl/command.go`  
**Lines**: 490  
**Commands**: 5  
**Functions**: 5 RunE handlers

**Dependencies**:
- `github.com/spf13/cobra` - CLI framework
- `github.com/olekukonko/tablewriter` - Table formatting
- Existing `APIClient` - HTTP communication
- Existing `ParseResponse` - JSON parsing

**Key Design Decisions**:
1. **Consistent API patterns** - Uses same client methods as other commands
2. **User-friendly output** - Table views with emojis for status
3. **Flexible parameters** - Both key-value and JSON syntax
4. **Wait mode** - Optional polling for synchronous execution
5. **Status visualization** - Clear emoji indicators for command states

## Testing

Test the command feature:

```bash
# Build
cd cmd/datumctl && go build

# Test commands
./datumctl command --help
./datumctl command send --help
./datumctl command list --help
./datumctl command history --help

# Integration test (requires running server)
./datumctl command send test-device reboot
./datumctl command list test-device
```

## Future Enhancements

Potential improvements:

1. **Batch commands** - Send same command to multiple devices
2. **Command templates** - Save and reuse common command patterns
3. **Scheduled commands** - Queue commands for future execution
4. **Command logs** - Persistent command history with search
5. **Command macros** - Chain multiple commands together
6. **Interactive mode** - Integrate into datumctl interactive menu

## Summary

The command management feature makes datumctl a **complete IoT device management CLI**, covering:

- ✅ Device lifecycle (create, read, update, delete)
- ✅ Provisioning workflow (WiFi AP setup)
- ✅ **Command execution** (remote control) **← NEW**
- ✅ Data management (query, post, stats)
- ✅ User administration (users, system)
- ✅ System monitoring (status, config)

**Impact**: Enables complete device management workflows from the command line without needing to use HTTP APIs directly.
