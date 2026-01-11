# ESP32 WiFi AP Provisioning Example

This example demonstrates zero-touch device provisioning using WiFi AP mode.

## Overview

The device creates a WiFi access point when unconfigured. Users connect to this AP and configure the device via a web interface or mobile app.

## Provisioning Flow

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Device    │     │  Mobile App │     │   Datum     │     │   Device    │
│   (Boot)    │     │             │     │   Server    │     │  (Normal)   │
└──────┬──────┘     └──────┬──────┘     └──────┬──────┘     └──────┬──────┘
       │                   │                   │                   │
       │ No credentials    │                   │                   │
       │ Start AP mode     │                   │                   │
       │ "Datum-Setup-XXX" │                   │                   │
       │◄──────────────────│                   │                   │
       │                   │                   │                   │
       │  User connects    │                   │                   │
       │  to device AP     │                   │                   │
       │◄──────────────────│                   │                   │
       │                   │                   │                   │
       │  GET /info        │                   │                   │
       │──────────────────►│                   │                   │
       │  {uid, model}     │                   │                   │
       │◄──────────────────│                   │                   │
       │                   │                   │                   │
       │                   │ POST /dev/register               │
       │                   │ {uid, name}       │                   │
       │                   │──────────────────►│                   │
       │                   │ {api_key, device_id}                 │
       │                   │◄──────────────────│                   │
       │                   │                   │                   │
       │  POST /configure  │                   │                   │
       │  {server, wifi}   │                   │                   │
       │◄──────────────────│                   │                   │
       │  Save & restart   │                   │                   │
       │                   │                   │                   │
       │───────── Connect to WiFi ─────────────│                   │
       │                   │                   │                   │
       │  POST /provisioning/activate          │                   │
       │  {device_uid}     │                   │                   │
       │──────────────────────────────────────►│                   │
       │  {device_id, api_key}                 │                   │
       │◄──────────────────────────────────────│                   │
       │                   │                   │                   │
       │─────────────────── Normal Operation ──────────────────────►
```

## Hardware Requirements

- ESP32 (any variant: DevKit, WROOM, WROVER, etc.)
- Optional: LED on GPIO2 (built-in on most dev boards)
- Optional: Button on GPIO0 (built-in BOOT button on most dev boards)

## LED Status Indicators

| Pattern | Meaning |
|---------|---------|
| Fast blink (100ms) | Setup mode - waiting for configuration |
| Medium blink (250ms) | Connecting to WiFi |
| Slow blink (500ms) | Activating with server |
| Solid ON | Online and operational |
| Very slow blink (1s) | Offline / Error |

## Factory Reset

Hold the BOOT button (GPIO0) for 5 seconds to perform a factory reset. This clears all saved credentials and restarts the device in setup mode.

## Installation

### Arduino IDE

1. Install ESP32 board support:
   - File → Preferences → Additional Boards Manager URLs
   - Add: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Tools → Board → Boards Manager → Search "ESP32" → Install

2. Install required libraries:
   - Sketch → Include Library → Manage Libraries
   - Search and install: `ArduinoJson` (by Benoit Blanchon)

3. Open `datum_provisioning.ino`

4. Select your board: Tools → Board → ESP32 Dev Module

5. Upload

### PlatformIO

```ini
[env:esp32]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = 
    bblanchon/ArduinoJson@^6.21.0
monitor_speed = 115200
```

## Usage

### First-Time Setup

1. Power on the device
2. Device creates WiFi AP: `Datum-Setup-XXXX` (XXXX = last 4 chars of MAC)
3. Connect your phone/computer to this AP
4. Open browser: `http://192.168.4.1`
5. You'll see the device info and setup form

### Using Mobile App (Recommended)

1. Open Datum mobile app
2. Tap "Add Device"
3. App scans for setup APs
4. Select your device
5. App automatically:
   - Reads device UID
   - Registers with server
   - Configures device

### Manual Setup (Web Interface)

1. Connect to device AP
2. Open `http://192.168.4.1`
3. Enter:
   - Server URL: `https://your-datum-server.com`
   - WiFi SSID: Your home WiFi name
   - WiFi Password: Your WiFi password
4. Click "Configure Device"
5. Device restarts and connects

**Note:** For manual setup, you must first register the device UID with the server via the dashboard or API.

## API Reference

### Device Endpoints (Setup Mode)

#### GET /info

Returns device information for mobile app discovery.

```json
{
    "device_uid": "AABBCCDDEEFF",
    "mac_address": "AA:BB:CC:DD:EE:FF",
    "firmware_version": "1.0.0",
    "model": "datum-sensor-v1",
    "status": "unconfigured"
}
```

#### POST /configure

Configures WiFi and server settings.

**Request:**
```json
{
    "server_url": "https://datum.example.com",
    "wifi_ssid": "MyWiFi",
    "wifi_pass": "password123"
}
```

**Response:** HTML success page, device restarts.

#### GET /status

Returns current device state.

```json
{
    "state": 1,
    "has_credentials": false,
    "is_activated": false,
    "uptime_ms": 12345
}
```

## Customization

### Adding Sensors

Replace the simulated sensor readings in `sendSensorData()`:

```cpp
bool sendSensorData() {
    // ... existing code ...
    
    StaticJsonDocument<256> doc;
    
    // Replace with your actual sensor readings
    doc["temperature"] = readTemperature();  // Your sensor function
    doc["humidity"] = readHumidity();        // Your sensor function
    doc["pressure"] = readPressure();        // Your sensor function
    
    // ... rest of function ...
}
```

### Changing LED Pin

```cpp
#define LED_PIN 2  // Change to your LED pin
```

### Changing Send Interval

```cpp
#define SEND_INTERVAL_MS 60000  // Change to desired interval in ms
```

### Adding More Configuration Options

Extend the setup form in `handleSetupRoot()` and `handleConfigure()` to accept additional parameters.

## Troubleshooting

### Device doesn't create AP

- Check serial output for errors
- Ensure no credentials are saved (factory reset)
- Verify LED is blinking fast

### Can't connect to device AP

- Make sure you're connecting to `Datum-Setup-XXXX`
- Some phones may show "No Internet" warning - ignore it
- Try disabling mobile data temporarily

### Activation fails

- Ensure device is registered via mobile app first
- Check server URL is correct
- Verify server is reachable from your network

### Device keeps restarting

- Check WiFi credentials are correct
- Verify WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
- Check serial output for error messages

## Security Considerations

1. **Setup AP is open** - Anyone nearby can access the setup page. Consider adding password.
2. **Credentials stored in flash** - Use encrypted storage in production.
3. **HTTP in setup mode** - Setup mode uses HTTP, not HTTPS. OK for local network.
4. **Server communication** - Always use HTTPS in production.

## License

MIT License - See repository root for details.
