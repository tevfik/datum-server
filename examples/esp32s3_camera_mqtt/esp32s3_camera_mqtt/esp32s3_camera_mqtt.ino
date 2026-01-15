/**
 * ESP32-S3 Camera Streamer with WiFi Provisioning Portal (ESP-DASH Style)
 *
 * This firmware combines camera streaming with zero-touch WiFi provisioning.
 * When no WiFi is configured, it starts an AP with a captive portal for setup.
 * Supports both cloud streaming (via WebSocket) and local MJPEG streaming.
 *
 * FEATURES:
 * - WiFi AP provisioning (Datum-Camera-XXXX hotspot)
 * - Modern Dashboard UI (Dark Theme, Responsive)
 * - Local MJPEG Stream (/stream endpoint)
 * - Real-time stats (RSSI, Uptime)
 * - Cloud integration with Datum Server
 * - Command-based control
 *
 * Author: Datum IoT Platform
 * License: MIT
 */

#include "camera_manager.h" // Camera Manager Module
#include "camera_pins.h"    // Camera Pins
#include "esp_camera.h"
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Preferences.h>
// PubSubClient included via mqtt_manager.h
#include "mqtt_manager.h"
#include "wifi_manager.h"

// frameCounter moved to camera_manager.cpp, declaring extern here
extern int frameCounter;

// Methods for SD Card & Web Routes
#include "sd_storage.h"
#include "web_routes.h"

// Motion Globals
#include "eif_motion.h"

// Forward define neopixelWrite if not available (safe for S3)
extern void neopixelWrite(uint8_t pin, uint8_t red, uint8_t green,
                          uint8_t blue);
#include <WebServer.h>
#include <WiFi.h>

// Debug Configuration
// Uncomment the following line to enable detailed debug logs
// Debug Configuration
// Uncomment the following line to enable detailed debug logs
#define DEBUG_MODE

#ifdef DEBUG_MODE
#define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#define DEBUG_PRINTLN(...)
#define DEBUG_PRINTF(...)
#endif

// ============================================================================
// Board Selection
// ============================================================================
// #define CAMERA_MODEL_AI_THINKER
// #define CAMERA_MODEL_ESP32S3_CAM
// Camera Model selected in camera_manager.cpp
// #define CAMERA_MODEL_FREENOVE_S3

// ============================================================================
// Configuration
// ============================================================================
#define FIRMWARE_VERSION "2.1.0"
#define DEVICE_MODEL "datum-camera-s3"
#define DEFAULT_SERVER_URL "https://datum.bezg.in"

#define SETUP_AP_PREFIX "Datum-Camera-"
#define SETUP_AP_PASSWORD ""
#define SETUP_TIMEOUT_MS 300000
#define SETUP_HTTP_PORT 80

// ============================================================================
// Pin Definitions
// ============================================================================
// Pin Definitions moved to camera_manager.cpp

// ============================================================================
// Globals
// ============================================================================
Preferences prefs;
// MQTT Objects moved to mqtt_manager.cpp
// WiFiClient espClient;
// PubSubClient mqttClient(espClient);
WebServer setupServer(SETUP_HTTP_PORT);
HTTPClient httpStream;

// Dual Core Globals
// Dual Core Globals
// camTaskHandle, cameraMutex moved to camera_manager.cpp

// State Variables
String deviceUID;
String deviceName;
String apiKey;
String serverURL;
String wifiSSID;
String wifiPass;
String deviceID;
String deviceMAC;
String userToken; // Store token directly
// mqttHost moved to mqtt_manager.cpp

// LED State Globals
byte savedR = 255;
byte savedG = 255;
byte savedB = 255;
int savedBrightness = 100;
bool torchState = false;

// Task Handles
TaskHandle_t networkTaskHandle = NULL;
void networkTask(void *pvParameters);

enum DeviceState {
  STATE_BOOT,
  STATE_SETUP_MODE,
  STATE_CONNECTING,
  STATE_ACTIVATING,
  STATE_ONLINE,
  STATE_OFFLINE
};
DeviceState currentState = STATE_BOOT;

volatile bool streaming = false; // Cloud streaming state
unsigned long lastFrameTime = 0;
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// Forward Declarations
// Forward Declarations: handleSnap, getFrameSizeFromName moved to
// camera_manager.h

// Task handles
TaskHandle_t streamTask;
const int COMMAND_POLL_INTERVAL_MS = 10000;
unsigned long lastCommandPoll = 0;

// updateFirmware moved to ota_manager.cpp
unsigned long setupStartTime = 0;
unsigned long lastLedBlink = 0;
bool ledState = false;

unsigned long lastTelemetryTime = 0; // Telemetry timer

HTTPClient httpCommand;
// Removed redundant httpStream, espClient, mqttClient

bool requestRestart = false;
bool isActivated() { return apiKey.length() > 0; }
bool justConnected = false;
bool initialBoot = true;

void onWifiConnected() {
  setServerURL(serverURL);
  syncTime();
}

// ============================================================================
// Web Interface (ESP-DASH Style)
// ============================================================================
// DASHBOARD_HTML moved to web_routes.cpp

// ============================================================================
// Core Functions
// ============================================================================

// Helper to extract nested JSON object
// Helper functions (extractJson etc) moved to mqtt_manager.cpp/h

// Frame Size helpers moved to camera_manager.cpp/h

// Check for Factory Reset (Boot Button held manually during runtime)
unsigned long buttonPressStart = 0;

// Button Handler with improved feedback
void handleFactoryResetButton() {
  // Use GPIO 0 for Boot Button
  const int BTN_PIN = 0;
  pinMode(BTN_PIN, INPUT_PULLUP);

  static unsigned long pressStart = 0;
  static bool isPressed = false;

  // Read button (Active LOW)
  if (digitalRead(BTN_PIN) == LOW) {
    if (!isPressed) {
      // Just pressed
      isPressed = true;
      pressStart = millis();
      Serial.println("[BTN] Button Pressed... Hold 3s to Reset");
    }

    // Check if held long enough
    if (millis() - pressStart > 3000) {
      Serial.println("[BTN] Factory Reset TRIGGERED!");

// Visual Feedback (Fast Blink)
#ifdef LED_GPIO_NUM
      for (int i = 0; i < 10; i++) {
        digitalWrite(LED_GPIO_NUM, !digitalRead(LED_GPIO_NUM));
        delay(50);
      }
      digitalWrite(LED_GPIO_NUM, LOW);
#endif

      // Clear NVS
      prefs.begin("datum", false);
      prefs.clear();
      prefs.end();

      Serial.println("[BTN] Reset Complete. Restarting...");
      delay(100);
      ESP.restart();
    }
  } else {
    // Button is HIGH (Released)
    if (isPressed) {
      // Just released
      long duration = millis() - pressStart;
      Serial.printf("[BTN] Button Released after %ld ms (Action Cancelled)\n",
                    duration);
      isPressed = false;
      pressStart = 0;
    }
  }
}

void updateLED() {
  if (digitalRead(0) == LOW)
    return; // Don't interfere if button is being used for other things (though
            // usually checked at boot)

  unsigned long now = millis();

  int blinkInterval = 1000;
  switch (currentState) {
  case STATE_SETUP_MODE:
    blinkInterval = 100;
    break;
  case STATE_CONNECTING:
    blinkInterval = 250;
    break;
  case STATE_ACTIVATING:
    blinkInterval = 500;
    break;
  case STATE_ONLINE:
#ifdef LED_GPIO_NUM
#if LED_GPIO_NUM == 48
    // Unified LED Logic:
    // If Torch ON: Use Saved Color/Brightness
    // If Torch OFF: Turn OFF completely (No status blink)
    if (torchState) {
      int r = (savedR * savedBrightness) / 100;
      int g = (savedG * savedBrightness) / 100;
      int b = (savedB * savedBrightness) / 100;
      neopixelWrite(LED_GPIO_NUM, r, g, b);
    } else {
      neopixelWrite(LED_GPIO_NUM, 0, 0, 0); // OFF
    }
#else
    digitalWrite(LED_GPIO_NUM, HIGH);
#endif
#endif
    return;
  default:
    blinkInterval = 1000;
  }
  if (now - lastLedBlink >= blinkInterval) {
    ledState = !ledState;
#ifdef LED_GPIO_NUM
#if LED_GPIO_NUM == 48
    // Freenove S3 WS2812 status blink (Blue for Setup/Connecting)
    // Only blink if torch is NOT on
    if (!torchState) {
      neopixelWrite(LED_GPIO_NUM, 0, 0, ledState ? 10 : 0);
    } else {
      // Keep Torch Color
      int r = (savedR * savedBrightness) / 100;
      int g = (savedG * savedBrightness) / 100;
      int b = (savedB * savedBrightness) / 100;
      neopixelWrite(LED_GPIO_NUM, r, g, b);
    }
#else
    digitalWrite(LED_GPIO_NUM, ledState ? HIGH : LOW);
#endif
#endif
    lastLedBlink = now;
  }
}

void loadCredentials() {
  prefs.begin("datum", true);
  apiKey = prefs.getString("api_key", "");
  apiKey = prefs.getString("api_key", "");
  serverURL = prefs.getString("server_url", DEFAULT_SERVER_URL);
  if (serverURL.length() == 0)
    serverURL = DEFAULT_SERVER_URL;
  wifiSSID = prefs.getString("wifi_ssid", "");
  wifiPass = prefs.getString("wifi_pass", "");
  deviceID = prefs.getString("device_id", "");
  userToken = prefs.getString("user_token", "");
  deviceName = prefs.getString("device_name", "");
  prefs.end();
}

void saveCredentials(String u, String s, String p, String token, String name) {
  prefs.begin("datum", false);
  prefs.putString("server_url", u);
  prefs.putString("wifi_ssid", s);
  prefs.putString("wifi_pass", p);
  prefs.putString("user_token", token);
  prefs.putString("device_name", name);
  prefs.end();
  serverURL = u;
  wifiSSID = s;
  wifiPass = p;
  userToken = token;
  deviceName = name;
}

void saveActivation(String id, String key) {
  prefs.begin("datum", false);
  prefs.putString("device_id", id);
  prefs.putString("api_key", key);
  // Clear temp user creds
  prefs.putString("api_key", key);
  // Clear temp user creds
  prefs.remove("user_token");
  prefs.end();
  deviceID = id;
  apiKey = key;
  userToken = "";
}

// Use Low-Level MAC retrieval to ensure it works even if WiFi isn't ready
#include "esp_mac.h"
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

// initCamera moved to camera_manager.cpp

// ============================================================================
// Handlers
// ============================================================================
// Handlers moved to web_routes.cpp

// Include DNSServer for Captive Portal
#include <DNSServer.h>
DNSServer dnsServer;

void startSetupMode() {
  Serial.println("Starting Setup Mode"); // Always print critical state changes
  currentState = STATE_SETUP_MODE;
  setupStartTime = millis();
  WiFi.mode(WIFI_AP);
  String ap = String(SETUP_AP_PREFIX) + deviceUID.substring(8);
  WiFi.softAP(ap.c_str(), SETUP_AP_PASSWORD);

  // Setup DNS Server for Captive Portal (Redirect all to AP IP)
  dnsServer.start(53, "*", WiFi.softAPIP());

  setupWebRoutes(setupServer, apiKey);
  setupServer.begin();

  Serial.println("AP Started: " + ap);
  Serial.println("Gateway IP: " + WiFi.softAPIP().toString());

// Blink LED rapidly to indicate Setup Mode
#ifdef LED_GPIO_NUM
  pinMode(LED_GPIO_NUM, OUTPUT);
  digitalWrite(LED_GPIO_NUM, HIGH);
#endif

  // BLOCKING LOOP for Setup Mode
  while (true) {
    dnsServer.processNextRequest();
    setupServer.handleClient();

    // Simple non-blocking LED blink
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 200) {
      lastBlink = millis();
#ifdef LED_GPIO_NUM
#if LED_GPIO_NUM != 48
      digitalWrite(LED_GPIO_NUM, !digitalRead(LED_GPIO_NUM));
#else
      static bool s = false;
      s = !s;
      neopixelWrite(LED_GPIO_NUM, 0, 0, s ? 50 : 0);
#endif
#endif
    }

    handleFactoryResetButton();
    delay(5); // Yield
  }
}

// ============================================================================
// MQTT Logic
// ============================================================================

// Forward declaration
void ackCommand(String cmdId);

// mqttCallback, getMQTTHost, setupMQTT moved to mqtt_manager.cpp

void startCameraServer() {
  setupWebRoutes(setupServer, apiKey);
  setupServer.begin();
}

// connectToWiFi removal logic
// connectToWiFi moved to wifi_manager.cpp

bool attemptSelfRegistration() {
  if (userToken.length() == 0)
    return false;

  currentState = STATE_ACTIVATING;
  HTTPClient http;

  // 1. Register Device directly using Token
  DEBUG_PRINTLN("Attempting registration with token");

  http.begin(serverURL + "/dev/register");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + userToken); // Use User Token

  String finalName = deviceName;
  if (finalName.length() == 0)
    finalName = String(DEVICE_MODEL);

  String regPayload = "{";
  regPayload += "\"device_uid\":\"" + deviceUID + "\",";
  regPayload += "\"device_name\":\"" + finalName + "\",";
  regPayload += "\"device_type\":\"camera\"";
  regPayload += "}";

  DEBUG_PRINTLN("Sending Registration Payload: ");
  DEBUG_PRINTLN(regPayload);

  int code = http.POST(regPayload);
  DEBUG_PRINT("Registration Response Code: ");
  DEBUG_PRINTLN(code);
  String resp = http.getString();
  DEBUG_PRINT("Registration Response Body: ");
  DEBUG_PRINTLN(resp);
  http.end();

  DEBUG_PRINTLN("Register Code: " + String(code));

  if (code == 201 || code == 200) {
    String did = extractJsonVal(resp, "device_id");
    String key = extractJsonVal(resp, "api_key");
    if (did.length() > 0 && key.length() > 0) {
      saveActivation(did, key);
      return true;
    }
  }
  return false;
}

bool activateProvisioning() {
  currentState = STATE_ACTIVATING;
  HTTPClient http;

  // Call /provisioning/activate endpoint with Device UID
  // The server checks if there is a pending provisioning request for this UID
  http.begin(serverURL + "/prov/activate");
  http.addHeader("Content-Type", "application/json");

  // We send UID, Firmware Version, and Model
  String payload = "{";
  payload += "\"device_uid\":\"" + deviceUID + "\",";
  payload += "\"firmware_version\":\"" + String(FIRMWARE_VERSION) + "\",";
  payload += "\"model\":\"" + String(DEVICE_MODEL) + "\"";
  payload += "}";

  DEBUG_PRINTLN("Activating: " + payload);

  int code = http.POST(payload);
  String resp = http.getString();
  http.end();

  DEBUG_PRINTLN("Activation Code: " + String(code));
  DEBUG_PRINTLN("Response: " + resp);

  if (code == 200) {
    String did = extractJsonVal(resp, "device_id");
    String key = extractJsonVal(resp, "api_key");
    if (did.length() > 0 && key.length() > 0) {
      saveActivation(did, key);
      return true;
    }
  }
  return false;
}

// Helper to ACK commands so they don't loop
// ackCommand moved to mqtt_manager.cpp

// Telemetry Configuration
// Telemetry moved to mqtt_manager.cpp

// checkCommands removed (conflicted with MQTT)

#include <WiFiClientSecure.h>

// Global Clients
WiFiClientSecure sslClient;
WiFiClient tcpClient;
Client *msgClient = NULL; // Pointer to active client
bool uploadClientConnected = false;

void uploadFrame(camera_fb_t *fb) {
  // 0. Parse Server URL
  String protocol = "https";
  String host = "datum.bezg.in";
  int port = 443;

  int protoEnd = serverURL.indexOf("://");
  if (protoEnd != -1) {
    protocol = serverURL.substring(0, protoEnd);
    int portStart = serverURL.indexOf(":", protoEnd + 3);
    int pathStart = serverURL.indexOf("/", protoEnd + 3);

    if (pathStart == -1)
      pathStart = serverURL.length();

    if (portStart != -1 && portStart < pathStart) {
      host = serverURL.substring(protoEnd + 3, portStart);
      port = serverURL.substring(portStart + 1, pathStart).toInt();
    } else {
      host = serverURL.substring(protoEnd + 3, pathStart);
      port = (protocol == "http") ? 80 : 443;
    }
  }

  // 1. Select Client & Connect
  bool isSecure = (protocol == "https");
  if (isSecure) {
    msgClient = &sslClient;
    sslClient.setInsecure();
  } else {
    msgClient = &tcpClient;
  }

  if (!msgClient->connected()) {
    Serial.printf("[UPLOAD] Connecting to %s:%d (%s)...\n", host.c_str(), port,
                  protocol.c_str());
    if (!msgClient->connect(host.c_str(), port)) {
      Serial.println("[UPLOAD] Connection failed!");
      uploadClientConnected = false;
      return;
    }
    Serial.println("[UPLOAD] Connected!");
    uploadClientConnected = true;
  }

  // 2. Send HTTP Headers (Keep-Alive!)
  String head = "";
  head += "POST /dev/" + deviceID + "/stream/frame HTTP/1.1\r\n";
  head += "Host: " + host + "\r\n";
  head += "Authorization: Bearer " + apiKey + "\r\n";
  head += "Content-Type: image/jpeg\r\n";
  head += "Content-Length: " + String(fb->len) + "\r\n";
  head += "Connection: keep-alive\r\n";
  head += "\r\n";

  msgClient->print(head);

  // 3. Send Body (Image Data)
  const uint8_t *data = fb->buf;
  size_t len = fb->len;
  size_t written = 0;
  size_t chunkSize = 1024;

  while (written < len) {
    size_t toWrite = (len - written) > chunkSize ? chunkSize : (len - written);
    size_t r = msgClient->write(data + written, toWrite);
    if (r == 0) {
      Serial.println("[UPLOAD] Write failed/timeout");
      msgClient->stop();
      return;
    }
    written += r;
  }

  // 4. Read Response (Headers only)
  unsigned long timeout = millis();
  while (msgClient->connected()) {
    if (msgClient->available()) {
      String line = msgClient->readStringUntil('\n');
      if (line == "\r")
        break; // End of headers
      if (millis() - timeout > 2000) {
        msgClient->stop(); // Timeout
        return;
      }
    } else {
      if (millis() - timeout > 500)
        break; // Short wait for Ack
      delay(1);
    }
  }

  // Drain body if small
  while (msgClient->available()) {
    msgClient->read();
  }
}

// Motion: Check using EIF module (Grayscale Diff)
// Camera Logic (checkMotion, processCameraLoop, cameraTaskLoop,
// startCameraTask, handleSnap) moved to camera_manager.cpp

// Load Settings from NVS
void loadStartupSettings() {
  prefs.begin("datum", true);

  // 1. Motion Settings
  motionEnabled = prefs.getBool("pref_mot", true);
  int sens = prefs.getInt("pref_msens", 50);
  motionThreshold = map(sens, 0, 100, 60, 5);
  // Area: 0 -> 20%, 100 -> 1%
  float areaMin = 1.0;
  float areaMax = 20.0;
  motionMinAreaPct = areaMax - ((float)sens / 100.0 * (areaMax - areaMin));
  int per = prefs.getInt("pref_mper", 1);
  motionPeriodMs = per * 1000;
  if (motionPeriodMs < 500)
    motionPeriodMs = 500;

  // 2. LED Settings
  String color = prefs.getString("pref_lcol", "#FFFFFF");
  if (color.startsWith("#")) {
    long number = strtol(&color.c_str()[1], NULL, 16);
    savedR = number >> 16;
    savedG = number >> 8 & 0xFF;
    savedB = number & 0xFF;
  }
  savedBrightness = prefs.getInt("pref_lbri", 100);
  torchState = prefs.getBool("pref_led_on", false); // Default OFF for safety

  // Apply LED State at Boot
  int r = 0, g = 0, b = 0;
  if (torchState) {
    r = (savedR * savedBrightness) / 100;
    g = (savedG * savedBrightness) / 100;
    b = (savedB * savedBrightness) / 100;
  }
  neopixelWrite(48, r, g, b); // LED_GPIO_NUM for Freenove S3

  // 3. Orientation
  bool mir = prefs.getBool("pref_imir", true);   // Default Mirror on
  bool flip = prefs.getBool("pref_iflip", true); // Default Flip on

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_hmirror(s, mir ? 1 : 0);
    s->set_vflip(s, flip ? 1 : 0);
  }

  prefs.end();
  Serial.println("--- Startup Settings Loaded ---");
  Serial.printf("Motion: %s, Sens: %d, Area: >%.1f%%, Period: %d ms\n",
                motionEnabled ? "ON" : "OFF", sens, motionMinAreaPct,
                motionPeriodMs);
  Serial.printf("LED: %s, Bri: %d, Power: %s\n", color.c_str(), savedBrightness,
                torchState ? "ON" : "OFF");
  Serial.printf("Orientation: Mirror %s, Flip %s\n", mir ? "ON" : "OFF",
                flip ? "ON" : "OFF");

  if (s) {
    int ires = prefs.getInt("pref_ires", 10); // Default 10 (UXGA/HD) if not set
    Serial.printf("Resolution: Stream %u (VGA=8), Snapshot %u\n",
                  s->status.framesize, ires);
  }
}

// Setup logic with late camera init
void setup() {
  setCpuFrequencyMhz(160); // Reduce CPU freq to 160MHz for power saving
  Serial.begin(115200);
  delay(1000); // Wait for Serial
  Serial.println("\n\n--- BOOTING DATUM CAMERA ---");
#ifdef LED_GPIO_NUM
  pinMode(LED_GPIO_NUM, OUTPUT);
#endif

  // checkFactoryReset(); // Removed: Calling this at boot triggers
  // Download Mode. Moved to loop.

  initDeviceUID();
  loadCredentials();

  // If no credentials, start AP immediately (and init camera for
  // preview)
  if (wifiSSID.length() == 0) {
    initCamera();
    loadStartupSettings();
    startSetupMode();
    return;
  }

  // Try to connect to WiFi FIRST (without camera active)
  if (connectToWiFiBlocking(30, handleFactoryResetButton)) {
    justConnected = true; // Set flag for telemetry

    // Sync time and set server URL
    onWifiConnected();

    // Connection successful, NOW init camera
    initCamera();
    loadStartupSettings();

    // Check activation
    if (apiKey.length() == 0 && userToken.length() > 0) {
      Serial.println(
          "[SETUP] No API Key, attempting registration with User Token...");
      if (attemptSelfRegistration()) {
        Serial.println(
            "[SETUP] Registration Successful! New API Key acquired.");
      } else {
        Serial.println(
            "[SETUP] Registration Failed. Will retry later or check logs.");
        // Optional: Panic blink?
      }
    }

    if (apiKey.length() > 0) {
      currentState = STATE_ONLINE;
      setupMQTT();

      // Init SD Card & Web Routes
      if (initSD()) {
        DEBUG_PRINTLN("SD Card Initialized");
      }
      setupWebRoutes(setupServer, apiKey);
      setupServer.begin(); // !!! CRITICAL: Start the web server !!!
      Serial.println("[WEB] Server started on port 80");

      // Create Network Task (Core 0) - Handles WiFi, MQTT, Web Server
      xTaskCreatePinnedToCore(networkTask,        /* Task function */
                              "NetworkTask",      /* Name */
                              16384,              /* Stack size */
                              NULL,               /* Parameters */
                              1,                  /* Priority */
                              &networkTaskHandle, /* Handle */
                              0                   /* Core 0 (Pro Core) */
      );

      // Start Camera Task (Core 1)
      startCameraTask();

      // Start Async Upload Task (Core 0) for non-blocking frame uploads
      startUploadTask();
    } else {
      Serial.println("[SETUP] Device not activated. Waiting for config...");
      // Fallback: Start web server anyway so user can re-configure if needed?
      // Or just wait in loop.
    }
  } // End if(connected)
} // End setup()

// Main Loop
void loop() {
  handleFactoryResetButton();
  vTaskDelay(20 / portTICK_PERIOD_MS);
}

// Network Task - Core 0
void networkTask(void *pvParameters) {
  unsigned long lastHeartbeat = 0;
  for (;;) {
    // Heartbeat removed as per user request

    // 1. Connection State Machine
    if (WiFi.status() == WL_CONNECTED) {
      processMqttLoop();
      checkCommands();
    }

    // Handle WiFi Reconnection
    handleWiFiLoop();

    // 2. Web Server (Online or Setup Mode)
    // NOTE: In Setup Mode, setupServer IS the main server.
    // In Online Mode, setupServer is ALSO the main server (renamed
    // conceptually?). Yes, 'setupServer' variable is used for both.
    setupServer.handleClient();

    // 3. LED & Button
    updateLED();
    handleFactoryResetButton();

    vTaskDelay(10 / portTICK_PERIOD_MS); // Yield to IDLE task
  }
}
