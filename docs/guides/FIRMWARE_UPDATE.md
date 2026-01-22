# Firmware Update Guide (OTA)

Datum Server supports **Over-The-Air (OTA)** firmware updates for ESP32 devices. This allows you to update devices remotely without physical access.

## Prerequisites

1.  **Datum Server** running (v1.0.0+).
2.  **Mount Configuration**: The server requires the `firmware/` directory to be mounted.
    *   Docker: Ensure `./firmware:/root/firmware` (or similar) volume mapping exists.
3.  **ESP32 Device**: Must be running firmware that supports the `update_firmware` command (e.g., `esp32-s3-camera`).

## Security Model

*   **Protected Access**: The `/dev/fw/:filename` endpoint is protected by the `DeviceAuthMiddleware`.
*   **Token Authentication**: Devices must authenticate using their `auth_token`. Since ESP32's `HTTPUpdate` library has limitations with custom headers, authentication is done via a query parameter:
    *   URL: `https://your-server.com/dev/fw/my_fw.bin?token=dk_...`
*   **Targeted Update**: The update command is sent to a specific Device ID. Only that device receives the command and initiates the download using its own token.

## Step-by-Step Procedure

### 1. Compile the New Firmware
In Arduino IDE or PlatformIO:
1.  Make your code changes.
2.  Increment the `VERSION` variable in your code (e.g., `1.0.1`) so you can verify the update.
3.  **Export Compiled Binary**:
    *   **Arduino IDE**: Sketch -> Export Compiled Binary (`Ctrl+Alt+S`).
    *   It will create a `.bin` file in your sketch folder (e.g., `esp32_camera_streamer.ino.esp32s3.bin`).

### 2. Rename and Upload
1.  Rename the binary to something simple, e.g., `fw_v1.0.1.bin`.
2.  Upload/Copy this file to the `firmware/` folder on your server host machine.
    ```bash
    cp fw_v1.0.1.bin ~/scripts/services/datum-server/firmware/
    ```

### 3. Trigger the Update
Send a command to the target device to tell it to download this specific file.

**Option A: Using HTTP API (curl)**
```bash
curl -X POST https://datum.bezg.in/dev/<DEVICE_ID>/cmd \
  -H "Authorization: Bearer <USER_TOKEN>" \
  -H "Content-Type: application/json" \
  -d '{
    "action": "update_firmware",
    "params": {
        "url": "https://datum.bezg.in/dev/fw/fw_v1.0.1.bin"
    }
}'
```

**Option B: Using MQTT**
Publish to topic `dev/<DEVICE_ID>/cmd`:
```json
{
  "id": "ota_cmd_1",
  "action": "update_firmware",
  "params": {
    "url": "https://datum.bezg.in/dev/fw/fw_v1.0.1.bin"
  }
}
```

**Note**: You DO NOT need to append `?token=...` to the URL parameter. The device firmware logic should automatically append its current auth token to the URL before downloading.

### 4. Verify
1.  The device will receive the command.
2.  Logs will show `Starting OTA Update...` and `Firmware URL: ...?token=...`.
3.  The device will download the file, flash it, and reboot.
4.  Once back online, check the logs or device info to confirm the new version.

## Troubleshooting

*   **HTTP_UPDATE_FAILED Error (-1)**: HTTP Error.
    *   Check if the URL is reachable from the network.
    *   Check server logs: "Firmware not found" (404) or "Unauthorized" (401).
*   **Magic Header Error**: The file is not a valid ESP32 binary. Make sure you exported the correct `.bin` file.
*   **Space Issues**: Ensure the destination partition (OTA/app partition) has enough space.
