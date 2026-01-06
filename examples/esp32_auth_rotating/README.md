# ESP32 Rotating Authentication Example (SAS-like)

This example demonstrates the advanced **Rotating Token** (SAS-like) authentication method.

## Overview
In this method, the device holds a **Master Secret** but never sends it over the network. Instead, it generates short-lived, cryptographically signed Access Tokens.
- **Pros**: High Security. Master Secret is safe. Tokens expire automatically.
- **Cons**: Requires time synchronization (NTP) and slightly more complex code (HMAC).

## Logic
1. Device Syncs Time via NTP.
2. Device calculates `expiry` (e.g. Current Time + 7 days).
3. Payload = `deviceID` + `:` + `expiryUnix`.
4. Signature = `HMAC-SHA256(Payload, MasterSecret)`.
6. Device sends HTTP POST with `Authorization: Bearer dk_...`.

## Rotating Auth Flow

```ascii
+---------+                                   +--------------+
|  ESP32  |                                   | Datum Server |
+----+----+                                   +-------+------+
     |                                                |
     | 1. Time Sync (NTP)                             |
     +---(Self)----> [Time OK]                        |
     |                                                |
     | 2. Generate Token (HMSC-SHA256)                |
     |    "dk_{expiry}.{signature}"                   |
     |                                                |
     | 3. POST /data/dev-1 (Auth: dk_...)             |
     +----------------------------------------------->|
     |                                                | 4. Validate Token
     |                                                |    (Check Sign & Expiry)
     |         200 OK (Saved)                         |
     |<-----------------------------------------------+
     |                                                |
```

For more details on key types, see [KEY_MANAGEMENT.md](../../docs/guides/KEY_MANAGEMENT.md).


## Setup
1. Open `esp32_auth_rotating.ino` in Arduino IDE.
2. Update the **CONFIGURATION** section:
   ```cpp
   const char* wifiSSID = "YOUR_WIFI_SSID";
   const char* wifiPass = "YOUR_WIFI_PASS";
   const char* deviceID = "YOUR_DEVICE_ID";
   const char* masterSecret = "YOUR_MASTER_SECRET"; // Keep this safe!
   const char* serverURL = "http://YOUR_SERVER_IP:8080";
   ```
3. Flash to your ESP32.

## Dependencies
This example uses `mbedtls` which is included by default in the ESP32 Arduino Core. No external libraries are needed for crypto.
