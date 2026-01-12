#include <DatumIoT.h>
#include <WiFi.h>

// 1. Credentials
const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASS";

// 2. Datum Server Config
const char *DATUM_URL = "https://datum.bezg.in";
const char *DEVICE_ID = "YOUR_DEVICE_ID";
const char *API_KEY = "YOUR_API_KEY"; // Get from Datum Dashboard

// 3. Main Objects
WiFiClient espClient;
DatumIoT datum(espClient);

void setup() {
  Serial.begin(115200);

  // Connect WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // Initialize Datum IoT
  datum.begin(DATUM_URL, DEVICE_ID, API_KEY);

  // Register Command Callback
  datum.onCommand([](String id, String action, JsonObject params) {
    Serial.println("Received Command via Library!");
    Serial.println("Action: " + action);

    if (action == "led_on") {
      digitalWrite(2, HIGH); // Example
    } else if (action == "led_off") {
      digitalWrite(2, LOW);
    }
  });

  // Connect to Cloud
  datum.connect();
}

void loop() {
  datum.loop(); // Must be called frequently

  // Send Telemetry every 10 seconds
  static unsigned long lastSent = 0;
  if (millis() - lastSent > 10000) {
    lastSent = millis();

    if (datum.isConnected()) {
      datum.sendTelemetry("uptime", (float)millis() / 1000.0);
      datum.sendTelemetry("rssi", (float)WiFi.RSSI());
      Serial.println("Telemetry sent!");
    }
  }
}
