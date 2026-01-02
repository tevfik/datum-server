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

#include "Arduino.h"
#include "esp_camera.h"
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

// Debug Configuration
// Uncomment the following line to enable detailed debug logs
// #define DEBUG_MODE

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
#define LED_GPIO_NUM 2
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
String userEmail;
String userPass;

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
unsigned long lastCommandCheck = 0;
unsigned long setupStartTime = 0;
unsigned long lastLedBlink = 0;
bool ledState = false;

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
  int s = json.indexOf("\"" + key + "\":\"");
  if (s < 0)
    return "";
  s += key.length() + 4;
  int e = json.indexOf("\"", s);
  return json.substring(s, e);
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
    digitalWrite(LED_GPIO_NUM, HIGH);
#endif
    return;
  default:
    blinkInterval = 1000;
  }
  if (now - lastLedBlink >= blinkInterval) {
    ledState = !ledState;
#ifdef LED_GPIO_NUM
    digitalWrite(LED_GPIO_NUM, ledState ? HIGH : LOW);
#endif
    lastLedBlink = now;
  }
}

void loadCredentials() {
  prefs.begin("datum", true);
  apiKey = prefs.getString("api_key", "");
  serverURL = prefs.getString("server_url", "");
  wifiSSID = prefs.getString("wifi_ssid", "");
  wifiPass = prefs.getString("wifi_pass", "");
  deviceID = prefs.getString("device_id", "");
  userEmail = prefs.getString("user_email", "");
  userPass = prefs.getString("user_pass", "");
  prefs.end();
}

void saveCredentials(String u, String s, String p, String email, String pwd) {
  prefs.begin("datum", false);
  prefs.putString("server_url", u);
  prefs.putString("wifi_ssid", s);
  prefs.putString("wifi_pass", p);
  prefs.putString("user_email", email);
  prefs.putString("user_pass", pwd);
  prefs.end();
  serverURL = u;
  wifiSSID = s;
  wifiPass = p;
  userEmail = email;
  userPass = pwd;
}

void saveActivation(String id, String key) {
  prefs.begin("datum", false);
  prefs.putString("device_id", id);
  prefs.putString("api_key", key);
  // Clear temp user creds
  prefs.remove("user_email");
  prefs.remove("user_pass");
  prefs.end();
  deviceID = id;
  apiKey = key;
  userEmail = "";
  userPass = "";
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
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }
  return esp_camera_init(&config) == ESP_OK;
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
    digitalWrite(LED_GPIO_NUM, HIGH);
    delay(200);
    digitalWrite(LED_GPIO_NUM, LOW);
#endif
    setupServer.send(200, "text/plain", "OK");
  } else if (type == "restart") {
    setupServer.send(200, "text/plain", "Restarting...");
    delay(100);
    ESP.restart();
  } else
    setupServer.send(400, "text/plain", "Unknown");
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

  saveCredentials(u, s, p, "",
                  ""); // Clear user creds as they are not needed on device

  setupServer.send(200, "application/json",
                   "{\"status\":\"success\",\"message\":\"Credentials saved. "
                   "Restarting...\"}");
  delay(500);
  ESP.restart();
}

void handleConfigure() {
  String u = setupServer.arg("server_url");
  String s = setupServer.arg("wifi_ssid");
  String p = setupServer.arg("wifi_pass");
  String email = setupServer.arg("user_email");
  String pwd = setupServer.arg("user_password");

  if (u.length() == 0 || s.length() == 0 || email.length() == 0) {
    setupServer.send(400, "text/plain", "Missing fields");
    return;
  }
  saveCredentials(u, s, p, email, pwd);
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

void checkCommands() {
  if (millis() - lastCommandCheck < 5000)
    return;
  lastCommandCheck = millis();
  if (WiFi.status() != WL_CONNECTED || apiKey.length() == 0)
    return;

  HTTPClient http;
  http.begin(serverURL + "/device/" + deviceID + "/commands");
  http.addHeader("Authorization", "Bearer " + apiKey);
  if (http.GET() == 200) {
    String pl = http.getString();
    if (pl.indexOf("start-stream") > 0)
      streaming = true;
    else if (pl.indexOf("stop-stream") > 0)
      streaming = false;
    else if (pl.indexOf("restart") > 0)
      ESP.restart();
  }
  http.end();
}

void streamLoop() {
  if (!streaming || millis() - lastFrameTime < 100)
    return;
  lastFrameTime = millis();
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
    return;

  HTTPClient http;
  http.begin(serverURL + "/device/" + deviceID + "/stream/frame");
  http.addHeader("Authorization", "Bearer " + apiKey);
  http.addHeader("Content-Type", "image/jpeg");
  http.POST(fb->buf, fb->len);
  http.end();
  esp_camera_fb_return(fb);
}

// Setup logic with late camera init
void setup() {
  Serial.begin(115200);
#ifdef LED_GPIO_NUM
  pinMode(LED_GPIO_NUM, OUTPUT);
#endif

  // checkFactoryReset(); // Removed: Calling this at boot triggers Download
  // Mode. Moved to loop.

  initDeviceUID();
  loadCredentials();

  // If no credentials, start AP immediately (and init camera for preview)
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
    if (apiKey.length() > 0 || activateProvisioning()) {
      currentState = STATE_ONLINE;
      // Optional: Keep setup server running for reconfiguration?
      // startSetupMode();
      startCameraServer(); // Start streaming server
      streaming = true;    // Auto start stream if configured
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
  // Auto reconnect logic could be here
  delay(1);
}
