/*
 * Datum IoT Platform - ESP32 Rotating Authentication Example (SAS-like)
 *
 * DESCRIPTION:
 * This example demonstrates the advanced "Rotating Token" authentication.
 * Instead of sending the Master Secret over the wire, the device computes
 * a temporary, time-limited Access Token.
 *
 * SECURITY LEVEL:
 * High. The Master Secret never leaves the device's secure storage.
 * Tokens expire automatically (e.g., in 7 days).
 * Even if a token is intercepted, it is only valid for a limited time.
 *
 * REQUIRES:
 * - mbedtls (Standard in ESP32 SDK)
 */

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <mbedtls/md.h>
#include <time.h>

// ============ CONFIGURATION ============
const char *wifiSSID = "YOUR_WIFI_SSID";
const char *wifiPass = "YOUR_WIFI_PASS";

// Device credentials
const char *deviceID = "YOUR_DEVICE_ID";
// Master Secret is critical! Never expose this.
// In production, store this in Encrypted NVS or Secure Element.
const char *masterSecret = "YOUR_MASTER_SECRET_BASE64";

const char *serverURL = "http://YOUR_SERVER_IP:8000";

// Token Settings
const int TOKEN_VALIDITY_DAYS = 1; // Short life for demo

// Globals
String currentToken;
time_t tokenExpiresAt = 0;

// NTP Server for time (Required for token generation)
const char *ntpServer = "pool.ntp.org";

// Sync Time via HTTP (Christian's Algorithm)
bool syncTimeViaHttp() {
  HTTPClient http;
  long t_start = millis();

  // Note: /system/time returns { "unix": ..., "unix_ms": ... }
  http.begin(String(serverURL) + "/system/time");
  int code = http.GET();

  if (code == 200) {
    String resp = http.getString();
    long t_end = millis();

    DynamicJsonDocument doc(512);
    deserializeJson(doc, resp);

    // Use 64-bit int for milliseconds
    uint64_t server_ms = doc["unix_ms"].as<uint64_t>();

    long rtt = t_end - t_start;
    uint64_t adjusted_ms = server_ms + (rtt / 2);

    struct timeval tv;
    tv.tv_sec = adjusted_ms / 1000;
    tv.tv_usec = (adjusted_ms % 1000) * 1000;
    settimeofday(&tv, NULL);

    Serial.printf("Time synced via HTTP! RTT: %ld ms\n", rtt);
    return true;
  }
  Serial.printf("HTTP Time Sync Failed: %d\n", code);
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Connect to WiFi
  WiFi.begin(wifiSSID, wifiPass);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  // Init Time (Critical for SAS Tokens)
  // 1. Try NTP
  configTime(0, 0, ntpServer);
  Serial.print("Waiting for NTP time");

  int retries = 0;
  while (time(nullptr) < 1000000000 && retries < 10) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  // 2. Fallback to HTTP if NTP failed
  if (time(nullptr) < 1000000000) {
    Serial.println("\nNTP Timeout. Trying HTTP Fallback...");
    if (!syncTimeViaHttp()) {
      Serial.println("FATAL: Could not sync time. Auth will fail.");
    }
  } else {
    Serial.println("\nTime synced via NTP!");
  }
}

// Generate HMAC-SHA256 Signature
String hmacSHA256(String payload, String secret) {
  byte hmacResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

  const size_t payloadDim = payload.length();

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char *)secret.c_str(),
                         secret.length());
  mbedtls_md_hmac_update(&ctx, (const unsigned char *)payload.c_str(),
                         payloadDim);
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);

  // Convert to Hex
  String hexStr = "";
  for (int i = 0; i < 32; i++) {
    if (hmacResult[i] < 16)
      hexStr += "0";
    hexStr += String(hmacResult[i], HEX);
  }
  return hexStr;
}

// Generate SAS Token: dk_{expiry}.{signature}
void rotateToken() {
  time_t now;
  time(&now);

  // Calculate Expiry
  time_t expiry = now + (TOKEN_VALIDITY_DAYS * 24 * 3600);

  // Payload: deviceID:expiryUnix (Must match server logic)
  String payload = String(deviceID) + ":" + String(expiry);

  // Sign
  String signature = hmacSHA256(payload, String(masterSecret));

  // Truncate signature to first 24 chars (Standardized in Datum)
  String truncatedSig = signature.substring(0, 24);

  // Format Token
  // Format: dk_{expiry}.{signature}
  currentToken = "dk_" + String(expiry) + "." + truncatedSig;
  tokenExpiresAt = expiry;

  Serial.println("Token Rotated!");
  Serial.println("New Token: " + currentToken);
  Serial.println("Expires At: " + String(expiry));
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    time_t now;
    time(&now);

    // Check if we need a token (or refresh if < 1 hour left)
    if (currentToken.length() == 0 || (tokenExpiresAt - now) < 3600) {
      rotateToken();
    }

    HTTPClient http;
    String url = String(serverURL) + "/dev/" + deviceID + "/data";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + currentToken);

    DynamicJsonDocument doc(128);
    doc["auth_mode"] = "rotating_sas";
    doc["ts"] = now;

    String payload;
    serializeJson(doc, payload);

    int httpCode = http.POST(payload);

    if (httpCode > 0) {
      Serial.printf("Push Success (%d)\n", httpCode);
    } else {
      Serial.printf("Push Error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }

  delay(10000);
}
