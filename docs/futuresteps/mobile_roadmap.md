# Mobile Application & Backend Roadmap

## 1. Push Notifications (FCM Integration)
Currently, alerts are delivered via Email and MQTT. To support real-time mobile alerts even when the app is backgrounded, efficient Push Notification support is needed.

### Plan
1.  **Firebase Project Setup**: Configure FCM for Android/iOS.
2.  **Backend Integration**:
    - Add `firebase-admin-go` SDK to `datum-server`.
    - Create `internal/push` package.
    - Store `FCM Token` per Device/Session in `users` or `mobile_sessions` table.
3.  **Trigger logic**:
    - Update `internal/processing/processor.go` to trigger Push when Alarm Rule matches.
4.  **API Endpoint**:
    - `POST /api/mobile/fcm-token`: Register device token from Flutter app.

## 2. Offline Mode & Sync (Backend Support)
The Mobile App has local storage (Isar/SQL). The backend needs a Sync API to handle data queued while offline.

### Plan
1.  **Sync Endpoint**: `POST /api/v1/sync`
    - Accepts a batch of operations (JSON array).
    - Returns success/failure per operation.
2.  **Conflict Resolution**:
    - Implement "Last-Write-Wins" based on `timestamp`.
    - Return updated state if server has newer data.
3.  **Data Versioning**:
    - Add `version` column to critical entities (Devices, Rules) to allow efficient delta downloads (`GET /dev?since_version=102`).

## 3. WebRTC Streaming
For ESP32-Cam and other camera devices, MJPEG is inefficient over WAN.

### Plan
1.  **Signaling Server**: Implement WebRTC signaling over WebSocket (`/ws/signaling`).
2.  **ICE/TURN Servers**: Deploy local coturn container or use public STUN.
3.  **Device Side**: Update ESP32 firmware to support WebRTC (using `arduino-esp32` webrtc lib).
