# Firmware & Device Management Roadmap

## 1. Secure OTA Updates
Current OTA checks URL and downloads binary. Security needs enhancement.

### Plan
1.  **Image Signing**:
    - Use `ed25519` to sign firmware binaries before upload.
    - Firmware validates signature against public key before flashing.
2.  **Rollback Protection**:
    - Store version number in NVS. Refuse to downgrade unless "Force" flag is set.
    - A/B Partitioning (already standard in ESP32) - Ensure fallback works if new FW crashes.

## 2. Delta Updates
Reduce bandwidth costs for GSM/LTE connected devices.

### Plan
1.  **Binary Diffing**: Use `bsdiff` or specific MCU delta tools.
2.  **Server Side**: Generate `.patch` file when new FW is uploaded vs previous version.
3.  **Device Side**: Apply patch to running partition -> Write to Next partition.

## 3. Remote Configuration (Shadow Twin)
Currently, configuration is mostly static or custom commands.

### Plan
1.  **Device Shadow**:
    - Store `desired_config` and `reported_config` in DB.
    - MQTT Topic: `devices/{id}/config/set` (Server -> Device).
    - MQTT Topic: `devices/{id}/config/status` (Device -> Server).
2.  **UI Editor**: generated JSON Schema form for editing config in Web Dashboard.
