#include "includes.h"

// Global Objects
ESP8266WebServer server(80);
Config config;
bool provisioningMode = false;

// Global State
bool relays[4] = {false, false, false, false}; // State tracking
unsigned long lastDataReport = 0;
unsigned long lastCommandCheck = 0;
const char *fingerprint =
    "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
    "00"; // Should be real cert FP, but we use insecure client
          // for simplicity in example

// Intervals
const unsigned long REPORT_INTERVAL = 30000; // 30s
const unsigned long COMMAND_INTERVAL = 5000; // 5s

// Helper: Save Config
void saveConfig() {
  EEPROM.put(0, config);
  EEPROM.commit();
}

// Helper: Load Config
void loadConfig() {
  EEPROM.get(0, config);
  // Sanity check empty EEPROM
  if (config.wifi_ssid[0] == 0xFF) {
    memset(&config, 0, sizeof(Config));
  }
}

// Helper: Control Relay
void setRelay(int index, bool state) {
  if (index < 0 || index > 3)
    return;
  relays[index] = state;
  int pin;
  switch (index) {
  case 0:
    pin = GPIO_RELAY_0;
    break;
  case 1:
    pin = GPIO_RELAY_1;
    break;
  case 2:
    pin = GPIO_RELAY_2;
    break;
  case 3:
    pin = GPIO_RELAY_3;
    break;
  }
  // Active LOW for most relay boards
  digitalWrite(pin, state ? LOW : HIGH);
}

// ===========================
//       PROVISIONING
// ===========================

void setupProvisioning() {
  provisioningMode = true;
  WiFi.mode(WIFI_AP);
  String apName = "Datum-Relay-" + String(ESP.getChipId(), HEX);
  WiFi.softAP(apName.c_str(), "datum123");

  // Web Server Routes
  server.on("/info", HTTP_GET, []() {
    StaticJsonDocument<200> doc;
    doc["device_uid"] = String(ESP.getChipId());
    doc["firmware_version"] = FIRMWARE_VER;
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });

  server.on("/scan", HTTP_GET, []() {
    int n = WiFi.scanNetworks();
    DynamicJsonDocument doc(2048);
    for (int i = 0; i < n; ++i) {
      JsonObject net = doc.createNestedObject();
      net["ssid"] = WiFi.SSID(i);
      net["rssi"] = WiFi.RSSI(i);
      net["auth"] =
          (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? "open" : "secure";
    }
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });

  server.on("/configure", HTTP_POST, []() {
    if (!server.hasArg("wifi_ssid") || !server.hasArg("wifi_pass")) {
      server.send(400, "text/plain", "Missing WiFi credentials");
      return;
    }

    // Save Basic Info
    strncpy(config.wifi_ssid, server.arg("wifi_ssid").c_str(),
            sizeof(config.wifi_ssid));
    strncpy(config.wifi_pass, server.arg("wifi_pass").c_str(),
            sizeof(config.wifi_pass));

    // Optional Params
    if (server.hasArg("device_name"))
      strncpy(config.device_name, server.arg("device_name").c_str(),
              sizeof(config.device_name));
    if (server.hasArg("server_url"))
      strncpy(config.server_url, server.arg("server_url").c_str(),
              sizeof(config.server_url));
    if (server.hasArg("user_token"))
      strncpy(config.user_token, server.arg("user_token").c_str(),
              sizeof(config.user_token));

    // Reset API Key if re-configuring from scratch, unless preserved?
    // Usually safe to clear API key on re-provision to force re-registration
    memset(config.api_key, 0, sizeof(config.api_key));

    saveConfig();
    server.send(200, "text/plain", "Saved. Restarting...");
    delay(1000);
    ESP.restart();
  });

  server.begin();
  Serial.println("Provisioning Mode Started: " + apName);
}

// ===========================
//       DATUM CLIENT
// ===========================

WiFiClientSecure client;

void registerDevice() {
  if (strlen(config.user_token) == 0) {
    Serial.println("No User Token for registration.");
    return;
  }

  client.setInsecure(); // Skip cert validation for simplicity
  HTTPClient http;
  String url = String(config.server_url) + "/devices";

  if (http.begin(client, url)) {
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization",
                   "Bearer " + String(config.user_token)); // Use User Token

    StaticJsonDocument<200> doc;
    doc["name"] =
        strlen(config.device_name) > 0 ? config.device_name : "Relay Board";
    doc["type"] = DEVICE_TYPE_NAME;
    doc["uid"] = String(ESP.getChipId()); // Helper for server to identify HW

    String payload;
    serializeJson(doc, payload);

    int code = http.POST(payload);
    if (code == 200 || code == 201) {
      String response = http.getString();
      StaticJsonDocument<512> respDoc;
      deserializeJson(respDoc, response);

      const char *key = respDoc["api_key"];
      const char *id = respDoc["id"];

      if (key && id) {
        strncpy(config.api_key, key, sizeof(config.api_key));
        strncpy(config.device_id, id, sizeof(config.device_id));
        // Clear user token after successful registration for security
        memset(config.user_token, 0, sizeof(config.user_token));
        saveConfig();
        Serial.println("Registration Successful! API Key and ID saved.");
      }
    } else {
      Serial.printf("Registration Failed: %d %s\n", code,
                    http.getString().c_str());
    }
    http.end();
  }
}

void sendData() {
  if (strlen(config.api_key) == 0)
    return;

  client.setInsecure();
  HTTPClient http;
  // Get Device ID (assuming we stored it or ID is not needed if using API Key
  // auth which identifies device?) Wait, standard Datum API is
  // /devices/:id/data OR /data (if key identifies device). Current Server
  // `handlers_data.go` supports `POST /data` where device is inferred from
  // `dk_` key. PERFECT.

  String url = String(config.server_url) + "/data";

  if (http.begin(client, url)) {
    http.addHeader("Content-Type", "application/json");
    http.addHeader(
        "X-API-Key",
        String(config.api_key)); // Legacy Header or Header based auth
    // OR "Authorization: Bearer <sk_...>"
    // The server middleware checks "Authorization: Bearer" or "token" query
    // param. Let's use Bearer.
    http.addHeader("Authorization", "Bearer " + String(config.api_key));

    StaticJsonDocument<200> doc;
    // Map relays to generic names or just custom JSON
    doc["relay_0"] = relays[0];
    doc["relay_1"] = relays[1];
    doc["relay_2"] = relays[2];
    doc["relay_3"] = relays[3];
    doc["battery_adc"] = analogRead(ADC_BATTERY);

    String payload;
    serializeJson(doc, payload);

    int code = http.POST(payload);
    if (code == 200) {
      Serial.println("Data sent.");
    } else {
      Serial.printf("Data send failed: %d\n", code);
    }
    http.end();
  }
}

void checkCommands() {
  if (strlen(config.api_key) == 0 || strlen(config.device_id) == 0)
    return;

  client.setInsecure();
  HTTPClient http;

  // Endpoint: GET /devices/:id/commands
  String url = String(config.server_url) + "/devices/" +
               String(config.device_id) + "/commands";

  if (http.begin(client, url)) {
    http.addHeader("Authorization", "Bearer " + String(config.api_key));

    int code = http.GET();
    if (code == 200) {
      String payload = http.getString();
      StaticJsonDocument<1024> doc;
      deserializeJson(doc, payload); // Expecting array of commands or single

      // Handle "commands" array if wrapper exists, or just array
      JsonArray commands = doc.as<JsonArray>();
      if (commands.isNull() && doc.containsKey("commands")) {
        commands = doc["commands"].as<JsonArray>();
      }

      for (JsonVariant v : commands) {
        String type = v["type"].as<String>();
        JsonObject params = v["params"];

        if (type == "relay_control") {
          int index = params["relay_index"];
          bool state = params["state"];
          setRelay(index, state);
          Serial.printf("Command: Relay %d -> %s\n", index,
                        state ? "ON" : "OFF");
        } else if (type == "update_firmware") {
          String fwUrl = params["url"];
          Serial.println("Command: OTA Update -> " + fwUrl);
          // Simple OTA Implementation (Blocking)
          t_httpUpdate_return ret = ESPhttpUpdate.update(client, fwUrl);
          switch (ret) {
          case HTTP_UPDATE_FAILED:
            Serial.printf("OTA Failed: %s\n",
                          ESPhttpUpdate.getLastErrorString().c_str());
            break;
          case HTTP_UPDATE_NO_UPDATES:
            Serial.println("OTA: No updates");
            break;
          case HTTP_UPDATE_OK:
            Serial.println("OTA: OK");
            break;
          }
        }
      }
    }
    http.end();
  }
}

// ===========================
//          SETUP
// ===========================

void setup() {
  Serial.begin(115200);
  delay(500);
  EEPROM.begin(EEPROM_SIZE);
  loadConfig();

  // Init Relays
  pinMode(GPIO_RELAY_0, OUTPUT);
  pinMode(GPIO_RELAY_1, OUTPUT);
  pinMode(GPIO_RELAY_2, OUTPUT);
  pinMode(GPIO_RELAY_3, OUTPUT);
  setRelay(0, false);
  setRelay(1, false);
  setRelay(2, false);
  setRelay(3, false);

  if (strlen(config.wifi_ssid) == 0) {
    setupProvisioning();
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifi_ssid, config.wifi_pass);
    Serial.print("Connecting to WiFi");

    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 20) {
      delay(500);
      Serial.print(".");
      attempt++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

      // Auto-Register if needed
      if (strlen(config.api_key) == 0 && strlen(config.user_token) > 0) {
        registerDevice();
      }
    } else {
      Serial.println("\nWiFi Failed. Starting Provisioning Mode.");
      setupProvisioning();
    }
  }
}

// ===========================
//           LOOP
// ===========================

void loop() {
  if (provisioningMode) {
    server.handleClient();
  } else {
    if (WiFi.status() == WL_CONNECTED) {
      unsigned long now = millis();

      if (now - lastDataReport > REPORT_INTERVAL) {
        sendData();
        lastDataReport = now;
      }

      if (now - lastDataReport > REPORT_INTERVAL) {
        sendData();
        lastDataReport = now;
      }

      if (now - lastCommandCheck > COMMAND_INTERVAL) {
        checkCommands();
        lastCommandCheck = now;
      }
    }
  }
}
