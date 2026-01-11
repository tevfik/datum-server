/*
 * Datum IoT Platform - ESP32 WiFi AP Provisioning Example
 *
 * This firmware implements the WiFi AP provisioning mode for zero-touch setup.
 *
 * PROVISIONING FLOW:
 * 1. Device boots without credentials → enters Setup Mode
 * 2. Device creates WiFi AP: "Datum-Setup-XXXX" (last 4 of MAC)
 * 3. User connects phone to this AP
 * 4. Mobile app reads device UID via HTTP API
 * 5. Mobile app registers device with Datum server
 * 6. Mobile app sends credentials to device via HTTP
 * 7. Device saves credentials, restarts, connects to WiFi
 * 8. Device activates with Datum server and gets API key
 * 9. Normal operation begins
 *
 * HARDWARE:
 * - ESP32 (any variant)
 * - Optional: LED on GPIO2 for status indication
 * - Optional: Button on GPIO0 for factory reset
 *
 * LIBRARIES REQUIRED:
 * - ArduinoJson (v6+)
 * - WiFi (built-in)
 * - WebServer (built-in)
 * - Preferences (built-in)
 *
 * LICENSE: MIT
 */

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

// ============ CONFIGURATION ============
#define FIRMWARE_VERSION "1.0.0"
#define DEVICE_MODEL "datum-sensor-v1"

// LED Configuration
#define LED_PIN 2
#define LED_ON HIGH
#define LED_OFF LOW

// Factory Reset Button
#define RESET_BUTTON_PIN 0
#define RESET_HOLD_TIME 5000 // 5 seconds to factory reset

// Setup Mode Configuration
#define SETUP_AP_PREFIX "Datum-Setup-"
#define SETUP_AP_PASSWORD ""    // Open network for easy setup
#define SETUP_TIMEOUT_MS 300000 // 5 minutes timeout
#define SETUP_HTTP_PORT 80

// Data Send Configuration
#define SEND_INTERVAL_MS 60000  // 60 seconds
#define RETRY_INTERVAL_MS 30000 // 30 seconds for retries

// ============ GLOBAL VARIABLES ============
Preferences prefs;
WebServer setupServer(SETUP_HTTP_PORT);

// Device identification
String deviceUID;
String deviceMAC;

// Credentials (loaded from flash)
String apiKey;
String serverURL;
String wifiSSID;
String wifiPass;
String deviceID;

// State management
enum DeviceState {
  STATE_BOOT,
  STATE_SETUP_MODE,
  STATE_CONNECTING,
  STATE_ACTIVATING,
  STATE_ONLINE,
  STATE_OFFLINE
};
DeviceState currentState = STATE_BOOT;

unsigned long setupStartTime = 0;
unsigned long lastSendTime = 0;
unsigned long lastBlinkTime = 0;
bool ledState = false;

// ============ LED PATTERNS ============
void setLEDPattern(int onTime, int offTime) {
  unsigned long now = millis();
  if (now - lastBlinkTime >= (ledState ? onTime : offTime)) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? LED_ON : LED_OFF);
    lastBlinkTime = now;
  }
}

void updateLED() {
  switch (currentState) {
  case STATE_SETUP_MODE:
    setLEDPattern(100, 100); // Fast blink - setup mode
    break;
  case STATE_CONNECTING:
    setLEDPattern(250, 250); // Medium blink - connecting
    break;
  case STATE_ACTIVATING:
    setLEDPattern(500, 500); // Slow blink - activating
    break;
  case STATE_ONLINE:
    digitalWrite(LED_PIN, LED_ON); // Solid on - online
    break;
  case STATE_OFFLINE:
    setLEDPattern(1000, 1000); // Very slow blink - offline
    break;
  default:
    digitalWrite(LED_PIN, LED_OFF);
    break;
  }
}

// ============ CREDENTIAL MANAGEMENT ============
void loadCredentials() {
  prefs.begin("datum", true); // Read-only
  apiKey = prefs.getString("api_key", "");
  serverURL = prefs.getString("server_url", "");
  wifiSSID = prefs.getString("wifi_ssid", "");
  wifiPass = prefs.getString("wifi_pass", "");
  deviceID = prefs.getString("device_id", "");
  prefs.end();

  Serial.println("Loaded credentials:");
  Serial.printf("  Server: %s\n", serverURL.c_str());
  Serial.printf("  WiFi SSID: %s\n", wifiSSID.c_str());
  Serial.printf("  Device ID: %s\n", deviceID.c_str());
  Serial.printf("  Has API Key: %s\n", apiKey.length() > 0 ? "yes" : "no");
}

void saveCredentials(String newServerURL, String newWifiSSID,
                     String newWifiPass) {
  prefs.begin("datum", false); // Read-write
  prefs.putString("server_url", newServerURL);
  prefs.putString("wifi_ssid", newWifiSSID);
  prefs.putString("wifi_pass", newWifiPass);
  prefs.end();

  serverURL = newServerURL;
  wifiSSID = newWifiSSID;
  wifiPass = newWifiPass;

  Serial.println("Saved WiFi credentials");
}

void saveActivationCredentials(String newDeviceID, String newAPIKey) {
  prefs.begin("datum", false);
  prefs.putString("device_id", newDeviceID);
  prefs.putString("api_key", newAPIKey);
  prefs.end();

  deviceID = newDeviceID;
  apiKey = newAPIKey;

  Serial.println("Saved activation credentials");
}

void factoryReset() {
  Serial.println("!!! FACTORY RESET !!!");
  prefs.begin("datum", false);
  prefs.clear();
  prefs.end();

  Serial.println("Credentials cleared, restarting...");
  delay(1000);
  ESP.restart();
}

bool hasCredentials() {
  return wifiSSID.length() > 0 && serverURL.length() > 0;
}

bool isActivated() { return apiKey.length() > 0 && deviceID.length() > 0; }

// ============ DEVICE UID ============
void initDeviceUID() {
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);

  // Full MAC as UID (uppercase, no separators)
  char uidBuf[13];
  sprintf(uidBuf, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3],
          mac[4], mac[5]);
  deviceUID = String(uidBuf);

  // MAC with colons for display
  char macBuf[18];
  sprintf(macBuf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
          mac[3], mac[4], mac[5]);
  deviceMAC = String(macBuf);

  Serial.printf("Device UID: %s\n", deviceUID.c_str());
  Serial.printf("Device MAC: %s\n", deviceMAC.c_str());
}

// ============ SETUP MODE (WiFi AP) ============
void startSetupMode() {
  Serial.println("\n=== ENTERING SETUP MODE ===");
  currentState = STATE_SETUP_MODE;
  setupStartTime = millis();

  // Create AP name with last 4 chars of UID
  String apName = String(SETUP_AP_PREFIX) + deviceUID.substring(8);

  // Start AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName.c_str(), SETUP_AP_PASSWORD);

  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("AP Started: %s\n", apName.c_str());
  Serial.printf("AP IP: %s\n", apIP.toString().c_str());
  Serial.println("Connect to this WiFi and open http://" + apIP.toString());

  // Setup HTTP endpoints
  setupServer.on("/", HTTP_GET, handleSetupRoot);
  setupServer.on("/info", HTTP_GET, handleDeviceInfo);
  setupServer.on("/configure", HTTP_POST, handleConfigure);
  setupServer.on("/status", HTTP_GET, handleStatus);
  setupServer.onNotFound(handleNotFound);

  setupServer.begin();
  Serial.println("HTTP server started on port 80");
}

void stopSetupMode() {
  setupServer.stop();
  WiFi.softAPdisconnect(true);
  Serial.println("Setup mode stopped");
}

// ============ SETUP MODE HTTP HANDLERS ============
void handleSetupRoot() {
  String html =
      R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Datum Device Setup</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #333; font-size: 24px; margin-bottom: 20px; }
        .info { background: #e8f4fd; padding: 15px; border-radius: 5px; margin-bottom: 20px; }
        .info-item { margin: 8px 0; }
        .label { color: #666; font-size: 12px; }
        .value { font-family: monospace; font-size: 14px; color: #333; }
        form { margin-top: 20px; }
        input, button { width: 100%; padding: 12px; margin: 8px 0; border-radius: 5px; box-sizing: border-box; }
        input { border: 1px solid #ddd; }
        button { background: #007bff; color: white; border: none; cursor: pointer; font-size: 16px; }
        button:hover { background: #0056b3; }
        .note { font-size: 12px; color: #666; margin-top: 15px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔧 Datum Device Setup</h1>
        <div class="info">
            <div class="info-item">
                <div class="label">Device UID</div>
                <div class="value">)" +
          deviceUID + R"(</div>
            </div>
            <div class="info-item">
                <div class="label">MAC Address</div>
                <div class="value">)" +
          deviceMAC + R"(</div>
            </div>
            <div class="info-item">
                <div class="label">Firmware</div>
                <div class="value">)" +
          FIRMWARE_VERSION + R"(</div>
            </div>
            <div class="info-item">
                <div class="label">Model</div>
                <div class="value">)" +
          DEVICE_MODEL + R"(</div>
            </div>
        </div>
        <form action="/configure" method="POST">
            <input type="text" name="server_url" placeholder="Server URL (e.g., https://datum.example.com)" required >
      <input type = "text" name = "wifi_ssid" placeholder =
           "WiFi Network Name" required>
      <input type = "password" name = "wifi_pass" placeholder = "WiFi Password">
      <button type = "submit"> Configure Device</ button></ form>
      <p class = "note">
            📱 Using the Datum mobile app
      ? The app will configure this device automatically
            .</ p></ div></ body></ html>) ";
      setupServer.send(200, "text/html", html);
}

void handleDeviceInfo() {
  StaticJsonDocument<512> doc;
  doc["device_uid"] = deviceUID;
  doc["mac_address"] = deviceMAC;
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["model"] = DEVICE_MODEL;
  doc["status"] = isActivated()
                      ? "configured"
                      : (hasCredentials() ? "pending" : "unconfigured");

  String response;
  serializeJson(doc, response);

  setupServer.sendHeader("Access-Control-Allow-Origin", "*");
  setupServer.send(200, "application/json", response);
}

void handleConfigure() {
  String newServerURL = setupServer.arg("server_url");
  String newWifiSSID = setupServer.arg("wifi_ssid");
  String newWifiPass = setupServer.arg("wifi_pass");

  // Validate
  if (newServerURL.length() == 0 || newWifiSSID.length() == 0) {
    setupServer.send(400, "application/json",
                     "{\"error\":\"server_url and wifi_ssid are required\"}");
    return;
  }

  // Save credentials
  saveCredentials(newServerURL, newWifiSSID, newWifiPass);

  String html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Configuration Saved</title>
    <style>
        body { font-family: -apple-system, sans-serif; margin: 20px; background: #f5f5f5; text-align: center; }
        .container { max-width: 400px; margin: 50px auto; background: white; padding: 30px; border-radius: 10px; }
        h1 { color: #28a745; }
        p { color: #666; }
    </style>
</head>
<body>
    <div class="container">
        <h1>✅ Configuration Saved!</h1>
        <p>Device will restart and connect to WiFi.</p>
        <p>Please reconnect to your normal WiFi network.</p>
        <p style="margin-top: 30px; font-size: 14px;">
            If the device LED turns solid green, setup is complete.<br>
            If it keeps blinking, check your WiFi credentials.
        </p>
    </div>
</body>
</html>
)";
  setupServer.send(200, "text/html", html);

  // Give time for response to be sent
  delay(2000);

  // Restart to apply new configuration
  Serial.println("Configuration saved, restarting...");
  ESP.restart();
}

void handleStatus() {
  StaticJsonDocument<256> doc;
  doc["state"] = currentState;
  doc["has_credentials"] = hasCredentials();
  doc["is_activated"] = isActivated();
  doc["uptime_ms"] = millis();

  String response;
  serializeJson(doc, response);

  setupServer.sendHeader("Access-Control-Allow-Origin", "*");
  setupServer.send(200, "application/json", response);
}

void handleNotFound() {
  setupServer.send(404, "application/json", "{\"error\":\"not found\"}");
}

// ============ WIFI CONNECTION ============
bool connectToWiFi() {
  if (wifiSSID.length() == 0) {
    Serial.println("No WiFi SSID configured");
    return false;
  }

  Serial.printf("Connecting to WiFi: %s\n", wifiSSID.c_str());
  currentState = STATE_CONNECTING;

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    updateLED();
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  } else {
    Serial.println("\nWiFi connection failed!");
    return false;
  }
}

// ============ SERVER ACTIVATION ============
bool activateWithServer() {
  if (serverURL.length() == 0) {
    Serial.println("No server URL configured");
    return false;
  }

  Serial.println("Activating with server...");
  currentState = STATE_ACTIVATING;

  HTTPClient http;
  String activateURL = serverURL + "/prov/activate";

  http.begin(activateURL);
  http.addHeader("Content-Type", "application/json");

  // Create activation request
  StaticJsonDocument<256> doc;
  doc["device_uid"] = deviceUID;
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["model"] = DEVICE_MODEL;

  String payload;
  serializeJson(doc, payload);

  Serial.printf("POST %s\n", activateURL.c_str());
  int httpCode = http.POST(payload);

  if (httpCode == 200) {
    String response = http.getString();
    Serial.printf("Activation response: %s\n", response.c_str());

    StaticJsonDocument<512> respDoc;
    DeserializationError error = deserializeJson(respDoc, response);

    if (!error) {
      String newDeviceID = respDoc["device_id"].as<String>();
      String newAPIKey = respDoc["api_key"].as<String>();

      if (newDeviceID.length() > 0 && newAPIKey.length() > 0) {
        saveActivationCredentials(newDeviceID, newAPIKey);
        Serial.println("✓ Activation successful!");
        return true;
      }
    }
  } else if (httpCode == 404) {
    Serial.println(
        "No provisioning request found. Register device via mobile app first.");
  } else if (httpCode == 409) {
    Serial.println("Device already registered. Contact owner for reset.");
  } else if (httpCode == 410) {
    Serial.println(
        "Provisioning request expired. Create new request via mobile app.");
  } else {
    Serial.printf("Activation failed, HTTP code: %d\n", httpCode);
  }

  http.end();
  return false;
}

// ============ DATA SENDING ============
bool sendSensorData() {
  if (apiKey.length() == 0 || serverURL.length() == 0) {
    return false;
  }

  HTTPClient http;
  String dataURL = serverURL + "/dev/" + deviceID + "/data";

  http.begin(dataURL);
  http.addHeader("Authorization", "Bearer " + apiKey);
  http.addHeader("Content-Type", "application/json");

  // Create sensor data payload
  // TODO: Replace with actual sensor readings
  StaticJsonDocument<256> doc;
  doc["temperature"] = 20.0 + (random(100) / 10.0); // Simulated 20-30°C
  doc["humidity"] = 40.0 + (random(300) / 10.0);    // Simulated 40-70%
  doc["uptime_ms"] = millis();
  doc["rssi"] = WiFi.RSSI();

  String payload;
  serializeJson(doc, payload);

  int httpCode = http.POST(payload);
  http.end();

  if (httpCode == 200) {
    Serial.println("✓ Data sent successfully");
    return true;
  } else {
    Serial.printf("✗ Data send failed, HTTP code: %d\n", httpCode);
    return false;
  }
}

// ============ FACTORY RESET BUTTON ============
void checkFactoryReset() {
  static unsigned long buttonPressStart = 0;
  static bool buttonWasPressed = false;

  bool buttonPressed = (digitalRead(RESET_BUTTON_PIN) == LOW);

  if (buttonPressed && !buttonWasPressed) {
    buttonPressStart = millis();
    buttonWasPressed = true;
    Serial.println("Reset button pressed...");
  } else if (!buttonPressed && buttonWasPressed) {
    buttonWasPressed = false;
    Serial.println("Reset button released");
  } else if (buttonPressed && buttonWasPressed) {
    if (millis() - buttonPressStart >= RESET_HOLD_TIME) {
      factoryReset();
    }
  }
}

// ============ MAIN PROGRAM ============
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("  Datum IoT Device - WiFi AP Provision");
  Serial.printf("  Firmware: %s\n", FIRMWARE_VERSION);
  Serial.printf("  Model: %s\n", DEVICE_MODEL);
  Serial.println("========================================\n");

  // Initialize hardware
  pinMode(LED_PIN, OUTPUT);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, LED_OFF);

  // Get device UID
  initDeviceUID();

  // Load saved credentials
  loadCredentials();

  // Decision tree
  if (!hasCredentials()) {
    // No WiFi credentials - enter setup mode
    Serial.println("No credentials found, entering setup mode...");
    startSetupMode();
  } else {
    // Try to connect to WiFi
    if (connectToWiFi()) {
      if (!isActivated()) {
        // WiFi OK but not activated - try to activate
        if (activateWithServer()) {
          currentState = STATE_ONLINE;
        } else {
          // Activation failed - enter setup mode or retry?
          Serial.println("Activation failed, will retry periodically");
          currentState = STATE_OFFLINE;
        }
      } else {
        // Fully configured and activated
        currentState = STATE_ONLINE;
        Serial.println("Device is online and ready!");
      }
    } else {
      // WiFi connection failed - enter setup mode
      Serial.println("WiFi connection failed, entering setup mode...");
      startSetupMode();
    }
  }
}

void loop() {
  // Update LED based on state
  updateLED();

  // Check factory reset button
  checkFactoryReset();

  // State-specific handling
  switch (currentState) {
  case STATE_SETUP_MODE:
    setupServer.handleClient();

    // Check for timeout
    if (millis() - setupStartTime > SETUP_TIMEOUT_MS) {
      Serial.println("Setup mode timeout, restarting...");
      ESP.restart();
    }
    break;

  case STATE_ONLINE:
    // Send data periodically
    if (millis() - lastSendTime >= SEND_INTERVAL_MS) {
      if (WiFi.status() == WL_CONNECTED) {
        sendSensorData();
        lastSendTime = millis();
      } else {
        Serial.println("WiFi disconnected, reconnecting...");
        currentState = STATE_OFFLINE;
      }
    }
    break;

  case STATE_OFFLINE:
    // Try to reconnect
    if (millis() - lastSendTime >= RETRY_INTERVAL_MS) {
      if (WiFi.status() != WL_CONNECTED) {
        if (connectToWiFi()) {
          if (!isActivated()) {
            activateWithServer();
          }
          if (isActivated()) {
            currentState = STATE_ONLINE;
          }
        }
      }
      lastSendTime = millis();
    }
    break;

  default:
    break;
  }

  // Small delay to prevent tight loop
  delay(10);
}
