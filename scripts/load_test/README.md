# Datum Load Test Runner

A unified load testing tool for Datum Server, supporting both HTTP and MQTT protocols. It simulates multiple users and devices sending telemetry data.

## Features
- **Multi-Protocol Support**: HTTP, MQTT, or Mixed (50/50).
- **Realistic Flow**: Automatically registers users, logs in, creates devices, and obtains credentials.
- **Concurrent Simulation**: Simulates thousands of devices with realistic jitter.
- **Detailed Metrics**: Tracks Request/Sec, Latency, and Error rates.

## Installation

Ensure you have Go installed, then run:

```bash
# Install dependencies
go mod tidy
```

## Usage

Run the script using `go run`:

```bash
go run scripts/load_test/main.go [flags]
```

### Flags

| Flag | Default | Description |
|------|---------|-------------|
| `-url` | `http://localhost:8080` | Target Server HTTP URL |
| `-proto` | `http` | Protocol mode: `http`, `mqtt`, `mixed` |
| `-mqtt-broker` | `tcp://localhost:1883` | MQTT Broker URL (used if proto is mqtt/mixed) |
| `-users` | `5` | Number of unique users to simulate |
| `-devices` | `2` | Number of devices to create per user |
| `-duration` | `30s` | Duration of the load test |
| `-cleanup` | `false` | Run in cleanup mode (delete users from previous runs) |
| `-cleanup-after` | `false` | Automatically delete users after test completes |

## Examples

### 1. HTTP Load Test (Default)
Simulate 50 users (100 devices total) sending data via HTTP POST for 1 minute.

```bash
go run scripts/load_test/main.go -users 50 -devices 2 -duration 1m
```

### 2. MQTT Load Test
Simulate 100 devices connecting to MQTT broker and publishing telemetry.

```bash
go run scripts/load_test/main.go -proto mqtt -mqtt-broker tcp://localhost:1883 -users 20 -devices 5
```


### 3. Mixed Traffic Test
Simulate a hybrid environment where devices randomly use HTTP or MQTT.

```bash
go run scripts/load_test/main.go -proto mixed -users 100
```

> [!IMPORTANT]
> When testing a **remote** server in `mixed` or `mqtt` mode, you must specify **both** `-url` and `-mqtt-broker`.
> The flags are independent; setting `-url` does not automatically update the MQTT broker address.
>
> Example:
> ```bash
> go run scripts/load_test/main.go \
>   -proto mixed \
>   -url https://api.myserver.com \
>   -mqtt-broker tcp://mqtt.myserver.com:1883
> ```

## How it Works
1. **Setup Phase**: The script registers N users via HTTP API, logs them in to get JWTs, and registers M devices per user to get API Keys.
2. **Connect Phase**: If MQTT is enabled, it establishes persistent MQTT connections for all devices using their credentials.
3. **Load Phase**: All devices run concurrently, sending telemetry at varying intervals (approx every 100ms) for the specified duration.
4. **Report Phase**: Outputs aggregate statistics including total requests, success/error counts, and average latency.

## Data Cleanup

The load test creates many users (`loaduser_...`) and devices. To clean them up:

### Auto-Cleanup (Recommended)
Add `-cleanup-after` to your command. This will delete all created users immediately after the test finishes.

```bash
go run scripts/load_test/main.go -users 100 -devices 5 -cleanup-after
```

### Manual Cleanup
If a test was interrupted (e.g., Ctrl+C), you can run a cleanup-only pass. This reads the `load_test_users.json` file created during the test.

```bash
go run scripts/load_test/main.go -cleanup
```
