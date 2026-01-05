/*
 * Datum IoT Platform - ESP32 MQTT + Self-Registration Example
 *
 * FEATURES:
 * 1. Zero-Touch Provisioning (SoftAP) with User Token support.
 * 2. Self-Registration: Registers itself automatically using the User Token.
 * 3. MQTT Integration:
 *    - Publishes telemetry to "data/{device_id}"
 *    - Subscribes to commands on "commands/{device_id}"
 * 4. Simulated Sensors: Temperature & Humidity
 *
 * CONFIGURATION:
 * - Creates AP: "Datum-Sensor-{UID}"
 * - User configures: WiFi SSID/Pass, Server URL, User Token, Device Name
 *
 * LICENSE: MIT
 */

#include "esp_mac.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <WiFi.h>

// ============ CONFIGURATION ============
#define FIRMWARE_VERSION "2.0.0"
#define DEVICE_MODEL "datum-sensor-mqtt"
#define DEFAULT_SERVER_URL "https://datum.bezg.in"

// LED Configuration (Use GPIO2 for onboard LED on most boards)
#define LED_PIN 2
#define LED_ON HIGH
#define LED_OFF LOW

// Reset Button (GPIO0 on DevKits)
#define RESET_BUTTON_PIN 0
#define RESET_HOLD_TIME 3000

// Setup Mode
#define SETUP_AP_PREFIX "Datum-Sensor-"
#define SETUP_HTTP_PORT 80

// Telemetry
#define SEND_INTERVAL_MS 10000 // 10s for demo purposes

// ============ GLOBALS ============
Preferences prefs;
WebServer setupServer(SETUP_HTTP_PORT);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

String deviceUID; // Hardware ID (MAC)
String deviceMAC;
String apiKey;   // Device API Key
String deviceID; // Device ID (UUID)
String serverURL;
String wifiSSID;
String wifiPass;
String userToken;  // User's Auth Token (for registration)
String deviceName; // User-friendly name

enum DeviceState {
  STATE_BOOT,
  STATE_SETUP,
  STATE_CONNECTING,
  STATE_REGISTERING, // New state for Self-Reg
  STATE_ONLINE,
  STATE_OFFLINE
};
DeviceState currentState = STATE_BOOT;

unsigned long setupStartTime = 0;
unsigned long lastTelemetryTime = 0;
unsigned long lastBlinkTime = 0;
bool ledState = false;

// ============ FORWARD DECLARATIONS ============
void startSetupMode();
void handleSetupRoot();
void handleConfigure();
void handleInfo();
void factoryReset();
void factoryReset();
bool reconnectMQTT();
String getMQTTHost();

// ============ UTILS ============
void initDeviceUID() {
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  char buf[13];
  sprintf(buf, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3],
          mac[4], mac[5]);
  deviceUID = String(buf);
  char buf2[18];
  sprintf(buf2, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3],
          mac[4], mac[5]);
  deviceMAC = String(buf2);
}

void updateLED() {
  int interval = 1000;
  switch (currentState) {
  case STATE_SETUP:
    interval = 100;
    break; // Fast
  case STATE_CONNECTING:
    interval = 250;
    break; // Medium
  case STATE_REGISTERING:
    interval = 500;
    break; // Slow
  case STATE_ONLINE:
    digitalWrite(LED_PIN, mqttClient.connected()
                              ? LED_ON
                              : LED_OFF); // Solid if MQTT connected
    return;
  default:
    interval = 2000;
    break;
  }

  if (millis() - lastBlinkTime > interval) {
    lastBlinkTime = millis();
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? LED_ON : LED_OFF);
  }
}

// ============ CREDENTIALS ============
void loadCredentials() {
  prefs.begin("datum", true);
  apiKey = prefs.getString("api_key", "");
  deviceID = prefs.getString("device_id", "");
  serverURL = prefs.getString("server_url", DEFAULT_SERVER_URL);
  wifiSSID = prefs.getString("wifi_ssid", "");
  wifiPass = prefs.getString("wifi_pass", "");
  userToken = prefs.getString("user_token", "");
  deviceName = prefs.getString("device_name", "");
  prefs.end();
}

void saveCredentials(String u, String s, String p, String t, String n) {
  prefs.begin("datum", false);
  prefs.putString("server_url", u);
  prefs.putString("wifi_ssid", s);
  prefs.putString("wifi_pass", p);
  prefs.putString("user_token", t);
  prefs.putString("device_name", n);
  prefs.end();
  serverURL = u;
  wifiSSID = s;
  wifiPass = p;
  userToken = t;
  deviceName = n;
}

void saveActivation(String did, String key) {
  prefs.begin("datum", false);
  prefs.putString("device_id", did);
  prefs.putString("api_key", key);
  prefs.remove("user_token"); // Token consumed
  prefs.end();
  deviceID = did;
  apiKey = key;
  userToken = "";
}

// ============ HTTP HANDLERS & WoT ============
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1"><style>body{font-family:sans-serif;background:#eee;padding:20px}.card{background:white;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}input{width:100%;padding:10px;margin:5px 0 15px;border:1px solid #ddd;border-radius:4px}button{width:100%;padding:12px;background:#007bff;color:white;border:none;border-radius:4px;font-size:16px}</style></head><body><div class="card"><h1>🚀 Setup Sensor</h1><form action="/configure" method="POST"><label>Server URL</label><input type="text" name="server_url" value="https://datum.bezg.in"><label>WiFi SSID</label><input type="text" name="wifi_ssid"><label>WiFi Password</label><input type="password" name="wifi_pass"><label>User Token (from Profile)</label><input type="text" name="user_token"><label>Device Name</label><input type="text" name="device_name"><button type="submit">Save & Connect</button></form></div></body></html>
)rawliteral";

void handleSetupRoot() { setupServer.send(200, "text/html", INDEX_HTML); }

void handleInfo() {
  String json = "{\"uid\":\"" + deviceUID + "\",\"mac\":\"" + deviceMAC +
                "\",\"model\":\"" + DEVICE_MODEL + "\"}";
  setupServer.send(200, "application/json", json);
}

void handleWoT() {
  DynamicJsonDocument doc(1024);
  doc["@context"] = "https://www.w3.org/2019/wot/td/v1";
  doc["id"] = "urn:dev:ops:" + deviceUID;
  doc["title"] = deviceName.length() > 0 ? deviceName : DEVICE_MODEL;

  // Security
  JsonObject sec = doc.createNestedObject("securityDefinitions");
  sec["nosec"]["scheme"] = "nosec";
  doc["security"] = "nosec";

  // MQTT Forms
  JsonObject props = doc.createNestedObject("properties");
  JsonObject temp = props.createNestedObject("temperature");
  temp["type"] = "number";
  temp["readOnly"] = true;
  JsonArray forms = temp.createNestedArray("forms");
  JsonObject f = forms.createNestedObject();
  f["href"] = "mqtt://" + getMQTTHost() + "/data/" +
              deviceID; // Topic structure implied
  f["op"] = "readproperty";

  JsonObject actions = doc.createNestedObject("actions");
  JsonObject cmd = actions.createNestedObject("restart");
  JsonArray cmdForms = cmd.createNestedArray("forms");
  JsonObject cf = cmdForms.createNestedObject();
  cf["href"] = "mqtt://" + getMQTTHost() + "/commands/" + deviceID;
  cf["op"] = "invokeaction";

  String json;
  serializeJson(doc, json);
  setupServer.send(200, "application/td+json", json);
}

void handleConfigure() {
  String u = setupServer.arg("server_url");
  String s = setupServer.arg("wifi_ssid");
  String p = setupServer.arg("wifi_pass");
  String t = setupServer.arg("user_token");
  String n = setupServer.arg("device_name");

  if (s.length() > 0) {
    saveCredentials(u, s, p, t, n);
    setupServer.send(200, "text/html", "<h1>Saved! Restarting...</h1>");
    delay(1000);
    ESP.restart();
  } else {
    setupServer.send(400, "text/plain", "Missing WiFi SSID");
  }
}

void startSetupMode() {
  currentState = STATE_SETUP;
  WiFi.mode(WIFI_AP);
  String ap = SETUP_AP_PREFIX + deviceUID.substring(8);
  WiFi.softAP(ap.c_str());

  setupServer.on("/", HTTP_GET, handleSetupRoot);
  setupServer.on("/info", HTTP_GET, handleInfo);
  setupServer.on("/configure", HTTP_POST, handleConfigure);
  setupServer.begin();
  Serial.println("AP Started: " + ap);
}

void startOnlineServer() {
  // Keep server running for WoT Discovery
  setupServer.on("/.well-known/wot-thing-description", HTTP_GET, handleWoT);
  setupServer.begin();
}

// ============ SELF REGISTRATION ============
bool registerDevice() {
  if (userToken.length() == 0)
    return false;

  currentState = STATE_REGISTERING;
  HTTPClient http;
  http.begin(serverURL + "/devices/register");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + userToken);

  String name = deviceName.length() > 0 ? deviceName : DEVICE_MODEL;
  String json = "{\"device_uid\":\"" + deviceUID + "\",\"device_name\":\"" +
                name + "\",\"device_type\":\"sensor\"}";

  int code = http.POST(json);
  String resp = http.getString();
  http.end();

  if (code == 200 || code == 201) {
    DynamicJsonDocument doc(512);
    deserializeJson(doc, resp);
    String tid = doc["device_id"];
    String tkey = doc["api_key"];
    if (tid.length() > 0) {
      saveActivation(tid, tkey);
      return true;
    }
  }
  return false;
}

// ============ MQTT ============
void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++)
    msg += (char)payload[i];
  Serial.print("MQTT Command: ");
  Serial.println(topic);
  Serial.println(msg);

  // Parse Command
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, msg);

  String action = doc["action"];
  String cmdId = doc["id"];
  if (cmdId.isEmpty())
    cmdId = doc["command_id"].as<String>();

  if (action == "restart") {
    ESP.restart();
  } else if (action == "led") {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Toggle
  }

  // ACK
  if (cmdId.length() > 0) {
    // We could publish ack via MQTT here if server supports it, or HTTP
    // Currently server expects HTTP ack
    HTTPClient http;
    http.begin(serverURL + "/devices/" + deviceID + "/commands/" + cmdId +
               "/ack");
    http.addHeader("Authorization", "Bearer " + apiKey);
    http.POST("{}");
    http.end();
  }
}

String getMQTTHost() {
  int start = serverURL.indexOf("://");
  if (start == -1)
    start = 0;
  else
    start += 3;
  int end = serverURL.indexOf("/", start);
  String host = (end == -1) ? serverURL.substring(start)
                            : serverURL.substring(start, end);
  int portIdx = host.indexOf(":");
  if (portIdx != -1)
    host = host.substring(0, portIdx);
  return host;
}

bool reconnectMQTT() {
  if (mqttClient.connected())
    return true;

  String host = getMQTTHost();
  mqttClient.setServer(host.c_str(), 1883);
  mqttClient.setCallback(mqttCallback);

  if (mqttClient.connect(deviceID.c_str(), deviceID.c_str(), apiKey.c_str())) {
    Serial.println("MQTT Connected!");
    mqttClient.subscribe(("commands/" + deviceID).c_str());
    return true;
  }
  return false;
}

void sendTelemetry() {
  if (!mqttClient.connected())
    return;

  DynamicJsonDocument doc(256);
  doc["temperature"] = 20.0 + (random(150) / 10.0);
  doc["humidity"] = 40.0 + (random(200) / 10.0);
  doc["rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;

  String json;
  serializeJson(doc, json);

  mqttClient.publish(("data/" + deviceID).c_str(), json.c_str());
  Serial.println("Telemetry Sent: " + json);
}

// ============ MAIN ============
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  initDeviceUID();
  loadCredentials();

  if (wifiSSID.length() == 0) {
    startSetupMode();
    return;
  }

  WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
  currentState = STATE_CONNECTING;
}

void loop() {
  updateLED();

  // Factory Reset Check
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    unsigned long start = millis();
    while (digitalRead(RESET_BUTTON_PIN) == LOW) {
      if (millis() - start > RESET_HOLD_TIME) {
        factoryReset();
      }
    }
  }

  if (currentState == STATE_SETUP) {
    setupServer.handleClient();
    return;
  }

  // WiFi Check
  if (WiFi.status() == WL_CONNECTED) {
    if (currentState == STATE_CONNECTING) {
      currentState = STATE_ONLINE;
      startOnlineServer(); // Enable WoT endpoint
    }

    // Always handle web requests (WoT)
    setupServer.handleClient();

    // Registration Check
    if (apiKey.length() == 0) {
      if (userToken.length() > 0) {
        if (registerDevice()) {
          currentState = STATE_ONLINE;
        } else {
          delay(5000); // Retry delay
        }
      } else {
        // No credentials to register... stuck or Fallback to Provisioning API?
        // For this demo, we assume User Token is provided.
        Serial.println("Waiting for registration...");
        delay(2000);
      }
      return;
    }

    // MQTT & Telemetry
    if (!mqttClient.connected()) {
      static unsigned long lastReconnect = 0;
      if (millis() - lastReconnect > 5000) {
        lastReconnect = millis();
        reconnectMQTT();
      }
    } else {
      mqttClient.loop();
      if (millis() - lastTelemetryTime > SEND_INTERVAL_MS) {
        lastTelemetryTime = millis();
        sendTelemetry();
      }
    }

  } else {
    // WiFi Disconnected
    currentState = STATE_CONNECTING;
  }
}

void factoryReset() {
  Serial.println("RESETTING...");
  prefs.begin("datum", false);
  prefs.clear();
  prefs.end();
  ESP.restart();
}
