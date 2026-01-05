/*
 * Datum IoT Platform - ESP32 Self-Registration Example (HTTP)
 *
 * DESCRIPTION:
 * This example demonstrates the "Self-Registration" flow using HTTP.
 *
 * FLOW:
 * 1. Connects to WiFi.
 * 2. Checks if API Key exists in NVS.
 * 3. If NO API Key, but User Token IS provided:
 *    - POST /devices/register
 *    - Saves returned Device ID and API Key.
 * 4. Starts sending telemetry via HTTP POST.
 *
 * CONFIGURATION:
 * - Update wifiSSID, wifiPass, userToken, serverURL in code or via serial
 * input.
 */

#include "esp_mac.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>

// ============ CONFIGURATION ============
const char *wifiSSID = "YOUR_WIFI_SSID";
const char *wifiPass = "YOUR_WIFI_PASS";
const char *userToken = "YOUR_USER_TOKEN_FROM_PROFILE";
const char *serverURL = "https://datum.bezg.in";
const char *deviceName = "Self-Reg Device";

// ============ GLOBALS ============
Preferences prefs;
String deviceID;
String apiKey;
String deviceUID;

void initDeviceUID() {
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  char buf[13];
  sprintf(buf, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3],
          mac[4], mac[5]);
  deviceUID = String(buf);
  Serial.println("Device UID: " + deviceUID);
}

void loadCredentials() {
  prefs.begin("datum", true);
  deviceID = prefs.getString("device_id", "");
  apiKey = prefs.getString("api_key", "");
  prefs.end();
}

void saveCredentials(String id, String key) {
  prefs.begin("datum", false);
  prefs.putString("device_id", id);
  prefs.putString("api_key", key);
  prefs.end();
  deviceID = id;
  apiKey = key;
}

bool registerDevice() {
  Serial.println("Attempting Self-Registration...");

  if (String(userToken).length() == 0 ||
      String(userToken) == "YOUR_USER_TOKEN_FROM_PROFILE") {
    Serial.println("Error: User Token not configured!");
    return false;
  }

  HTTPClient http;
  http.begin(String(serverURL) + "/devices/register");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(userToken));

  DynamicJsonDocument doc(256);
  doc["device_uid"] = deviceUID;
  doc["device_name"] = deviceName;
  doc["device_type"] = "sensor_http";

  String payload;
  serializeJson(doc, payload);

  int httpCode = http.POST(payload);
  String resp = http.getString();
  http.end();

  Serial.printf("Register Response: %d\n%s\n", httpCode, resp.c_str());

  if (httpCode == 200 || httpCode == 201) {
    DynamicJsonDocument respDoc(512);
    deserializeJson(respDoc, resp);
    String tid = respDoc["device_id"];
    String tkey = respDoc["api_key"];
    if (tid.length() > 0) {
      saveCredentials(tid, tkey);
      Serial.println("Registration Successful! Credentials Saved.");
      return true;
    }
  }
  return false;
}

void sendTelemetry() {
  if (apiKey.length() == 0)
    return;

  HTTPClient http;
  http.begin(String(serverURL) + "/data/" +
             deviceID); // Note: New Standard Endpoint
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + apiKey);

  DynamicJsonDocument doc(128);
  doc["heartbeat"] = millis();
  doc["rssi"] = WiFi.RSSI();

  String payload;
  serializeJson(doc, payload);

  int code = http.POST(payload);
  Serial.printf("Telemetry Push: %d\n", code);
  http.end();
}

// ============ MAIN ============
void setup() {
  Serial.begin(115200);
  delay(1000);

  initDeviceUID();
  loadCredentials();

  // Connect WiFi
  WiFi.begin(wifiSSID, wifiPass);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");

  // Check Registration
  if (apiKey.length() == 0) {
    if (!registerDevice()) {
      Serial.println("Registration failed. halting.");
      while (1)
        delay(1000);
    }
  } else {
    Serial.println("Device already registered. ID: " + deviceID);
  }
}

void loop() {
  sendTelemetry();
  delay(10000);
}
