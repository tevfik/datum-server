# ESP32 Static Authentication Example

This example demonstrates the simplest way to authenticate a device with the Datum Platform using a **Static API Key**.

## Overview
In this method, the device uses a permanent API Key (`sk_...` or `dk_...`) that does not expire. 
- **Pros**: Simple to implement.
- **Cons**: If the key is stolen, it must be manually compromised and replaced on the device.

## Prerequisites
1. **Registered Device**: You must have a device registered on your Datum Server.
2. **Device ID & API Key**: Obtain these from the Datum Mobile App or Admin Console.

## Setup
1. Open `esp32_auth_static.ino` in Arduino IDE.
2. Update the **CONFIGURATION** section:
   ```cpp
   const char* wifiSSID = "YOUR_WIFI_SSID";
   const char* wifiPass = "YOUR_WIFI_PASS";
   const char* deviceID = "YOUR_DEVICE_ID";
   const char* apiKey   = "sk_YOUR_STATIC_KEY";
   const char* serverURL = "http://YOUR_SERVER_IP:8080";
   ```
3. Flash to your ESP32.

## How it works

The device uses a **Static Key** (`sk_...`) stored in its firmware or NVS. This key is sent in every request header.

```ascii
+---------+                    +--------------+
|  ESP32  |                    | Datum Server |
+----+----+                    +-------+------+
     |                                 |
     | POST /dev/dev-1/data (Auth: sk_...) |
     +-------------------------------->|
     |                                 |
     |         200 OK (Saved)          |
     |<--------------------------------+
     |                                 |
```

For more details on key types, see [KEY_MANAGEMENT.md](../../docs/guides/KEY_MANAGEMENT.md).

