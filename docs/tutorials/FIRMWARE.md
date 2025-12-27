# Device Firmware Examples

Ready-to-use firmware for connecting IoT devices to Datumpy.

## Available Examples

### 1. Python (Standard/CPython)
**Location:** `tests/device_simulator.py`

```bash
python tests/device_simulator.py \
  --device-id dev_xxx \
  --api-key sk_live_xxx \
  --interval 10
```

**Features:**
- Simulates temperature, humidity, battery sensors
- Sends data at configurable intervals
- Polls and executes commands
- Auto-acknowledges command completion

### 2. Arduino/ESP32
**Location:** `examples/arduino/datumpy_device.ino`

**Requirements:**
- ESP32 or ESP8266 board
- Arduino IDE with ESP32 board package
- ArduinoJson library

**Setup:**
1. Open `datumpy_device.ino` in Arduino IDE
2. Edit configuration section:
   ```cpp
   const char* WIFI_SSID = "your-wifi";
   const char* DEVICE_ID = "dev_xxx";
   const char* API_KEY = "sk_live_xxx";
   ```
3. Upload to board

### 3. MicroPython
**Location:** `examples/micropython/main.py`

**Requirements:**
- ESP32/ESP8266 with MicroPython firmware
- ampy, mpremote, or Thonny for file upload

**Setup:**
1. Edit CONFIG section in `main.py`
2. Upload to device:
   ```bash
   ampy --port /dev/ttyUSB0 put examples/micropython/main.py main.py
   ```

## Device Registration Flow

1. **Create device in Datumpy:**
   ```bash
   curl -X POST http://localhost:8007/devices \
     -H "Authorization: Bearer YOUR_JWT_TOKEN" \
     -H "Content-Type: application/json" \
     -d '{"name": "My Sensor", "type": "temperature"}'
   ```
   
2. **Save the response:**
   ```json
   {"device_id": "dev_xxx", "api_key": "sk_live_xxx"}
   ```

3. **Flash firmware with these credentials**

4. **Device sends data:**
   ```
   POST /data/{device_id}
   Authorization: Bearer {api_key}
   {"temperature": 25.5, "humidity": 60}
   ```

5. **Check for commands in response:**
   ```json
   {"status": "ok", "commands_pending": 1}
   ```

6. **Poll and execute if commands pending**

## Command Types

Devices should handle these standard commands:

| Action | Params | Description |
|--------|--------|-------------|
| `reboot` | `{delay: 5}` | Restart device |
| `set_interval` | `{seconds: 60}` | Change send interval |
| `update_firmware` | `{version: "1.0.1"}` | OTA update |

---

## GET Data Push (Ultra-Constrained Devices)

For devices that cannot make POST requests with JSON bodies, use the GET push endpoint:

### Simple HTTP GET Request
```http
GET /device/{device_id}/push?key={api_key}&temp=25.5&humidity=60&battery=85
```

### Arduino/ESP8266 Example with WiFiClient
```cpp
// Minimal implementation - no JSON library needed
void sendData(float temp, float humidity, int battery) {
  WiFiClient client;
  if (client.connect("your-server.com", 8007)) {
    String url = "/device/" + String(DEVICE_ID) + "/push";
    url += "?key=" + String(API_KEY);
    url += "&temp=" + String(temp, 1);
    url += "&humidity=" + String(humidity, 1);
    url += "&battery=" + String(battery);
    
    client.print("GET " + url + " HTTP/1.1\r\n");
    client.print("Host: your-server.com\r\n");
    client.print("Connection: close\r\n\r\n");
    
    while (client.connected()) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        // Parse response if needed
      }
    }
    client.stop();
  }
}
```

### Benefits for Constrained Devices
- **No JSON parsing needed** - simpler code, smaller binary
- **Single HTTP GET** - compatible with basic HTTP libraries
- **Auto type detection** - numbers, booleans, and strings parsed automatically
- **same authentication** - uses `key` query parameter
