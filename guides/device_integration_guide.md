# Datum IoT Device Integration Guide

This guide explains how to integrate a new IoT device (ESP32, ESP8266, or generic Linux/Python client) with the Datum Server.

## 1. Overview

The Datum Protocol is a **polling-based** IoT protocol designed for simplicity and reliability behind firewalls/NATs.
Devices initiate all connections to the server (HTTP POST/GET).

**Core Loop:**
1.  **Boot**: Register or Activate (Get API Key).
2.  **Heartbeat (60s)**: Send JSON telemetry.
3.  **Command Poll (5s)**: Check for pending actions from User.
4.  **Action**: Execute command and Acknowledge.

---

## 2. Authentication

All requests after provisioning MUST include the **API Key** in the header:
```http
Authorization: Bearer <YOUR_DEVICE_API_KEY>
Content-Type: application/json
```

---

## 3. Provisioning (Registration)

To connect a new device, you need a **User Token** (obtained via `POST /auth/login`).

### Endpoint: `POST /device`
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
  "id": "dev_123456789",
  "api_key": "dk_987654321...",
  "status": "created"
}
```
*Save `id` and `api_key` to non-volatile storage (EEPROM/NVS).*

---

## 4. Telemetry (Heartbeat)

Devices should report their state every 60 seconds.

### Endpoint: `POST /data/{device_id}`
**Auth Header:** `Authorization: Bearer <DEVICE_API_KEY>`

**Payload (Standard):**
```json
{
  "rssi": -55,
  "uptime": 120,
  "free_heap": 45000,
  "status": "online",
  "local_ip": "192.168.1.105",
  
  // Custom Device State (e.g. Camera)
  "led_on": true,
  "resolution": "UXGA"
}
```

**Optimization Tip:**
Send static data (Firmware Version, Reset Reason) only once at **Boot**:
```json
{
  ... standard fields ...,
  "fw_ver": "1.0.0",
  "reset_reason": "Power On"
}
```

---

## 5. Command Handling

Devices should poll for commands every 1-5 seconds.

### 5.1 Poll for Commands
**Endpoint:** `GET /commands/{device_id}/poll`
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

### 5.2 Execute & Acknowledge
After executing the action, you **MUST** acknowledge the command so it doesn't queue up again.

**Endpoint:** `POST /commands/{device_id}/ack`
**Payload:**
```json
{
  "command_id": "cmd_abc123"
}
```

---

## 6. Binary Uploads (Snapshots)

To upload generic files or images.

**Endpoint:** `POST /data/{device_id}/upload`
**Header:** `Authorization: Bearer <DEVICE_API_KEY>`
**Content-Type:** `multipart/form-data`

**Form Field:**
*   `file`: The binary data.

---

## 7. Example Firmware Logic (Pseudo-code)

```cpp
void loop() {
  if (millis() - lastHeartbeat > 60000) {
    sendTelemetry(); // POST /data/{id}
    lastHeartbeat = millis();
  }

  if (millis() - lastPoll > 5000) {
    checkCommands(); // GET /commands/{id}/poll
    lastPoll = millis();
  }
}

void checkCommands() {
  String json = httpGet("/commands/" + id + "/poll");
  if (json.has("commands")) {
    foreach (cmd in commands) {
      if (cmd.action == "led") toggleLed();
      if (cmd.action == "restart") restart();
      
      httpPost("/commands/" + id + "/ack", { "command_id": cmd.id });
    }
  }
}
```
