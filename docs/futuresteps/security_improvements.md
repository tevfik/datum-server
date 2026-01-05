# Security Improvements Proposal

This document outlines strategic security enhancements for the Datum IoT Platform, as requested for future discussion.

## 1. Device Communication Security (Immediate Priority)

### Problem
*   Firmware currently uses `client.setInsecure()`, bypassing SSL certificate validation.
*   This leaves devices vulnerable to Man-in-the-Middle (MITM) attacks.

### Proposal
1.  **Enforce SSL Verification**:
    *   Embed the Root CA certificate (e.g., Let's Encrypt ISRG Root X1) in the firmware.
    *   Use `client.setCACert()` instead of `setInsecure()`.
2.  **Implications**:
    *   Firmware OTA updates must also be over HTTPS with verification.
    *   Certificate rotation strategy needed (if Root CA expires).

## 2. Device Credential Storage

### Problem
*   WiFi passwords, API Keys, and User Tokens are stored in plaintext in ESP32/ESP8266 EEPROM/NVS.
*   Physical access to the device allows dumping these secrets.

### Proposal
1.  **Flash Encryption (ESP32)**:
    *   Enable ESP32 Flash Encryption and Secure Boot. This encrypts the filesystem and prevents unauthorized firmware modifications.
2.  **NVS Encryption**:
    *   Use the ESP-IDF NVS Encryption feature to store keys.

## 3. Backend Token Management

### Problem
*   Current implementation uses a custom HMAC-based token scheme (`internal/auth/token.go`).
*   While functional, custom crypto is difficult to audit and integrate with API Gateways.

### Proposal
1.  **Migrate to Standard JWT**:
    *   Use standard libraries (`golang-jwt`).
    *   Embed `deviceID` and `role` in the JWT claims.
    *   Sign with RS256 (Asymmetric) or HS256 (Symmetric).
2.  **Token Rotation**:
    *   Implement Refresh Tokens to keep Access Token lifespan short (e.g., 1 hour).

## 4. Provisioning Security

### Problem
*   Relay Board uses an Open WiFi network (`Datum-Relay-XXXX`) for provisioning.

### Proposal
1.  **WPA2-PSK with Dynamic Password**:
    *   Generate a random password on first boot (e.g., print to Serial or display on screen if available).
    *   OR, fail-safe: Use "PoP" (Proof of Possession) code printed on the device label.
2.  **SoftAP Encryption**:
    *   Re-enable SoftAP password, but make it user-configurable or unique per device.

## 5. API Rate Limiting

### Proposal
*   Implement strict rate limiting per IP and per API Key to prevent DoS attacks, especially on Polling endpoints.
