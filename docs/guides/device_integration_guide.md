# Datum IoT Device Integration Guide

This guide explains how to integrate a new IoT device (ESP32, ESP8266, or generic Linux/Python client) with the Datum Server.

## 1. Overview

The Datum Protocol is a **Hybrid (HTTP + MQTT)** IoT protocol designed for flexibility, reliability, and real-time control.

-   **HTTP**: Used for Provisioning, Firmware OTA, Large File Uploads, and Fallback/Sync.
-   **MQTT**: Used for Real-time Telemetry, Instant Commands, and Live Config Updates.

**Core Loop (Hybrid):**
1.  **Boot**: Connect WiFi -> Fetch Remote Config (HTTP/Pull).
2.  **Connect MQTT**: Subscribe to `dev/{id}/cmd`, `dev/{id}/conf/set`.
3.  **Run**: Publish telemetry to `dev/{id}/data`.
4.  **React**: Handle incoming MQTT messages immediately.

---

## 2. Authentication

All requests must differentiate based on the protocol:

**HTTP Headers:**
```http
Authorization: Bearer <YOUR_DEVICE_API_KEY>
Content-Type: application/json
```

**MQTT Credentials:**
-   **ClientID**: `<YOUR_DEVICE_ID>`
-   **Username**: `<YOUR_DEVICE_ID>`
-   **Password**: `<YOUR_DEVICE_API_KEY>`

---

## 3. Provisioning (Registration)

To connect a new device, you need a **User Token** (obtained via `POST /auth/login`).

### Endpoint: `POST /dev`
**Payload:**
```json
{
  "device_uid": "mac_address_or_unique_id",
  "name": "My Living Room Camera",
  "type": "camera"
}
```
**Auth Header:** `Authorization: Bearer <USER_TOKEN>`

**Response (Success 200/201):**
```json
{
  "device_id": "dev_123456789",
  "api_key": "dk_987654321...",
  "message": "Save this API key - it won't be shown again"
}
```
*Save `device_id` and `api_key` to non-volatile storage.*

---

## 4. Configuration Management (Shadow Twin)

Datum uses a "Shadow Twin" model where the server stores a `desired_state` and the device reports its actual state.

### 4.1. Boot Sync (HTTP Pull)
On boot, fetching the latest config ensures the device is in sync even if it missed updates while offline.
**Endpoint:** `GET /dev/{device_id}`
**Response:**
```json
{
  "id": "...",
  "desired_state": {
    "resolution": "1080p",
    "led_color": "#FF0000",
    "interval": 60
  }
}
```

### 4.2. Real-time Updates (MQTT Push)
Subscribe to: `dev/{device_id}/conf/set`
**Payload:**
```json
{
  "desired": {
    "resolution": "720p"
  },
  "timestamp": 1234567890
}
```
*Action: Update local setting immediately and persist if necessary.*

---

## 5. Telemetry

### 5.1. Via MQTT (Preferred)
**Topic:** `dev/{device_id}/data`
**Payload:**
```json
{
  "temp": 23.5,
  "hum": 45,
  "status": "active"
}
```

### 5.2. Via HTTP (Legacy/Fallback)
**Endpoint:** `POST /dev/{device_id}/data`
**Payload:** Same as MQTT.

---

## 6. Command Handling

### 6.1. Via MQTT (Instant)
**Subscribe:** `dev/{device_id}/cmd`
**Payload:**
```json
{
  "id": "cmd_123",
  "action": "reboot",
  "params": {}
}
```

### 6.2. Via HTTP (Polling)
**Endpoint:** `GET /dev/{device_id}/cmd/pending`
**Auth Header:** `Authorization: Bearer <DEVICE_API_KEY>`

**Response:**
```json
{
  "commands": [
    {
      "id": "cmd_abc123",
      "action": "led",
      "params": { "state": "on" }
    }
  ]
}
```

### 6.3. Execute & Acknowledge
After executing the action, you **MUST** acknowledge the command so it doesn't queue up again.

**Endpoint:** `POST /dev/{device_id}/cmd/{command_id}/ack`
**Payload:**
```json
{
  "result": "success"
}
```

---

## 7. Binary Uploads (Snapshots)

To upload generic files or images (e.g. MJPEG frames).

**Endpoint:** `POST /dev/{device_id}/stream/frame`
**Header:** `Authorization: Bearer <DEVICE_API_KEY>`
**Content-Type:** `image/jpeg` (Raw Binary) OR `application/json` (Base64)

**Option A: Raw Binary (Preferred for speed)**
Send raw JPEG bytes in the body.

**Option B: JSON Base64**
Payload:
```json
{
  "image": "base64_encoded_string...",
  "format": "jpeg"
}
```

---

## 8. Example Firmware Logic (Pseudo-code)

```cpp
void loop() {
  if (millis() - lastHeartbeat > 60000) {
    sendTelemetry(); // POST /dev/{id}/data
    lastHeartbeat = millis();
  }

  if (millis() - lastPoll > 5000) {
    checkCommands(); // GET /dev/{id}/cmd/pending
    lastPoll = millis();
  }
}

void checkCommands() {
  String json = httpGet("/dev/" + id + "/cmd/pending");
  if (json.has("commands")) {
    foreach (cmd in commands) {
      if (cmd.action == "led") toggleLed();
      if (cmd.action == "restart") restart();
      
      httpPost("/dev/" + id + "/cmd/" + cmd.id + "/ack", { "result": "success" });
    }
  }
}
```
