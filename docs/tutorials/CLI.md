# datumctl - Datum IoT Platform CLI

Command-line interface for interacting with Datum IoT platform from the terminal. Manage devices, query data, and monitor your infrastructure without leaving the command line.

## Features

- 🔐 **Authentication**: Login and save credentials
- 📱 **Device Management**: Create, list, view, and delete devices
- 📊 **Data Queries**: Retrieve time-series data with flexible filters
- 🔑 **API Key Management**: Save and use API keys
- 📋 **Multiple Output Formats**: Table or JSON output
- ⚙️ **Configuration**: Save server URL and credentials
- 🔍 **Verbose Mode**: Debug API calls

## Installation

### Build from Source

```bash
# Build CLI tool
cd datum-server
make build-cli

# Or manually
go build -o datumctl ./cmd/datumctl

# Install to PATH
sudo mv datumctl /usr/local/bin/
```

### Download Binary

```bash
# Download latest release (Linux)
curl -L https://github.com/yourusername/datum-server/releases/latest/download/datumctl-linux-amd64 -o datumctl
chmod +x datumctl
sudo mv datumctl /usr/local/bin/
```

## Quick Start

### 1. Login

```bash
# Login with email/password
datumctl login --email admin@example.com
Password: ******

✅ Login successful!
   User: admin@example.com (admin)
   Token: eyJhbGciOiJIUzI1NiIs...
   Saved to: /home/user/.datumctl.yaml
```

### 2. List Devices

```bash
datumctl device list

📱 Devices (3)

+----------------+-------------------+-------------+---------+----------------------+
| ID             | Name              | Type        | Status  | Last Seen            |
+----------------+-------------------+-------------+---------+----------------------+
| temp-sensor-01 | Temperature       | temperature | active  | 2025-12-25 10:30:00  |
| humidity-01    | Humidity Sensor   | humidity    | active  | 2025-12-25 10:29:45  |
| esp32-device   | ESP32 Device      | sensor      | offline | 2025-12-24 18:00:00  |
+----------------+-------------------+-------------+---------+----------------------+
```

### 3. Create Device

```bash
datumctl device create --name "New Sensor" --type temperature

✅ Device created!

  ID:      sensor-abc123
  Name:    New Sensor
  Type:    temperature

  🔑 API Key: dev_xxxxxxxxxxxxxxxxxxxxxxxx
  ⚠️  Save this key - it won't be shown again!
```

### 4. Query Data

```bash
# Get last hour of data
datumctl data get --device temp-sensor-01 --last 1h

📊 Data for device: temp-sensor-01 (120 records)

+---------------------+-------------+----------+
| Timestamp           | temperature | humidity |
+---------------------+-------------+----------+
| 2025-12-25 10:30:00 | 25.5        | 60       |
| 2025-12-25 10:29:00 | 25.3        | 61       |
| 2025-12-25 10:28:00 | 25.4        | 60       |
+---------------------+-------------+----------+
```

## Commands

### Global Flags

```bash
--server string      Datum server URL (default: http://localhost:8000)
--token string       JWT token for authentication
--api-key string     API key for authentication
--json               Output in JSON format
--config string      Config file (default: $HOME/.datumctl.yaml)
-v, --verbose        Verbose output (show HTTP requests)
```

### Authentication

#### Login

```bash
# Interactive password prompt
datumctl login --email admin@example.com

# With password (not recommended for production)
datumctl login --email admin@example.com --password secret

# Save API key
datumctl login --api-key your-device-api-key

# Custom server
datumctl login --server https://datum.example.com --email admin@example.com
```

### Device Management

#### List Devices

```bash
# Table format (default)
datumctl device list

# JSON format
datumctl device list --json
```

#### Get Device Details

```bash
datumctl device get my-device-id

📱 Device: Temperature Sensor

  ID:         temp-sensor-01
  Name:       Temperature Sensor
  Type:       temperature
  Status:     active
  Created:    2025-12-20 10:00:00
  Last Seen:  2025-12-25 10:30:00
  Data Count: 15420
```

#### Create Device

```bash
# Auto-generated ID
datumctl device create --name "My Sensor" --type temperature

# Custom ID
datumctl device create --id custom-sensor-01 --name "Custom Sensor" --type humidity
```

#### Delete Device

```bash
# With confirmation prompt
datumctl device delete my-device-id

# Skip confirmation
datumctl device delete my-device-id --force
```

### Data Queries

#### Get Data

```bash
# Last N time
datumctl data get --device my-device --last 1h   # Last hour
datumctl data get --device my-device --last 30m  # Last 30 minutes
datumctl data get --device my-device --last 7d   # Last 7 days

# Specific time range
datumctl data get --device my-device \
  --from "2025-12-25 10:00" \
  --to "2025-12-25 12:00"

# Limit results
datumctl data get --device my-device --last 1h --limit 10

# JSON output
datumctl data get --device my-device --last 1h --json
```

#### Post Data

```bash
# Post data to device
datumctl data post --device my-device \
  --data '{"temperature": 25.5, "humidity": 60}'

# Using API key
datumctl data post --api-key dev_xxxxx \
  --data '{"temperature": 25.5}'
```

#### Data Statistics

```bash
datumctl data stats --device my-device --last 24h

📈 Statistics for device: my-device (last 24h)

  Count:      1440
  First:      2025-12-24 10:30:00
  Last:       2025-12-25 10:30:00

  Metrics:
    temperature:
      Min:  18.5
      Max:  28.3
      Avg:  23.4
    humidity:
      Min:  45
      Max:  75
      Avg:  60
```

### System Commands

#### Check Status

```bash
datumctl status

✅ Server Status

  URL:      http://localhost:8000
  Status:   healthy
  Version:  1.0.0
  Uptime:   5h 23m
```

#### Show Configuration

```bash
datumctl config show

⚙️  Configuration

  Config file: /home/user/.datumctl.yaml
  Server:      http://localhost:8000
  Token:       eyJhbGciOiJIUzI1NiIs...
  API Key:     (not set)
```

#### Version

```bash
datumctl version

datumctl version 1.0.0
Datum IoT Platform CLI
```

## Configuration File

The CLI saves credentials in `~/.datumctl.yaml`:

```yaml
server: http://localhost:8000
token: eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
# OR
api_key: dev_xxxxxxxxxxxxxxxxxxxxxxxx
```

You can also specify a custom config file:

```bash
datumctl --config /path/to/config.yaml device list
```

## Examples

### Complete Workflow

```bash
# 1. Login
datumctl login --email admin@example.com

# 2. Create a device
datumctl device create --name "Office Sensor" --type temperature

# 3. Get the device ID and use API key to post data
export API_KEY="dev_xxxxx"  # From previous step
datumctl data post --api-key $API_KEY \
  --data '{"temperature": 23.5, "humidity": 55}'

# 4. Query recent data
datumctl data get --device office-sensor --last 1h

# 5. Get statistics
datumctl data stats --device office-sensor --last 24h
```

### Testing & Development

```bash
# Use verbose mode to see API calls
datumctl -v device list

→ GET /admin/dev
← 200 OK

# Use JSON output for scripting
devices=$(datumctl device list --json)
echo $devices | jq '.[] | .name'

# Use custom server for testing
datumctl --server http://localhost:9000 status
```

### Automation & Scripts

```bash
#!/bin/bash
# Auto-create devices from CSV

while IFS=, read -r name type; do
  echo "Creating device: $name ($type)"
  datumctl device create --name "$name" --type "$type"
done < devices.csv
```

```bash
#!/bin/bash
# Backup device data daily

DEVICE="my-sensor"
YESTERDAY=$(date -d "yesterday" +%Y-%m-%d)

datumctl data get \
  --device $DEVICE \
  --from "$YESTERDAY 00:00" \
  --to "$YESTERDAY 23:59" \
  --json > backup-$YESTERDAY.json
```

## Error Handling

The CLI provides clear error messages:

```bash
# Authentication required
datumctl device list
Error: API error (401): Unauthorized

# Device not found
datumctl device get nonexistent
Error: API error (404): Device not found

# Network error
datumctl status
Error: request failed: dial tcp: connection refused
```

## Environment Variables

You can also use environment variables:

```bash
export DATUM_SERVER=http://localhost:8000
export DATUM_TOKEN=eyJhbGci...
export DATUM_API_KEY=dev_xxxxx

datumctl device list  # Uses environment variables
```

## Shell Completion

Generate shell completion scripts:

```bash
# Bash
datumctl completion bash > /etc/bash_completion.d/datumctl

# Zsh
datumctl completion zsh > "${fpath[1]}/_datumctl"

# Fish
datumctl completion fish > ~/.config/fish/completions/datumctl.fish
```

## Tips & Tricks

### Use JSON + jq for Advanced Queries

```bash
# Count devices by type
datumctl device list --json | jq 'group_by(.type) | map({type: .[0].type, count: length})'

# Get device IDs only
datumctl device list --json | jq -r '.[].id'

# Filter active devices
datumctl device list --json | jq '.[] | select(.status == "active")'
```

### Alias Common Commands

```bash
# Add to ~/.bashrc or ~/.zshrc
alias dctl='datumctl'
alias dls='datumctl device list'
alias ddata='datumctl data get'

# Usage
dls
ddata --device temp-01 --last 1h
```

### Multiple Environments

```bash
# Development
alias dctl-dev='datumctl --server http://localhost:8000'

# Staging
alias dctl-staging='datumctl --server https://staging.datum.example.com'

# Production
alias dctl-prod='datumctl --server https://api.datum.example.com'
```

## Troubleshooting

### Connection Issues

```bash
# Test server connectivity
curl http://localhost:8000/health

# Verbose mode to debug
datumctl -v status
```

### Authentication Issues

```bash
# Check saved credentials
datumctl config show

# Re-login
datumctl login --email admin@example.com

# Use token directly
datumctl --token your-jwt-token device list
```

### Data Not Found

```bash
# Check device exists
datumctl device get my-device

# Verify time range
datumctl data get --device my-device --last 7d --limit 1
```

## Development

### Building

```bash
# Build for current platform
go build -o datumctl ./cmd/datumctl

# Cross-compile for Linux
GOOS=linux GOARCH=amd64 go build -o datumctl-linux-amd64 ./cmd/datumctl

# Cross-compile for macOS
GOOS=darwin GOARCH=amd64 go build -o datumctl-darwin-amd64 ./cmd/datumctl

# Cross-compile for Windows
GOOS=windows GOARCH=amd64 go build -o datumctl-windows-amd64.exe ./cmd/datumctl
```

### Testing

```bash
# Start server
make run-server

# Test CLI commands
./datumctl login --email admin@example.com
./datumctl device list
./datumctl status
```

## License

MIT License - See [LICENSE](../LICENSE)

## Related

- [Datum Server](../README.md) - Go backend API
- [API Documentation](API.md) - REST API reference
- [Datum Dashboard](../../datum-dashboard/README.md) - Web UI
