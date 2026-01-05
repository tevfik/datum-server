# ESP32 Self-Registration (HTTP)

This example demonstrates the **Self-Registration** flow using standard HTTP.

It is minimal and focuses on:
1.  Connecting to WiFi (Hardcoded credentials for simplicity).
2.  Using a **User Token** to call `POST /devices/register`.
3.  Saving the returned **API Key** to NVS.
4.  Sending data using the API Key.

## Usage

1.  Open `esp32_self_register.ino`.
2.  Update `wifiSSID`, `wifiPass`, and `userToken`.
3.  Upload to ESP32.
4.  Monitor Serial Output.
