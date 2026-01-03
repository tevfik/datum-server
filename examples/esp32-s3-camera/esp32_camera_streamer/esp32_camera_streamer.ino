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

#include "esp_camera.h"
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Preferences.h>

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
#define CAMERA_MODEL_FREENOVE_S3

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
#if defined(CAMERA_MODEL_ESP32S3_CAM)
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 10
#define SIOD_GPIO_NUM 40
#define SIOC_GPIO_NUM 39
#define Y9_GPIO_NUM 48
#define Y8_GPIO_NUM 11
#define Y7_GPIO_NUM 12
#define Y6_GPIO_NUM 14
#define Y5_GPIO_NUM 16
#define Y4_GPIO_NUM 18
#define Y3_GPIO_NUM 17
#define Y2_GPIO_NUM 15
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM 47
#define PCLK_GPIO_NUM 13
#define LED_GPIO_NUM 21
#elif defined(CAMERA_MODEL_FREENOVE_S3)
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 15
#define SIOD_GPIO_NUM 4
#define SIOC_GPIO_NUM 5
#define Y9_GPIO_NUM 16
#define Y8_GPIO_NUM 17
#define Y7_GPIO_NUM 18
#define Y6_GPIO_NUM 12
#define Y5_GPIO_NUM 10
#define Y4_GPIO_NUM 8
#define Y3_GPIO_NUM 9
#define Y2_GPIO_NUM 11
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM 7
#define PCLK_GPIO_NUM 13
#define LED_GPIO_NUM 48
#elif defined(CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22
#define LED_GPIO_NUM 4
#else
#error "Camera model not selected!"
#endif

// ============================================================================
// Globals
// ============================================================================
Preferences prefs;
WebServer setupServer(SETUP_HTTP_PORT);

String deviceUID;
String deviceMAC;
String apiKey;
String serverURL;
String wifiSSID;
String wifiPass;
String deviceID;
String userToken;  // Store token directly
String deviceName; // Custom device name

enum DeviceState {
  STATE_BOOT,
  STATE_SETUP_MODE,
  STATE_CONNECTING,
  STATE_ACTIVATING,
  STATE_ONLINE,
  STATE_OFFLINE
};
DeviceState currentState = STATE_BOOT;

bool streaming = false; // Cloud streaming state
unsigned long lastFrameTime = 0;
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// Forward Declarations
void handleSnap(String resolution);
framesize_t getFrameSizeFromName(String name);

// Task handles
TaskHandle_t streamTask;
const int COMMAND_POLL_INTERVAL_MS = 5000;
unsigned long lastCommandPoll = 0;

void updateFirmware(String url) {
  if (url.length() == 0)
    return;

  DEBUG_PRINTLN("Starting OTA Update...");
  DEBUG_PRINT("Firmware URL: ");
  DEBUG_PRINTLN(url);

  // Stop camera to free memory
  esp_camera_deinit();

  // Disable WDT to prevent timeouts during large downloads
  disableCore0WDT();

  WiFiClient client;
  // Set 60s timeout for download
  client.setTimeout(60);

  // Add progress callback
  httpUpdate.onProgress([](int cur, int total) {
    Serial.printf("OTA Progress: %d%%\n", (cur * 100) / total);
  });

  t_httpUpdate_return ret = httpUpdate.update(client, url);

  switch (ret) {
  case HTTP_UPDATE_FAILED:
    Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n",
                  httpUpdate.getLastError(),
                  httpUpdate.getLastErrorString().c_str());
    // Reboot to try to recover camera
    ESP.restart();
    break;

  case HTTP_UPDATE_NO_UPDATES:
    Serial.println("HTTP_UPDATE_NO_UPDATES");
    break;

  case HTTP_UPDATE_OK:
    Serial.println("HTTP_UPDATE_OK");
    // Auto-restarts on success
    break;
  }
}
unsigned long setupStartTime = 0;
unsigned long lastLedBlink = 0;
bool ledState = false;
bool torchState = false;
byte savedR = 255;
byte savedG = 255;
byte savedB = 255;
int savedBrightness = 0;

HTTPClient httpCommand;
HTTPClient httpStream;

bool isActivated() { return apiKey.length() > 0; }

// ============================================================================
// Web Interface (ESP-DASH Style)
// ============================================================================
const char DASHBOARD_HTML[] PROGMEM =
    R"rawliteral(<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>Datum Camera</title><style>body{background:#1b1b1b;color:white;font-family:sans-serif;margin:0;padding:20px}.card{background:#2d2d2d;padding:20px;margin-bottom:20px;border-radius:8px}.btn{background:#00bcd4;color:white;border:none;padding:10px;width:100%;border-radius:4px;font-size:16px;cursor:pointer;margin-top:5px}.btn-dan{background:#f44336}input{width:100%;padding:10px;margin:5px 0 15px;box-sizing:border-box;background:#444;border:none;color:white;border-radius:4px}img{width:100%;max-width:640px;display:block;margin:0 auto;background:black;border-radius:4px}.info{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:10px}.info div{background:#333;padding:10px;border-radius:4px;text-align:center}label{display:block;margin-bottom:5px;color:#aaa;font-size:14px}</style></head><body><h1>📷 Datum Camera</h1><div class="card"><img id="stream" src="/capture"><div class="info"><div>Signal<br><b id="rssi">-</b></div><div>Uptime<br><b id="uptime">0s</b></div></div></div><div class="card"><h2>📡 Configuration</h2><form action="/configure" method="POST"><label>Server URL</label><input type="url" name="server_url" placeholder="https://..." required><label>WiFi SSID</label><input type="text" name="wifi_ssid" required><label>WiFi Password</label><input type="password" name="wifi_pass"><hr style="border-color:#444;margin:20px 0"><label>User Email</label><input type="email" name="user_email" required><label>User Password</label><input type="password" name="user_password" required><button type="submit" class="btn">Save & Restart</button></form></div><div class="card"><h2>⚡ Controls</h2><div style="display:flex;gap:10px"><button class="btn" onclick="fetch('/action?type=led')">Toggle LED</button><button class="btn btn-dan" onclick="if(confirm('Reboot?')) fetch('/action?type=restart')">Restart</button></div></div><script>const i=document.getElementById('stream');const l=()=>{setTimeout(()=>{i.src='/capture?t='+Date.now()},200)};i.onload=l;i.onerror=l;document.querySelector('form').onsubmit=(e)=>{e.preventDefault();const b=document.querySelector('button[type=submit]');b.disabled=true;b.innerText='Saving...';fetch('/configure',{method:'POST',body:new FormData(e.target)}).then(()=>{document.body.innerHTML='<div style="text-align:center;margin-top:50px"><h1>🔄 Restarting...</h1><p>Please connect to your WiFi network.</p></div>'}).catch(e=>alert('Error: '+e))};function update(){fetch('/info').then(r=>r.json()).then(d=>{document.getElementById('rssi').innerText='Active'}).catch(e=>console.log(e))}let s=0;setInterval(()=>{document.getElementById('uptime').innerText=Math.floor(++s/60)+'m '+(s%60)+'s'},1000);setInterval(update,5000);update()</script></body></html>)rawliteral";

// ============================================================================
// Core Functions
// ============================================================================

String extractJsonVal(String json, String key) {
  int keyIdx = json.indexOf("\"" + key + "\"");
  if (keyIdx < 0)
    return "";

  // Find the colon after key
  int colonIdx = json.indexOf(":", keyIdx);
  if (colonIdx < 0)
    return "";

  // Find start of value (first quote after colon)
  int valStart = json.indexOf("\"", colonIdx);
  if (valStart < 0)
    return "";

  // Find end of value
  int valEnd = json.indexOf("\"", valStart + 1);
  if (valEnd < 0)
    return "";

  return json.substring(valStart + 1, valEnd);
}

// Helper to extract nested JSON object
String extractNestedJsonVal(String json, String parentKey, String childKey) {
  int parentIdx = json.indexOf("\"" + parentKey + "\":");
  if (parentIdx < 0)
    return "";

  // Find start of object (look for { after parent key)
  int objStart = json.indexOf("{", parentIdx);
  if (objStart < 0)
    return "";

  // Find end of object (simple heuristic: first })
  int objEnd = json.indexOf("}", objStart);
  if (objEnd < 0)
    return "";

  String nested = json.substring(objStart, objEnd + 1);
  return extractJsonVal(nested, childKey);
}

// Helper to extract numeric JSON value (robust)
int extractJsonInt(String json, String key) {
  int keyIdx = json.indexOf("\"" + key + "\"");
  if (keyIdx < 0)
    return -1;

  int colonIdx = json.indexOf(":", keyIdx);
  if (colonIdx < 0)
    return -1;

  // Skip whitespace/quotes/brackets to find start of number
  int s = colonIdx + 1;
  while (s < json.length() && !isDigit(json.charAt(s)) &&
         json.charAt(s) != '-') {
    s++;
  }

  if (s >= json.length())
    return -1;

  int e = s;
  while (e < json.length() &&
         (isDigit(json.charAt(e)) || json.charAt(e) == '-')) {
    e++;
  }
  return json.substring(s, e).toInt();
}

// Helper to extract boolean JSON value (true/false or 1/0)
bool extractJsonBool(String json, String key) {
  int keyIdx = json.indexOf("\"" + key + "\"");
  if (keyIdx < 0)
    return false;

  int colonIdx = json.indexOf(":", keyIdx);
  if (colonIdx < 0)
    return false;

  // Look ahead for "true", "1", or "false", "0"
  String remainder = json.substring(colonIdx + 1);
  remainder.trim(); // Trim leading whitespace

  if (remainder.startsWith("true") || remainder.startsWith("1"))
    return true;
  return false;
}

// Check for Factory Reset (Boot Button held manually during runtime)
unsigned long buttonPressStart = 0;

void handleFactoryResetButton() {
  pinMode(0, INPUT_PULLUP);

  if (digitalRead(0) == LOW) {
    // Button pressed
    if (buttonPressStart == 0) {
      buttonPressStart = millis();
      Serial.println("Button Pressed... Hold for 3s to Reset");
    }

    // Check duration
    if (millis() - buttonPressStart > 3000) {
      Serial.println("Factory Reset: TRIGGERED!");

      // Visual feedback
      for (int i = 0; i < 5; i++) {
#ifdef LED_GPIO_NUM
        digitalWrite(LED_GPIO_NUM, !digitalRead(LED_GPIO_NUM));
#endif
        delay(100);
      }

      // Clear NVS
      prefs.begin("datum", false);
      prefs.clear();
      prefs.end();

      Serial.println("Factory Reset: Complete. Restarting...");
      ESP.restart();
    }
  } else {
    // Button released
    if (buttonPressStart != 0) {
      Serial.println("Button Released (Reset Cancelled)");
    }
    buttonPressStart = 0;
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

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000; // Reduced to 10MHz for OV3660 stability
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // Only capture when buffer is free

  if (psramFound()) {
    Serial.printf("PSRAM Found! Size: %d bytes, Free: %d bytes\n",
                  ESP.getPsramSize(), ESP.getFreePsram());
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 2; // Double buffering to prevent tearing/overflow
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST; // Always get the freshest frame
  } else {
    Serial.println("WARNING: No PSRAM! High-res will fail.");
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 20;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err == ESP_OK) {
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
      s->set_hmirror(s, 1);
      s->set_vflip(s, 1);
    }
  }
  return err == ESP_OK;
}

// ============================================================================
// Handlers
// ============================================================================
void handleSetupRoot() { setupServer.send(200, "text/html", DASHBOARD_HTML); }

void handleStream() {
  WiFiClient client = setupServer.client();
  String response = "HTTP/1.1 200 OK\r\nContent-Type: "
                    "multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  setupServer.sendContent(response);

  while (client.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      setupServer.sendContent("\r\n");
      break;
    }

    String head = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
                  String(fb->len) + "\r\n\r\n";
    setupServer.sendContent(head);
    client.write(fb->buf, fb->len);
    setupServer.sendContent("\r\n");
    esp_camera_fb_return(fb);
    delay(1);
  }
}

void handleAction() {
  String type = setupServer.arg("type");
  if (type == "led") {
#ifdef LED_GPIO_NUM
#if LED_GPIO_NUM == 48
    torchState = !torchState;
    neopixelWrite(LED_GPIO_NUM, torchState ? 255 : 0, torchState ? 255 : 0,
                  torchState ? 255 : 0);
#else
    digitalWrite(LED_GPIO_NUM, HIGH);
    delay(200);
    digitalWrite(LED_GPIO_NUM, LOW);
#endif
#endif
    setupServer.send(200, "text/plain", "OK");
  } else if (type == "restart") {
    setupServer.send(200, "text/plain", "Restarting...");
    delay(100);
    ESP.restart();
  } else {
    setupServer.send(400, "text/plain", "Unknown");
  }
}

void handleDeviceInfo() {
  String json = "{";
  json += "\"device_uid\":\"" + deviceUID + "\",";
  json += "\"mac_address\":\"" + deviceMAC + "\",";
  json += "\"firmware_version\":\"" + String(FIRMWARE_VERSION) + "\",";
  json += "\"status\":\"" +
          String(isActivated() ? "configured" : "unconfigured") + "\"";
  json += "}";
  setupServer.send(200, "application/json", json);
}

void handleCapture() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    setupServer.send(500, "text/plain", "Error");
    return;
  }
  setupServer.sendHeader("Content-Disposition", "inline; filename=capture.jpg");
  setupServer.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// Handle JSON provisioning from Mobile App
void handleProvision() {
  if (setupServer.method() != HTTP_POST) {
    setupServer.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  String body = setupServer.arg("plain");
  String u = extractJsonVal(body, "server_url");
  String s = extractJsonVal(body, "wifi_ssid");
  String p = extractJsonVal(body, "wifi_pass");

  if (u.length() == 0 || s.length() == 0) {
    setupServer.send(400, "application/json", "{\"error\":\"Missing fields\"}");
    return;
  }

  saveCredentials(
      u, s, p, "",
      ""); // Clear user creds and name as they are not needed on device

  setupServer.send(200, "application/json",
                   "{\"status\":\"success\",\"message\":\"Credentials saved. "
                   "Restarting...\"}");
  delay(500);
  ESP.restart();
}

// Scan for WiFi networks and return JSON list
void handleScan() {
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i)
      json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json +=
        "\"auth\":" +
        String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") +
        "}";
  }
  json += "]";
  setupServer.send(200, "application/json", json);
}

void handleConfigure() {
  String u = setupServer.arg("server_url");
  String s = setupServer.arg("wifi_ssid");
  String p = setupServer.arg("wifi_pass");
  String token = setupServer.arg("user_token");
  String name = setupServer.arg("device_name");

  if (u.length() == 0 || s.length() == 0 || token.length() == 0) {
    setupServer.send(400, "text/plain", "Missing fields");
    return;
  }
  // Save credentials including user token and device name for
  // self-registration
  saveCredentials(u, s, p, token, name);

  String html =
      R"(<html><body style='background:#1b1b1b;color:white;display:flex;justify-content:center;align-items:center;height:100vh;font-family:sans-serif'>
        <div style='background:#2d2d2d;padding:40px;border-radius:12px;text-align:center'>
        <h1>✅ Saved!</h1><p>Restarting...</p></div></body></html>)";
  setupServer.send(200, "text/html", html);
  delay(2000);
  ESP.restart();
}

void handleNotFound() {
  setupServer.sendHeader("Location", "/");
  setupServer.send(302, "text/plain", "Redirect");
}

void startSetupMode() {
  Serial.println("Starting Setup Mode"); // Always print critical state changes
  currentState = STATE_SETUP_MODE;
  setupStartTime = millis();
  WiFi.mode(WIFI_AP);
  String ap = String(SETUP_AP_PREFIX) + deviceUID.substring(8);
  WiFi.softAP(ap.c_str(), SETUP_AP_PASSWORD);

  setupServer.on("/", HTTP_GET, handleSetupRoot);
  setupServer.on("/stream", HTTP_GET, handleStream);
  setupServer.on("/capture", HTTP_GET, handleCapture);
  setupServer.on("/info", HTTP_GET, handleDeviceInfo);
  setupServer.on("/scan", HTTP_GET, handleScan); // Add scan endpoint
  setupServer.on("/action", HTTP_GET, handleAction);
  setupServer.on("/configure", HTTP_POST, handleConfigure);
  setupServer.on("/provision", HTTP_POST, handleProvision);
  setupServer.onNotFound(handleNotFound);
  setupServer.begin();
}

void startCameraServer() {
  setupServer.on("/", HTTP_GET, handleSetupRoot);
  setupServer.on("/stream", HTTP_GET, handleStream);
  setupServer.on("/capture", HTTP_GET, handleCapture);
  setupServer.on("/info", HTTP_GET, handleDeviceInfo);
  setupServer.on("/action", HTTP_GET, handleAction);
  setupServer.on("/configure", HTTP_POST, handleConfigure);
  setupServer.on("/provision", HTTP_POST, handleProvision);
  setupServer.onNotFound(handleNotFound);
  setupServer.begin();
}

bool connectToWiFi() {
  Serial.printf("Connecting to %s\n", wifiSSID.c_str());
  currentState = STATE_CONNECTING;
  WiFi.setSleep(false); // Disable power saving to prevent connection crashes
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    Serial.print(".");
    delay(500);
    updateLED();
    attempts++;
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

bool attemptSelfRegistration() {
  if (userToken.length() == 0)
    return false;

  currentState = STATE_ACTIVATING;
  HTTPClient http;

  // 1. Register Device directly using Token
  DEBUG_PRINTLN("Attempting registration with token");

  http.begin(serverURL + "/devices/register");
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
  http.begin(serverURL + "/provisioning/activate");
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
void ackCommand(String cmdId) {
  HTTPClient http;
  http.begin(serverURL + "/device/" + deviceID + "/commands/" + cmdId + "/ack");
  http.addHeader("Authorization", "Bearer " + apiKey);
  http.addHeader("Content-Type", "application/json");
  http.POST("{\"status\":\"executed\"}");
  http.end();
}

void checkCommands() {
  // Check every 1 second (was 5s) for better responsiveness
  if (millis() - lastCommandPoll < 1000)
    return;
  lastCommandPoll = millis();
  if (WiFi.status() != WL_CONNECTED || apiKey.length() == 0)
    return;

  HTTPClient http;
  http.begin(serverURL + "/device/" + deviceID + "/commands");
  http.addHeader("Authorization", "Bearer " + apiKey);
  if (http.GET() == 200) {
    String pl = http.getString();

    // Robust iteration over command objects
    int startObj = pl.indexOf('{');
    while (startObj >= 0) {
      int endObj = startObj;
      int braceCount = 1;
      while (braceCount > 0 && endObj < pl.length() - 1) {
        endObj++;
        if (pl.charAt(endObj) == '{')
          braceCount++;
        else if (pl.charAt(endObj) == '}')
          braceCount--;
      }
      if (braceCount != 0)
        break;
      String cmdJson = pl.substring(startObj, endObj + 1);

      String pid = extractJsonVal(cmdJson, "id");
      if (pid.length() == 0)
        pid = extractJsonVal(cmdJson, "command_id");
      String action = extractJsonVal(cmdJson, "action");

      String paramsBlock = "";
      int pstart = cmdJson.indexOf("\"params\":");
      if (pstart > 0) {
        int pvalStart = cmdJson.indexOf('{', pstart);
        if (pvalStart > 0) {
          int pend = pvalStart;
          int pcount = 1;
          while (pcount > 0 && pend < cmdJson.length() - 1) {
            pend++;
            if (cmdJson.charAt(pend) == '{')
              pcount++;
            else if (cmdJson.charAt(pend) == '}')
              pcount--;
          }
          if (pcount == 0)
            paramsBlock = cmdJson.substring(pvalStart, pend + 1);
        }
      }

      if (pid.length() > 0 && action.length() > 0) {
        Serial.println("Processing Command: " + pid + " Action: " + action);
        if (action == "update_settings") {
          ackCommand(pid);
          String resolution = extractJsonVal(paramsBlock, "resolution");
          String color = extractJsonVal(paramsBlock, "led_color");
          int brightness = extractJsonInt(paramsBlock, "led_brightness");
          bool hmirror = extractJsonBool(paramsBlock, "hmirror");
          bool vflip = extractJsonBool(paramsBlock, "vflip");

          Serial.println("Settings: Res=" + resolution + " Col=" + color +
                         " Bri=" + String(brightness));

          // Update Global LED State
          if (color.length() > 0) {
            long number = strtol(&color.c_str()[1], NULL, 16);
            savedR = number >> 16;
            savedG = number >> 8 & 0xFF;
            savedB = number & 0xFF;
          }
          if (brightness != -1) {
            savedBrightness = brightness;
          }

          if (resolution.length() > 0) {
            framesize_t currentSize = esp_camera_sensor_get()->status.framesize;
            framesize_t newSize = getFrameSizeFromName(resolution);
            if (newSize != currentSize) {
              bool wasStreaming = streaming;
              streaming = false;
              delay(100);
              sensor_t *s = esp_camera_sensor_get();
              s->set_framesize(s, newSize);
              streaming = wasStreaming;
            }
          }
          sensor_t *s = esp_camera_sensor_get();
          if (s) {
            s->set_hmirror(s, hmirror ? 1 : 0);
            s->set_vflip(s, vflip ? 1 : 0);
          }

          // Apply LED State Unified
          int r = (savedR * savedBrightness) / 100;
          int g = (savedG * savedBrightness) / 100;
          int b = (savedB * savedBrightness) / 100;

#ifdef LED_GPIO_NUM
#if LED_GPIO_NUM == 48
          neopixelWrite(LED_GPIO_NUM, r, g, b);
          torchState = (r + g + b > 0);
#else
          digitalWrite(LED_GPIO_NUM, (r + g + b > 0) ? HIGH : LOW);
#endif
#endif
        } else if (action == "snap") {
          ackCommand(pid);
          String snapRes = extractJsonVal(paramsBlock, "resolution");
          handleSnap(snapRes);
        } else if (action == "update_firmware") {
          ackCommand(pid);
          String firmwareUrl = extractJsonVal(paramsBlock, "url");
          updateFirmware(firmwareUrl);
        } else if (action == "restart") {
          ackCommand(pid);
          ESP.restart();
        } else if (action == "start-stream") {
          ackCommand(pid);
          streaming = true;
        } else if (action == "stop-stream") {
          ackCommand(pid);
          streaming = false;
        } else {
          ackCommand(pid);
        }
      }
      startObj = pl.indexOf('{', endObj + 1);
    }
  }
  http.end();
}

void uploadFrame(camera_fb_t *fb) {
  // Use Global httpStream to allow Keep-Alive (reuse connection)
  httpStream.setReuse(true);
  httpStream.setTimeout(15000); // 15s for high-res uploads
  httpStream.begin(serverURL + "/device/" + deviceID + "/stream/frame");
  httpStream.addHeader("Authorization", "Bearer " + apiKey);
  httpStream.addHeader("Content-Type", "image/jpeg");

  int httpCode = httpStream.POST(fb->buf, fb->len);

  if (httpCode == 200) {
    // Only print verbose logs if debugging or occasionally to reduce Serial
    // overhead? Serial.printf("[UPLOAD] OK: %d bytes sent\n", fb->len);
  } else {
    Serial.printf("[UPLOAD] FAILED! HTTP %d, size: %d bytes\n", httpCode,
                  fb->len);
  }

  // With setReuse(true), enc() should not close the TCP connection
  httpStream.end();
}

void streamLoop() {
  if (!streaming)
    return;

  // Uncapped framerate
  if (millis() - lastFrameTime < 1)
    return;

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    delay(10); // Brief wait if no frame ready
    return;
  }

  lastFrameTime = millis();
  uploadFrame(fb);
  esp_camera_fb_return(fb);
}

// Helper to get framesize from string
framesize_t getFrameSizeFromName(String name) {
  if (name == "QCIF")
    return FRAMESIZE_QCIF;
  if (name == "QVGA")
    return FRAMESIZE_QVGA;
  if (name == "CIF")
    return FRAMESIZE_CIF;
  if (name == "VGA")
    return FRAMESIZE_VGA;
  if (name == "SVGA")
    return FRAMESIZE_SVGA;
  if (name == "XGA")
    return FRAMESIZE_XGA;
  if (name == "HD")
    return FRAMESIZE_HD;
  if (name == "SXGA")
    return FRAMESIZE_SXGA;
  if (name == "UXGA")
    return FRAMESIZE_UXGA;
  if (name == "QXGA")
    return FRAMESIZE_QXGA;
  return FRAMESIZE_VGA; // Default
}

void handleSnap(String resolution = "UXGA") {
  if (resolution.length() == 0)
    resolution = "UXGA";
  Serial.println("[SNAP] Starting high-res capture (full reinit)...");
  Serial.println("[SNAP] Target Resolution: " + resolution);

  // Save current state
  framesize_t savedSize = FRAMESIZE_VGA;
  int savedMirror = 1;
  int savedFlip = 1;

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    savedSize = s->status.framesize;
    savedMirror = s->status.hmirror;
    savedFlip = s->status.vflip;
  }

  framesize_t snapSize = getFrameSizeFromName(resolution);

  unsigned long startTime = millis();

  // Pause streaming
  bool wasStreaming = streaming;
  streaming = false;
  delay(200);

  // Deinit current camera
  esp_camera_deinit();
  delay(100);

  // Reinit with HIGH RES config
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000; // Lower XCLK for stability
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  config.frame_size = snapSize; // Use requested resolution
  config.jpeg_quality = 10;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[SNAP] High-res init FAILED: 0x%x\n", err);
    initCamera();
    streaming = wasStreaming;
    return;
  }

  // Force resolution explicitly (Fix for sticky resolution issue)
  s = esp_camera_sensor_get();
  if (s) {
    s->set_framesize(s, snapSize);
    s->set_hmirror(s, savedMirror);
    s->set_vflip(s, savedFlip);
  }

  // Skip a few frames to let the sensor settle and flush old buffers
  // This is critical to ensure we get the NEW resolution, not a buffered OLD
  // frame
  Serial.println("[SNAP] Flushing buffer...");
  for (int i = 0; i < 3; i++) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb)
      esp_camera_fb_return(fb);
    delay(100);
  }

  delay(200);

  // Capture
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb || fb->len < 5000) {
    Serial.println("[SNAP] Failed to capture!");
    if (fb)
      esp_camera_fb_return(fb);
    esp_camera_deinit();
    initCamera();
    streaming = wasStreaming;
    return;
  }

  Serial.printf("[SNAP] Captured: %d bytes (%dx%d) in %dms\n", fb->len,
                fb->width, fb->height, millis() - startTime);

  // Upload directly from camera buffer (streaming is paused)
  Serial.printf("[SNAP] Uploading %d bytes...\n", fb->len);
  unsigned long uploadStart = millis();

  // Use same uploadFrame function that works for stream
  uploadFrame(fb);

  unsigned long uploadTime = millis() - uploadStart;
  Serial.printf("[SNAP] Upload completed in %dms (%.1f KB/s)\n", uploadTime,
                (fb->len / 1024.0) / (uploadTime / 1000.0));

  esp_camera_fb_return(fb);

  // Deinit high-res and restore normal camera
  esp_camera_deinit();
  delay(50);
  initCamera();

  // Restore Settings
  s = esp_camera_sensor_get();
  if (s) {
    s->set_hmirror(s, savedMirror);
    s->set_vflip(s, savedFlip);
    if (s->status.framesize != savedSize) {
      s->set_framesize(s, savedSize);
    }
    Serial.printf("[SNAP] Restored State: Res=%d Mirror=%d Flip=%d\n",
                  savedSize, savedMirror, savedFlip);
  }

  // CRITICAL: Wait before resuming stream!
  // The server only stores the "last frame". If we send a new low-res
  // frame immediately, the client won't have time to fetch the high-res
  // snapshot we just uploaded.
  if (wasStreaming) {
    Serial.println("[SNAP] Waiting for client to fetch image...");
    delay(4000); // Give client 4 seconds to download
    streaming = true;
  }

  Serial.printf("[SNAP] Complete. Total time: %dms\n", millis() - startTime);
}

// Setup logic with late camera init
void setup() {
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
    startSetupMode();
    return;
  }

  // Try to connect to WiFi FIRST (without camera active)
  if (connectToWiFi()) {
    // Connection successful, NOW init camera
    initCamera();

    // Check activation
    if (apiKey.length() > 0) {
      currentState = STATE_ONLINE;
      startCameraServer();
      streaming = true;
    } else if (attemptSelfRegistration()) { // Try self registration if
                                            // we have user creds
      currentState = STATE_ONLINE;
      startCameraServer();
      streaming = true;
    } else if (activateProvisioning()) { // Fallback to old provisioning
                                         // if pending request exists
      currentState = STATE_ONLINE;
      startCameraServer();
      streaming = true;
    } else {
      currentState = STATE_OFFLINE;
      // Activation failed, go to setup mode
      startSetupMode();
    }
  } else {
    // Connection failed
    initCamera();
    startSetupMode();
  }
}

void loop() {
  handleFactoryResetButton(); // Check button every loop
  updateLED();
  if (currentState == STATE_SETUP_MODE || currentState == STATE_ONLINE) {
    setupServer.handleClient();
  }
  if (currentState == STATE_ONLINE) {
    checkCommands();
    streamLoop();
  }

  // Dynamic Delay for Performance vs Power
  if (currentState == STATE_ONLINE && streaming) {
    // Zero delay when streaming for max FPS
    // (httpStream.POST yields internally)
  } else {
    // Delay when idle or setup to save power
    delay(10);
  }
}
