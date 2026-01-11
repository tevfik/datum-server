/*
 * Datum IoT Platform - ESP32/Arduino Client
 *
 * Sends sensor data and receives commands from Datum API.
 *
 * Hardware: ESP32, ESP8266, or any Arduino with WiFi
 * Libraries: ArduinoJson, WiFi
 *
 * Configuration:
 *   - Set WIFI_SSID and WIFI_PASSWORD
 *   - Set DEVICE_ID and API_KEY from your Datum dashboard
 */

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

// ============ CONFIGURATION ============
const char *WIFI_SSID = "your-wifi-ssid";
const char *WIFI_PASSWORD = "your-wifi-password";

const char *API_URL = "http://your-server:8007";
const char *DEVICE_ID = "dev_xxxxxxxxxxxxxxxx";
const char *API_KEY = "sk_live_xxxxxxxxxxxxxxxx";

const int SEND_INTERVAL = 60000; // 60 seconds
// =======================================

HTTPClient http;

void setup() {
  Serial.begin(115200);
  Serial.println("\n🔌 Datumpy IoT Device");

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    WiFi.reconnect();
    delay(5000);
    return;
  }

  // Read sensors (replace with your actual sensors)
  float temperature = readTemperature();
  float humidity = readHumidity();
  float battery = readBattery();

  // Send data
  int commandsPending = sendData(temperature, humidity, battery);

  // Check for commands if any pending
  if (commandsPending > 0) {
    pollAndExecuteCommands();
  }

  delay(SEND_INTERVAL);
}

float readTemperature() {
  // Replace with actual sensor reading
  return 20.0 + random(100) / 10.0; // 20-30°C
}

float readHumidity() {
  // Replace with actual sensor reading
  return 40.0 + random(300) / 10.0; // 40-70%
}

float readBattery() {
  // Replace with actual battery reading
  return 70.0 + random(300) / 10.0; // 70-100%
}

int sendData(float temp, float humidity, float battery) {
  String url = String(API_URL) + "/dev/" + DEVICE_ID + "/data";

  // Create JSON payload
  StaticJsonDocument<256> doc;
  doc["temperature"] = temp;
  doc["humidity"] = humidity;
  doc["battery"] = battery;

  String payload;
  serializeJson(doc, payload);

  // Send request
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + API_KEY);

  int httpCode = http.POST(payload);
  int commandsPending = 0;

  if (httpCode == 200) {
    String response = http.getString();
    Serial.print("📤 Sent data: ");
    Serial.println(response);

    // Parse response
    StaticJsonDocument<256> respDoc;
    deserializeJson(respDoc, response);
    commandsPending = respDoc["commands_pending"] | 0;
  } else {
    Serial.print("❌ Send failed: ");
    Serial.println(httpCode);
  }

  http.end();
  return commandsPending;
}

void pollAndExecuteCommands() {
  String url = String(API_URL) + "/dev/" + DEVICE_ID + "/cmd?wait=5";

  http.begin(url);
  http.addHeader("Authorization", String("Bearer ") + API_KEY);
  http.setTimeout(10000);

  int httpCode = http.GET();

  if (httpCode == 200) {
    String response = http.getString();
    Serial.print("📥 Commands: ");
    Serial.println(response);

    // Parse and execute commands
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, response);
    JsonArray commands = doc["commands"].as<JsonArray>();

    for (JsonObject cmd : commands) {
      const char *cmdId = cmd["command_id"];
      const char *action = cmd["action"];

      Serial.print("  Executing: ");
      Serial.println(action);

      // Execute command
      StaticJsonDocument<256> result;
      if (strcmp(action, "reboot") == 0) {
        result["rebooted"] = true;
        // ESP.restart();  // Uncomment for actual reboot
      } else if (strcmp(action, "set_interval") == 0) {
        int seconds = cmd["params"]["seconds"] | 60;
        result["new_interval"] = seconds;
        // Update SEND_INTERVAL here
      }

      // Acknowledge command
      acknowledgeCommand(cmdId, result);
    }
  }

  http.end();
}

void acknowledgeCommand(const char *cmdId, JsonDocument &result) {
  String url = String(API_URL) + "/dev/" + DEVICE_ID + "/cmd/" + cmdId + "/ack";

  StaticJsonDocument<256> doc;
  doc["status"] = "success";
  doc["result"] = result;

  String payload;
  serializeJson(doc, payload);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + API_KEY);

  int httpCode = http.POST(payload);

  if (httpCode == 200) {
    Serial.println("  ✅ Command acknowledged");
  } else {
    Serial.print("  ❌ Ack failed: ");
    Serial.println(httpCode);
  }

  http.end();
}
