# Datum Camera Simulator

A standalone Go tool to simulate an ESP32-S3 Camera device connecting to the Datum Server. It simulates telemetry data, motion events, and a video stream using only standard HTTP endpoints (no MQTT required).

## Features
- **Telemetry:** Sends random Temperature, Humidity, and Motion events every 5 seconds.
- **Command Polling:** Listens for remote commands (e.g., `update_settings`) via HTTP Long Polling.
- **Video Stream:** Generates synthetic JPEG frames with timestamps and uploads them to the server.

## Installation

No installation required if you have Go installed. Just run it directly.

## Usage

```bash
# Run from the project root
go run scripts/datum_camera_sim/main.go --device-id <DEVICE_ID> --api-key <DEVICE_KEY>

# Optional Flags
--server  <URL>   # Datum Server URL (default: http://localhost:8000)
--fps     <INT>   # Frames Per Second for the stream (default: 1)
```

## Example

```bash
go run scripts/datum_camera_sim/main.go \
  --server http://localhost:8000 \
  --device-id 550e8400-e29b-41d4-a716-446655440000 \
  --api-key sk_test_123456 \
  --fps 5
```
