# ESP32 MQTT Sensor with Self-Registration

This example demonstrates a modern IoT device workflow using the Datum Platform.
It simulates a Temperature/Humidity sensor.

## 🚀 Features

1.  **Zero-Touch Provisioning**: Creates a WiFi Hostspot (`Datum-Sensor-XXXX`) to configure WiFi.
2.  **Self-Registration**: The user provides their **User Token** during setup. The device uses this to register itself and obtain its own API Key automatically.
3.  **MQTT Connectivity**:
    *   Publishes data to `data/{device_id}`.
    *   Listens for commands on `commands/{device_id}`.
    *   Authenticates using Device ID + API Key.

## 🛠 Hardware
*   ESP32 (DevKit, WROOM, etc.)
*   (Optional) LED on GPIO 2.

## 📦 Libraries Required
*   `ArduinoJson`
*   `PubSubClient`

## 📲 Setup Guide

1.  **Flash Firmware**: Compile and upload to ESP32.
2.  **Connect to Hotspot**: Connect your phone/PC to WiFi `Datum-Sensor-XXXX`.
3.  **Open Browser**: Go to `http://192.168.4.1`.
4.  **Configure**:
    *   **Server URL**: `https://datum.bezg.in` (or your local server IP).
    *   **WiFi SSID/Pass**: Your network credentials.
    *   **User Token**: Get this from your User Profile page on Datum.
    *   **Device Name**: e.g., "Living Room Sensor".
5.  **Save**: The device will restart, connect to WiFi, register itself, and start sending data via MQTT.

## 📡 MQTT Data Format

**Telemetry:**
```json
{
  "temperature": 24.5,
  "humidity": 55.2,
  "rssi": -60,
  "uptime": 120
}
```

**Commands Supported:**
*   `restart`: Reboots the device.
*   `led`: Toggles the LED.
