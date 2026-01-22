/*
 * Datum IoT Platform - ESP32 Static Authentication Example
 *
 * DESCRIPTION:
 * This example demonstrates the simplest authentication method: Static API Key.
 * Ideally, this key is provided during provisioning (via mobile app) and saved
 * to Non-Volatile Storage (NVS). For this example, it's hardcoded.
 *
 * SECURITY LEVEL:
 * Medium. The key does not change. If compromised, it must be manually revoked
 * and replaced on the device.
 *
 * FLOW:
 * 1. Connects to WiFi.
 * 2. Uses the static API Key to send data.
 */

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

// ============ CONFIGURATION ============
const char *wifiSSID = "YOUR_WIFI_SSID";
const char *wifiPass = "YOUR_WIFI_PASS";

// Device credentials (obtain these from the Datum Mobile App or Console)
const char *deviceID = "YOUR_DEVICE_ID";
const char *apiKey = "sk_YOUR_STATIC_API_KEY"; // Starts with sk_ or dk_

const char *serverURL =
    "http://YOUR_SERVER_IP:8000"; // e.g. https://api.datum.io

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
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(serverURL) + "/dev/" + deviceID + "/data";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    // Standard Bearer Auth with Static Key
    http.addHeader("Authorization", "Bearer " + String(apiKey));

    // Create a dummy telemetry payload
    DynamicJsonDocument doc(128);
    doc["temperature"] = random(20, 30);
    doc["humidity"] = random(40, 60);
    doc["ts"] = millis();

    String payload;
    serializeJson(doc, payload);

    Serial.print("Sending data... ");
    int httpCode = http.POST(payload);

    if (httpCode > 0) {
      Serial.printf("Success (%d): %s\n", httpCode, http.getString().c_str());
    } else {
      Serial.printf("Error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }

  delay(10000); // Send every 10 seconds
}
