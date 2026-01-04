# ESP8266 Relay Board Example

This example demonstrates how to integrate a generic ESP8266 Relay Board (e.g., ESP-01S Relay or NodeMCU Relay Shield) with the **Datum IoT Platform**.

It features:
- **SoftAP Provisioning**: Setup WiFi via the Datum Camera App (or any browser).
- **Datum Client**: Automatically registers with the server and reports relay states.
- **Remote Control**: Toggles relays via Datum API (and Mobile App).
- **OTA Updates**: Supports remote firmware updates.

## Hardware Setup

This firmware is designed for generic ESP8266 boards controlling relays via GPIOs.

**Default Pin Configuration:**
| Helper Constant | GPIO | Description |
|-----------------|------|-------------|
| `GPIO_RELAY_0`  | 16   | Relay 1 (Active Low*) |
| `GPIO_RELAY_1`  | 5    | Relay 2 |
| `GPIO_RELAY_2`  | 4    | Relay 3 |
| `GPIO_RELAY_3`  | 0    | Relay 4 |
| `ADC_BATTERY`   | A0   | Voltage Monitor |

*> Note: Many relay specific boards (like ESP-01S Relay) use **GPIO 0**. Be careful as this pin determines boot mode. The firmware defaults key relays to safe pins, but you may need to adjust `includes.h` for your specific hardware.*

### Common Boards
- **ESP-01S Relay Module**: Uses `GPIO 0`.
- **NodeMCU / WeMos D1 Mini Relay Shield**: Usually `D1 (GPIO 5)`.

**To Change Pins:**
Edit `includes.h`:
```cpp
#define GPIO_RELAY_0 5 // Example for WeMos D1 Mini Shield
```

## Flashing Instructions

### Prerequisites
- [Arduino IDE](https://www.arduino.cc/en/software)
- **ESP8266 Board Package**: Installed via Board Manager.
- **Libraries**:
    - `ArduinoJson` (v6+)

### Steps
1.  Open `datum_relay_board.ino` in Arduino IDE.
2.  Select your board (e.g., "Generic ESP8266 Module" or "NodeMCU 1.0").
3.  **Flash** the firmware.

## Onboarding (Provisioning)

1.  **Power on** the device.
2.  If WiFi is not configured (or connection fails), it enters **Provisioning Mode**.
3.  **LED Status**: (If applicable) usually blinks or stays solid depending on board.
4.  **Connect via Mobile App**:
    -   Open **Datum Camera App**.
    -   Go to **Add Device**.
    -   Follow the wizard. It will connect to the device's SoftAP (`Datum-Relay-XXXX`), send credentials, and link it to your account.

### Manual Provisioning
You can also provision manually without the app:
1.  Connect your phone/laptop to WiFi: `Datum-Relay-XXXX` (Pass: `datum123`).
2.  Open browser to `http://192.168.4.1/info` to verify connection.
3.  **Configure**:
    Send a POST request to `http://192.168.4.1/configure`:
    ```json
    {
        "wifi_ssid": "YOUR_WIFI",
        "wifi_pass": "YOUR_PASS",
        "user_token": "YOUR_DATUM_USER_TOKEN",
        "server_url": "https://your-datum-server.com"
    }
    ```

## Usage

Once connected, the device will:
1.  Register with the server (creates a new Device in your account).
2.  Appear in your Mobile App / CLI list.
3.  Report relay states every 30 seconds.

### Control Relays (CLI)

```bash
# List devices to find ID
datumctl device list

# Turn Relay 0 ON
datumctl device command <DEVICE_ID> relay_control --params '{"relay_index": 0, "state": true}'
```

### Control Relays (API)

**POST** `/devices/<DEVICE_ID>/commands`
```json
{
  "type": "relay_control",
  "params": {
    "relay_index": 0, 
    "state": true
  }
}
```
