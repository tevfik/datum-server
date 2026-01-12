/*
#include <EEPROM.h>

void setup() {
  Serial.begin(115200);
  EEPROM.begin(2048); // Firmware'de kullandığınız boyut

  Serial.println("\nSiliniyor...");

  // Bütün hafızayı 0 (veya 255) ile doldurarak temizle
  for (int i = 0; i < 2048; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();

  Serial.println("EEPROM Silindi! Simdi asil kodu tekrar yukleyebilirsiniz.");
}

void loop() {
  // Boş döngü
}
*/

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <PubSubClient.h> // MQTT Support
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <memory>

// ======== HARDWARE CONFIG ========
// Relay Pins (Active Low/High depends on board, typically Low for Relay Boards)
#define GPIO_RELAY_0 5
#define GPIO_RELAY_1 4
#define GPIO_RELAY_2 0
#define GPIO_RELAY_3 2 // Be careful, GPIO0 is boot mode pin

// Battery Voltage ADC
#define ADC_BATTERY A0

// ======== FIRMWARE CONFIG ========
#define FIRMWARE_VER "1.2.2" // Bumped version for MQTT
#define DEVICE_TYPE_NAME "relay_board"
#define DEVICE_FRIENDLY_NAME "ESP8266 Relay"

// ======== STORAGE CONFIG ========
#define EEPROM_SIZE 2048
#define CONFIG_MAGIC 0xD4701103 // Increment this when modifying struct

struct Config {
  uint32_t magic;
  char wifi_ssid[32];
  char wifi_pass[64];
  char api_key[64]; // Increased from 33 to support sk_ (35) and dk_ (38) keys
  char server_url[128];
  char user_token[1024];
  char device_name[33];
  char device_id[37];
  int boot_failures;
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

// Helper: Send Log via MQTT
void sendLog(String msg) {
  if (mqttClient.connected()) {
    String topic = "dev/" + String(config.device_id) + "/logs";
    // Create a simple JSON log object or just send text. Datum usually expects
    // JSON or handles text? Let's send a JSON object: {"msg": "..."}
    String payload =
        "{\"msg\":\"" + msg + "\", \"ts\":" + String(millis() / 1000) + "}";
    mqttClient.publish(topic.c_str(), payload.c_str());
    Serial.println("LOG: " + msg);
    // Give a moment for the message to leave the buffer
    mqttClient.loop();
    delay(50);
  } else {
    Serial.println("LOG (No MQTT): " + msg);
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  // Print payload for debugging (be careful with large payloads)
  Serial.printf("MQTT Message [%s] (%d bytes)\n", topic, length);

  // Use DynamicJsonDocument for flexibility, though Static is faster.
  // 1024 is plenty for commands.
  DynamicJsonDocument doc(1024);

  // Deserialize directly from payload to save memory
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("JSON Parse Failed: ");
    Serial.println(error.c_str());
    return;
  }

  // Handle both 'type' (legacy) and 'action' (standard)
  const char *typeStr = doc["type"];
  const char *actionStr = doc["action"];

  String type = "";
  if (typeStr)
    type = String(typeStr);
  else if (actionStr)
    type = String(actionStr);

  Serial.printf("Parsed Type/Action: '%s'\n", type.c_str());

  if (type == "relay_control") {
    JsonObject params = doc["params"]; // Params might be nested
    if (params.isNull()) {
      Serial.println("Error: No params found for relay_control");
      return;
    }

    int index = params["relay_index"];
    bool state = params["state"];

    setRelay(index, state);
    Serial.printf("Command Executed: Relay %d -> %s\n", index,
                  state ? "ON" : "OFF");

    delay(50); // Small debounce
    // Send immediate update
    sendData(false, false);
  } else if (type == "update_firmware") {
    JsonObject params = doc["params"];
    String fwUrl = params["url"];
    if (fwUrl.length() == 0 && doc.containsKey("url"))
      fwUrl = doc["url"].as<String>();

    if (fwUrl.indexOf('?') == -1)
      fwUrl += "?token=" + String(config.api_key);
    else
      fwUrl += "&token=" + String(config.api_key);

    sendLog("OTA Update Initiated. URL: " + fwUrl);

    // Use Secure Client for HTTPS support
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(10000);

    ESPhttpUpdate.onProgress([](int cur, int total) {
      Serial.printf("OTA Progress: %d%%\n", (cur * 100) / total);
    });

    // Ensure MQTT loop runs a bit before blocking update
    mqttClient.loop();
    delay(100);

    t_httpUpdate_return ret = ESPhttpUpdate.update(client, fwUrl);
    switch (ret) {
    case HTTP_UPDATE_FAILED:
      sendLog("OTA Failed: " + ESPhttpUpdate.getLastErrorString());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      sendLog("OTA: No updates available");
      break;
    case HTTP_UPDATE_OK:
      sendLog("OTA Success! Rebooting...");
      delay(500); // Wait for message to send
      ESP.restart();
      break;
    }
  } else {
    Serial.println("Unknown command type: " + type);
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

  if (pass.length() == 0) {
    Serial.println("Skipping MQTT: No API Key");
    return false;
  }

  Serial.print("Connecting to MQTT... ");
  if (mqttClient.connect(clientId.c_str(), user.c_str(), pass.c_str())) {
    Serial.println("Connected!");
    // Subscribe to commands
    String cmdTopic = "dev/" + String(config.device_id) + "/cmd";
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
    String topic = "dev/" + String(config.device_id) + "/data";
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
    if (server.hasArg("server_url")) {
      String url = server.arg("server_url");
      if (url.endsWith("/")) {
        url.remove(url.length() - 1);
      }
      strncpy(config.server_url, url.c_str(), sizeof(config.server_url));
    }
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
  if (strlen(config.user_token) == 0) {
    Serial.println("Skipping Registration: No User Token");
    return;
  }

  String srv = String(config.server_url);
  if (srv.endsWith("/"))
    srv.remove(srv.length() - 1);
  String url = srv + "/dev";
  Serial.println("Registering Device at: " + url);

  // Client selection (HTTP vs HTTPS)
  std::unique_ptr<WiFiClient> client;
  if (url.startsWith("https://")) {
    WiFiClientSecure *secureClient = new WiFiClientSecure();
    secureClient->setInsecure(); // Skip cert validation
    client.reset(secureClient);
    Serial.println("Using secure client (HTTPS)");
  } else {
    client.reset(new WiFiClient());
    Serial.println("Using plain client (HTTP)");
  }

  HTTPClient http;
  http.begin(*client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(config.user_token));

  StaticJsonDocument<200> doc;
  doc["name"] =
      strlen(config.device_name) > 0 ? config.device_name : "Relay Board";
  doc["type"] = DEVICE_TYPE_NAME;
  doc["uid"] = String(ESP.getChipId()); // Ensure UID is sent

  String payload;
  serializeJson(doc, payload);
  Serial.println("Payload: " + payload);

  int code = http.POST(payload);
  Serial.printf("Registration HTTP Code: %d\n", code);

  if (code == 200 || code == 201) {
    String responseBody = http.getString();
    Serial.println("Response: " + responseBody);

    StaticJsonDocument<1024> resp; // Increased buffer
    DeserializationError error = deserializeJson(resp, responseBody);
    if (error) {
      Serial.print("JSON Parse Failed: ");
      Serial.println(error.c_str());
      http.end();
      return;
    }

    const char *key = resp["api_key"];
    const char *id = resp["device_id"]; // Note: Server returns 'device_id' or
                                        // 'id', check handlers
    if (!id)
      id = resp["id"]; // Fallback

    if (key && id) {
      strncpy(config.api_key, key, sizeof(config.api_key));
      strncpy(config.device_id, id, sizeof(config.device_id));
      memset(config.user_token, 0, sizeof(config.user_token)); // Clear token
      saveConfig();
      Serial.println("Registration Success! Saved Credentials.");
      Serial.printf("ID: %s\n", config.device_id);
    } else {
      Serial.println("Registration Failed: Missing api_key or id in response");
    }
  } else {
    String responseBody = http.getString();
    Serial.println("Registration Error Response: " + responseBody);
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

  // -- Boot Failure Logic --
  // Sanitize garbage value from new struct field if OTA updated
  if (config.boot_failures < 0 || config.boot_failures > 20) {
    config.boot_failures = 0;
  }

  config.boot_failures++;
  saveConfig();
  Serial.printf("Boot Count: %d\n", config.boot_failures);

  if (config.boot_failures >= 5) {
    Serial.println("!!! 5 CONSECUTIVE BOOT FAILURES !!!");
    Serial.println("Resetting Configuration...");
    // Wipe
    memset(&config, 0, sizeof(Config));
    config.magic = CONFIG_MAGIC;
    config.boot_failures = 0;
    saveConfig();
    // Blink to indicate reset
    pinMode(2, OUTPUT); // LED on ESP module
    for (int i = 0; i < 5; i++) {
      digitalWrite(2, LOW);
      delay(200);
      digitalWrite(2, HIGH);
      delay(200);
    }
    // Continue to loop, which will trigger provisioning since SSID is empty
  }

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

      // Successful Connection Logic
      if (strlen(config.api_key) > 0) {
        // Reset boot failures on success
        if (config.boot_failures > 0) {
          config.boot_failures = 0;
          saveConfig();
        }

        // Setup Local OTA
        ArduinoOTA.setHostname("datum-relay");
        ArduinoOTA.begin();
      }

      // CRITICAL START: Auto-Provisioning Fallback
      // If we still have no API Key (registration failed or no token),
      // we MUST go to Provisioning Mode so user can fix it (since no button).
      if (strlen(config.api_key) == 0) {
        Serial.println(
            "No API Key and Registration Failed -> Entering Provisioning Mode");
        setupProvisioning();
      } else {
        connectMQTT();
        sendData(true, true);
      }
      // CRITICAL END

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
      ArduinoOTA.handle(); // Handle Local OTA
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
