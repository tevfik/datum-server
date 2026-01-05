# ESP32 Self-Registration Example (HTTP)

This example demonstrates the **Self-Registration** pattern, where a device automatically registers itself with the Datum Server upon first boot using a User Token.

## Overview

Unlike the [Provisioning Example](../esp32_provisioning/README.md), which requires a mobile app to send credentials, this example assumes the device is pre-configured with WiFi credentials and a User Token (e.g., during manufacturing or hardcoded for prototyping).

When the device boots:
1.  It connects to WiFi.
2.  It checks if it is already registered (has an API Key validation).
3.  If not, it calls the **Register Endpoint** (`POST /devices/register`) using the User Token.
4.  It saves the returned **Device ID** and **API Key** to permanent storage (NVS).
5.  It switches to normal operation, sending telemetry using its own API Key.

## Registration Flow

```
┌─────────────┐                                   ┌─────────────┐
│    Device   │                                   │   Datum     │
│             │                                   │   Server    │
└──────┬──────┘                                   └──────┬──────┘
       │                                                 │
       │ 1. Connect to WiFi                              │
       │────────────────────────────────────────────────►│
       │                                                 │
       │ 2. Check NVS for API Key                        │
       │    (Not found?)                                 │
       │                                                 │
       │ 3. Self-Register                                │
       │    POST /devices/register                       │
       │    Auth: Bearer {USER_TOKEN}                    │
       │    {uid, name, type}                            │
       │────────────────────────────────────────────────►│
       │                                                 │
       │    200 OK                                       │
       │    { "device_id": "...", "api_key": "..." }     │
       │◄────────────────────────────────────────────────│
       │                                                 │
       │ 4. Save Credentials to NVS                      │
       │                                                 │
       │ 5. Send Telemetry                               │
       │    POST /data/{device_id}                       │
       │    Auth: Bearer {API_KEY}                       │
       │    { "temp": 25.5 ... }                         │
       │────────────────────────────────────────────────►│
       │                                                 │
```

## Prerequisites

1.  **Datum Server**: Running and accessible.
2.  **User Token**: You must generate a JWT token for your user account.
    *   **CLI**: Run `datumctl login` and check `~/.datumctl.yaml`.
    *   **Web/App**: Login and inspect network requests (or use a dedicated token generation tool).
3.  **ESP32 Board**: Any standard ESP32 development board.

## Configuration

Open `esp32_self_register.ino` and update the **Configuration Section** at the top:

```cpp
// ============ CONFIGURATION ============
const char *wifiSSID = "YOUR_WIFI_SSID";       // Your WiFi Name
const char *wifiPass = "YOUR_WIFI_PASS";       // Your WiFi Password
const char *userToken = "YOUR_USER_TOKEN";     // Your JWT User Token
const char *serverURL = "http://192.168.1.100:8080"; // Your Server URL
const char *deviceName = "My Self-Reg Device"; // Desired Device Name
```

> **Security Warning**: Hardcoding WiFi passwords and User Tokens is for **prototyping only**. For production, use the Provisioning workflow or burn credentials securely during manufacturing.

## Code Explanation

### 1. Device Identity (`initDeviceUID`)
The device generates a unique ID based on its factory MAC address. This ensures that even if you flash the same code to 100 devices, they will all register as unique entities.

```cpp
uint8_t mac[6];
esp_efuse_mac_get_default(mac);
// Result: "A1B2C3D4E5F6"
```

### 2. Registration Logic (`registerDevice`)
This function constructs a JSON payload with the device's identity and sends it to the server.

```cpp
// Endpoint: POST /devices/register
http.begin(String(serverURL) + "/devices/register");
http.addHeader("Authorization", "Bearer " + String(userToken));

// Payload
doc["device_uid"] = deviceUID;   // Unique Hardware ID
doc["device_name"] = deviceName; // Friendly Name
doc["device_type"] = "sensor";
```

### 3. Persistent Storage (`Preferences`)
The `saveCredentials` function stores the server-assigned credentials in the ESP32's Non-Volatile Storage (NVS). This survives reboots and power cycles.

```cpp
void saveCredentials(String id, String key) {
  prefs.begin("datum", false); // Namespace "datum"
  prefs.putString("device_id", id);
  prefs.putString("api_key", key);
  prefs.end();
}
```

## Usage

1.  **Configure**: Update the credentials in the `.ino` file.
2.  **Upload**: Flash the code to your ESP32.
3.  **Monitor**: Open the Serial Monitor (115200 baud).

**Expected Output (First Run):**
```
Device UID: 246F28AABBCC
Connecting to WiFi...... Connected!
Attempting Self-Registration...
Register Response: 200
{"device_id":"device-123","api_key":"dk_xxxxx"}
Registration Successful! Credentials Saved.
Telemetry Push: 200
```

**Expected Output (Subsequent Runs):**
```
Device UID: 246F28AABBCC
Connecting to WiFi...... Connected!
Device already registered. ID: device-123
Telemetry Push: 200
```

## Comparison: Self-Register vs Provisioning

| Feature | Self-Registration | WiFi Provisioning |
| :--- | :--- | :--- |
| **Connectivity** | WiFi Creds Hardcoded | Sent via Mobile App (BLE/SoftAP) |
| **Auth** | User Token Hardcoded | Generated/Exchanged dynamically |
| **User Interaction** | Zero-Touch (Automatic) | User claims device via App |
| **Use Case** | Fleet simulations, Industrial (pre-configured) | Consumer products, Smart Home |
