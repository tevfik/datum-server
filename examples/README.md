# Device Examples

This directory contains example implementations for connecting IoT devices to the Datum platform.

## Arduino / ESP32

**Location:** [`arduino/datum_device.ino`](arduino/datum_device.ino)

A complete ESP32/Arduino client that demonstrates:
- WiFi connectivity
- Sensor data collection
- Data transmission to Datum API
- Command polling and execution
- Command acknowledgment

### Requirements

- ESP32 or ESP8266 board
- Arduino IDE or PlatformIO
- Libraries:
  - WiFi (built-in for ESP32)
  - HTTPClient (built-in for ESP32)
  - ArduinoJson

### Quick Setup

1. Open `datum_device.ino` in Arduino IDE
2. Update configuration section:
   ```cpp
   const char* WIFI_SSID = "your-wifi-ssid";
   const char* WIFI_PASSWORD = "your-wifi-password";
   const char* API_URL = "http://your-server:8080";
   const char* DEVICE_ID = "dev_xxxxxxxx";
   const char* API_KEY = "dk_xxxxxxxx";
   ```
3. Upload to your board
4. Open Serial Monitor at 115200 baud

### Features

- **Auto-reconnect**: Automatically reconnects on WiFi loss
- **Command support**: Polls and executes pending commands
- **Efficient JSON**: Uses ArduinoJson for minimal memory footprint

---

## MicroPython

**Location:** [`micropython/main.py`](micropython/main.py)

A MicroPython client for Raspberry Pi Pico W, ESP32, or any MicroPython-compatible device.

### Requirements

- MicroPython-compatible board (ESP32, Pico W, etc.)
- MicroPython firmware installed
- `urequests` module (usually built-in)

### Quick Setup

1. Copy `main.py` to your device
2. Update configuration:
   ```python
   WIFI_SSID = "your-wifi-ssid"
   WIFI_PASSWORD = "your-wifi-password"
   API_URL = "http://your-server:8080"
   DEVICE_ID = "dev_xxxxxxxx"
   API_KEY = "dk_xxxxxxxx"
   ```
3. Reset device or run `main.py`

### Features

- **WiFi management**: Connection handling with retry logic
- **Command support**: Polls commands and sends acknowledgments
- **Error handling**: Graceful handling of network failures
- **Low power**: Designed for battery-powered devices

---

## Creating Your Own Client

### API Endpoints

| Endpoint | Method | Auth | Description |
|----------|--------|------|-------------|
| `/dev/{id}/data` | POST | API Key | Send sensor data |
| `/dev/{id}/cmd` | GET | API Key | Get pending commands |
| `/dev/{id}/cmd/{cmd}/ack` | POST | API Key | Acknowledge command |

### Authentication

All device endpoints use Bearer token authentication with the device API key:

```
Authorization: Bearer dk_your_api_key_here
```

### Data Format

Send JSON payloads with your sensor data:

```json
{
  "temperature": 22.5,
  "humidity": 45,
  "battery_voltage": 3.7
}
```

### Response Format

The `/data` endpoint returns:

```json
{
  "status": "ok",
  "timestamp": "2024-01-15T10:30:00Z",
  "commands_pending": 2
}
```

### Handling Commands

1. Check `commands_pending` in push response
2. If > 0, poll `/dev/{id}/cmd`
3. Execute each command
4. Acknowledge with `/dev/{id}/cmd/{cmd}/ack`

Command format:
```json
{
  "id": "cmd_abc123",
  "action": "reboot",
  "payload": {},
  "created_at": "2024-01-15T10:30:00Z"
}
```

### Best Practices

1. **Use HTTPS** in production
2. **Implement retries** for network failures
3. **Buffer data** when offline
4. **Use efficient intervals** - don't oversample
5. **Handle rate limits** (429 responses)
6. **Rotate API keys** periodically

---

## Additional Resources

- [Quick Start Guide](../docs/tutorials/QUICK_START.md)
- [Use Cases](../docs/tutorials/USE_CASES.md)
- [API Reference](../docs/reference/API.md)
- [Firmware Development](../docs/tutorials/FIRMWARE.md)
