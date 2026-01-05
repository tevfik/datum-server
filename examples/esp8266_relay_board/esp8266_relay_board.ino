#include <ArduinoJson.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <PubSubClient.h> // MQTT Support
#include <WiFiClient.h>

// ======== HARDWARE CONFIG ========
// Relay Pins (Active Low/High depends on board, typically Low for Relay Boards)
#define GPIO_RELAY_0 16
#define GPIO_RELAY_1 5
#define GPIO_RELAY_2 4
#define GPIO_RELAY_3 0 // Be careful, GPIO0 is boot mode pin

// Battery Voltage ADC
#define ADC_BATTERY A0

// ======== FIRMWARE CONFIG ========
#define FIRMWARE_VER "1.1.0" // Bumped version for MQTT
#define DEVICE_TYPE_NAME "relay_board"
#define DEVICE_FRIENDLY_NAME "ESP8266 Relay"

// ======== STORAGE CONFIG ========
#define EEPROM_SIZE 2048
#define CONFIG_MAGIC 0xD4701102 // Increment this when modifying struct

struct Config {
  uint32_t magic;
  char wifi_ssid[32];
  char wifi_pass[64];
  char api_key[33];
  char server_url[128];
  char user_token[1024];
  char device_name[33];
  char device_id[37];
};

// Global Objects
ESP8266WebServer server(80);
Config config;
bool provisioningMode = false;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
String mqttHost; // Global for PubSubClient pointer safety

// Global State
bool relays[4] = {false, false, false, false}; // State tracking
unsigned long lastDataReport = 0;
// const unsigned long COMMAND_INTERVAL = 5000; // Not needed for MQTT (Push)
const unsigned long REPORT_INTERVAL = 30000; // 30s
unsigned long lastReconnectAttempt = 0;

// Helper: Save/Load Config
void saveConfig() {
  config.magic = CONFIG_MAGIC;
  EEPROM.put(0, config);
  EEPROM.commit();
}

void loadConfig() {
  EEPROM.get(0, config);
  if (config.magic != CONFIG_MAGIC) {
    Serial.println("Invalid Config Magic! Wiping EEPROM...");
    memset(&config, 0, sizeof(Config));
    config.magic = CONFIG_MAGIC;
    saveConfig();
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
  default:
    return;
  }
  digitalWrite(pin, state ? LOW : HIGH); // Active LOW
}

// ===========================
//       MQTT & COMMANDS
// ===========================

// Parse domain/IP from URL string (e.g., https://datum.bezg.in ->
// datum.bezg.in)
String getHostFromUrl(String url) {
  int index = url.indexOf("://");
  if (index != -1)
    url = url.substring(index + 3);
  int slash = url.indexOf("/");
  if (slash != -1)
    url = url.substring(0, slash);
  int port = url.indexOf(":");
  if (port != -1)
    url = url.substring(0, port);
  return url;
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.printf("MQTT Message [%s]: %s\n", topic, message.c_str());

  StaticJsonDocument<1024> doc;
  deserializeJson(doc, message);

  String type = doc["type"].as<String>();
  JsonObject params = doc["params"];

  if (type == "relay_control") {
    int index = params["relay_index"];
    bool state = params["state"];
    setRelay(index, state);
    Serial.printf("Command: Relay %d -> %s\n", index, state ? "ON" : "OFF");
    delay(100);
    // Send immediate update
    sendData(false, false);
  } else if (type == "update_firmware") {
    String fwUrl = params["url"];
    if (fwUrl.indexOf('?') == -1)
      fwUrl += "?token=" + String(config.api_key);
    else
      fwUrl += "&token=" + String(config.api_key);

    t_httpUpdate_return ret = ESPhttpUpdate.update(espClient, fwUrl);
    switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.println("OTA Failed");
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

boolean connectMQTT() {
  if (mqttClient.connected())
    return true;

  mqttHost = getHostFromUrl(String(config.server_url));

  espClient.setTimeout(10000);                  // 10s (Unit is ms!)
  mqttClient.setServer(mqttHost.c_str(), 1883); // Use global buffer
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512); // 512 bytes is safer for ESP8266 RAM
  mqttClient.setKeepAlive(60);   // 60s Keep Alive

  String clientId = String(config.device_id);
  String user = String(config.device_id);
  String pass = String(config.api_key);

  Serial.print("Connecting to MQTT... ");
  if (mqttClient.connect(clientId.c_str(), user.c_str(), pass.c_str())) {
    Serial.println("Connected!");
    // Subscribe to commands
    String cmdTopic = "cmd/" + String(config.device_id);
    mqttClient.subscribe(cmdTopic.c_str());
    Serial.println("Subscribed to " + cmdTopic);
    return true;
  } else {
    Serial.print("Failed, rc=");
    int rc = mqttClient.state();
    Serial.print(rc);
    if (rc == 5) {
      Serial.println(" (UNAUTHORIZED) - Check API Key!");
    } else {
      Serial.println("");
    }
    return false;
  }
}

// Send Data via MQTT (Fallback to HTTP if needed, but MQTT preferred)
void sendData(bool isBoot, bool isConnect) {
  if (strlen(config.api_key) == 0)
    return;

  StaticJsonDocument<512> doc;
  doc["relay_0"] = relays[0];
  doc["relay_1"] = relays[1];
  doc["relay_2"] = relays[2];
  doc["relay_3"] = relays[3];
  doc["battery_adc"] = analogRead(ADC_BATTERY);
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["uptime"] = millis() / 1000;

  if (isConnect) {
    doc["local_ip"] = WiFi.localIP().toString();
    doc["ssid"] = WiFi.SSID();
  }
  if (isBoot)
    doc["fw_ver"] = FIRMWARE_VER;

  String payload;
  serializeJson(doc, payload);

  // Try MQTT first
  if (mqttClient.connected()) {
    String topic = "data/" + String(config.device_id);
    mqttClient.publish(topic.c_str(), payload.c_str());
    Serial.println("MQTT Publish: " + payload);
  } else {
    // Fallback? Or just skip? Camera fw skips. Let's stick to MQTT logic
    // mostly.
    Serial.println("MQTT Disconnected. Skip report.");
  }
}

// ===========================
//       PROVISIONING (Simplified)
// ===========================
void setupProvisioning() {
  provisioningMode = true;
  WiFi.mode(WIFI_AP);
  String apName = "Datum-Relay-" + String(ESP.getChipId(), HEX);
  WiFi.softAP(apName.c_str(), NULL);

  server.on("/info", HTTP_GET, []() {
    StaticJsonDocument<200> doc;
    doc["device_uid"] = String(ESP.getChipId());
    doc["firmware_version"] = FIRMWARE_VER;
    doc["device_type"] = DEVICE_TYPE_NAME;
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
    if (!server.hasArg("wifi_ssid")) {
      server.send(400, "text/plain", "Missing");
      return;
    }
    strncpy(config.wifi_ssid, server.arg("wifi_ssid").c_str(),
            sizeof(config.wifi_ssid));
    strncpy(config.wifi_pass, server.arg("wifi_pass").c_str(),
            sizeof(config.wifi_pass));
    if (server.hasArg("server_url"))
      strncpy(config.server_url, server.arg("server_url").c_str(),
              sizeof(config.server_url));
    if (server.hasArg("user_token"))
      strncpy(config.user_token, server.arg("user_token").c_str(),
              sizeof(config.user_token));
    if (server.hasArg("device_name"))
      strncpy(config.device_name, server.arg("device_name").c_str(),
              sizeof(config.device_name));

    // Clear old key to force re-register
    memset(config.api_key, 0, sizeof(config.api_key));
    saveConfig();
    server.send(200, "text/plain", "Saved. Restarting...");
    delay(1000);
    ESP.restart();
  });

  server.begin();
  Serial.println("Provisioning Mode Started: " + apName);
}

// Helper: HTTP Registration (One-time)
void registerDevice() {
  if (strlen(config.user_token) == 0)
    return;
  HTTPClient http;
  String url = String(config.server_url) + "/devices";
  http.begin(espClient, url); // Use basic client
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(config.user_token));

  StaticJsonDocument<200> doc;
  doc["name"] =
      strlen(config.device_name) > 0 ? config.device_name : "Relay Board";
  doc["type"] = DEVICE_TYPE_NAME;
  doc["uid"] = String(ESP.getChipId());

  String payload;
  serializeJson(doc, payload);
  int code = http.POST(payload);

  if (code == 200 || code == 201) {
    StaticJsonDocument<512> resp;
    deserializeJson(resp, http.getString());
    const char *key = resp["api_key"];
    const char *id = resp["id"];
    if (key && id) {
      strncpy(config.api_key, key, sizeof(config.api_key));
      strncpy(config.device_id, id, sizeof(config.device_id));
      memset(config.user_token, 0, sizeof(config.user_token)); // Clear token
      saveConfig();
      Serial.println("Registration Success!");
    }
  }
  http.end();
}

// ===========================
//           SETUP
// ===========================
void setup() {
  Serial.begin(115200);
  delay(500);
  EEPROM.begin(EEPROM_SIZE);
  loadConfig();

  pinMode(GPIO_RELAY_0, OUTPUT);
  pinMode(GPIO_RELAY_1, OUTPUT);
  pinMode(GPIO_RELAY_2, OUTPUT);
  pinMode(GPIO_RELAY_3, OUTPUT);
  // Default OFF
  setRelay(0, false);
  setRelay(1, false);
  setRelay(2, false);
  setRelay(3, false);

  if (strlen(config.wifi_ssid) == 0) {
    setupProvisioning();
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifi_ssid, config.wifi_pass);
    Serial.print("Connecting WiFi");
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 20) {
      delay(500);
      Serial.print(".");
      attempt++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi Connected!");
      if (strlen(config.api_key) == 0 && strlen(config.user_token) > 0) {
        registerDevice();
      }
      connectMQTT();
      sendData(true, true);
    } else {
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
      // Manage MQTT
      if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > 5000) {
          lastReconnectAttempt = now;
          if (connectMQTT()) {
            lastReconnectAttempt = 0;
          }
        }
      } else {
        mqttClient.loop();
      }

      // Periodic Reporting
      unsigned long now = millis();
      if (now - lastDataReport > REPORT_INTERVAL) {
        sendData(false, false);
        lastDataReport = now;
      }
    }
  }
}
