/**
 * ESP32-S3 Camera Streamer with WiFi Provisioning Portal
 *
 * This firmware combines camera streaming with zero-touch WiFi provisioning.
 * When no WiFi is configured, it starts an AP with a captive portal for setup.
 *
 * FEATURES:
 * - WiFi AP provisioning (Datum-Camera-XXXX hotspot)
 * - Live camera preview in setup portal
 * - Real-time frame streaming to Datum server
 * - Command-based stream control
 * - OV2640/OV3660 sensor support
 *
 * PROVISIONING FLOW:
 * 1. Device boots without credentials → enters Setup Mode
 * 2. Device creates WiFi AP: "Datum-Camera-XXXX"
 * 3. User connects phone/laptop to this AP
 * 4. Open browser → captive portal shows camera preview
 * 5. Enter WiFi credentials and server URL
 * 6. Device saves config, restarts, connects to WiFi
 * 7. Device activates with Datum server
 * 8. Normal streaming operation begins
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

// ============================================================================
// Board Selection - Uncomment your board
// ============================================================================
// #define CAMERA_MODEL_AI_THINKER      // ESP32-CAM AI-Thinker
#define CAMERA_MODEL_ESP32S3_CAM // ESP32-S3-CAM (Seeed XIAO, Waveshare)
// #define CAMERA_MODEL_FREENOVE_S3      // Freenove ESP32-S3 WROOM CAM

// ============================================================================
// Firmware Configuration
// ============================================================================
#define FIRMWARE_VERSION "2.0.0"
#define DEVICE_MODEL "datum-camera-s3"

// Stream settings
#define STREAM_FPS 10
#define JPEG_QUALITY 12
#define FRAME_SIZE FRAMESIZE_VGA

// Provisioning settings
#define SETUP_AP_PREFIX "Datum-Camera-"
#define SETUP_AP_PASSWORD ""    // Open network for easy setup
#define SETUP_TIMEOUT_MS 300000 // 5 minutes timeout
#define SETUP_HTTP_PORT 80

// ============================================================================
// Pin Definitions - ESP32-S3-CAM
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
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22
#define LED_GPIO_NUM 4
#else
#error "Camera model not selected!"
#endif

// ============================================================================
// Global Variables
// ============================================================================
Preferences prefs;
WebServer setupServer(SETUP_HTTP_PORT);

// Device identification
String deviceUID;
String deviceMAC;

// Credentials
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

// Streaming state
bool streaming = false;
unsigned long lastFrameTime = 0;
unsigned long lastCommandCheck = 0;
unsigned long setupStartTime = 0;
unsigned long lastLedBlink = 0;
bool ledState = false;
int frameCount = 0;

HTTPClient httpCommand;
HTTPClient httpStream;

// ============================================================================
// LED Patterns
// ============================================================================
void updateLED() {
  unsigned long now = millis();
  int blinkInterval = 1000;

  switch (currentState) {
  case STATE_SETUP_MODE:
    blinkInterval = 100; // Fast blink - setup mode
    break;
  case STATE_CONNECTING:
    blinkInterval = 250; // Medium blink
    break;
  case STATE_ACTIVATING:
    blinkInterval = 500; // Slow blink
    break;
  case STATE_ONLINE:
#ifdef LED_GPIO_NUM
    digitalWrite(LED_GPIO_NUM, HIGH); // Solid on
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

// ============================================================================
// Credential Management
// ============================================================================
void loadCredentials() {
  prefs.begin("datum", true);
  apiKey = prefs.getString("api_key", "");
  serverURL = prefs.getString("server_url", "");
  wifiSSID = prefs.getString("wifi_ssid", "");
  wifiPass = prefs.getString("wifi_pass", "");
  deviceID = prefs.getString("device_id", "");
  prefs.end();

  Serial.println("Loaded credentials:");
  Serial.printf("  Server: %s\n", serverURL.c_str());
  Serial.printf("  WiFi: %s\n", wifiSSID.c_str());
  Serial.printf("  Device ID: %s\n", deviceID.c_str());
  Serial.printf("  Has API Key: %s\n", apiKey.length() > 0 ? "yes" : "no");
}

void saveCredentials(String newServerURL, String newWifiSSID,
                     String newWifiPass) {
  prefs.begin("datum", false);
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
  delay(1000);
  ESP.restart();
}

bool hasCredentials() {
  return wifiSSID.length() > 0 && serverURL.length() > 0;
}

bool isActivated() { return apiKey.length() > 0; }

// ============================================================================
// Device UID
// ============================================================================
void initDeviceUID() {
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);

  char uidBuf[13];
  sprintf(uidBuf, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3],
          mac[4], mac[5]);
  deviceUID = String(uidBuf);

  char macBuf[18];
  sprintf(macBuf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
          mac[3], mac[4], mac[5]);
  deviceMAC = String(macBuf);

  Serial.printf("Device UID: %s\n", deviceUID.c_str());
}

// ============================================================================
// Camera Initialization
// ============================================================================
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
    config.frame_size = FRAME_SIZE;
    config.jpeg_quality = JPEG_QUALITY;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_brightness(s, 0);
  s->set_contrast(s, 0);
  s->set_saturation(s, 0);
  s->set_whitebal(s, 1);
  s->set_exposure_ctrl(s, 1);
  s->set_gain_ctrl(s, 1);

  Serial.println("Camera initialized");
  return true;
}

// ============================================================================
// Web Interface (ESP-DASH Style)
// ============================================================================
const char *DASHBOARD_HTML =
    R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Datum Device Dashboard</title>
    <style>
        :root {
            --bg-color: #1b1b1b;
            --card-bg: #2d2d2d;
            --text-primary: #ffffff;
            --text-secondary: #a0a0a0;
            --accent-color: #00bcd4;
            --success-color: #4caf50;
            --warning-color: #ff9800;
            --danger-color: #f44336;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Oxygen, Ubuntu, Cantarell, "Open Sans", "Helvetica Neue", sans-serif; }
        body { background-color: var(--bg-color); color: var(--text-primary); padding: 20px; line-height: 1.6; }
        .container { max-width: 1200px; margin: 0 auto; }
        
        /* Header */
        header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 30px; border-bottom: 1px solid #333; padding-bottom: 15px; }
        .brand { font-size: 24px; font-weight: 700; color: var(--accent-color); display: flex; align-items: center; gap: 10px; }
        .status-badge { background: var(--card-bg); padding: 5px 12px; border-radius: 20px; font-size: 14px; display: flex; align-items: center; gap: 8px; }
        .dot { width: 8px; height: 8px; border-radius: 50%; background: var(--text-secondary); }
        .dot.online { background: var(--success-color); box-shadow: 0 0 10px var(--success-color); }
        
        /* Grid Layout */
        .dashboard-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
        .card { background: var(--card-bg); border-radius: 12px; padding: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); transition: transform 0.2s; }
        .card h2 { font-size: 18px; color: var(--text-secondary); margin-bottom: 15px; display: flex; align-items: center; gap: 10px; }
        .card h2 svg { width: 20px; height: 20px; stroke: var(--accent-color); }
        
        /* Camera Preview */
        .preview-container { text-align: center; background: #000; border-radius: 8px; overflow: hidden; position: relative; min-height: 240px; display: flex; align-items: center; justify-content: center; }
        img#camera-stream { width: 100%; height: auto; display: block; }
        .refresh-btn { position: absolute; bottom: 10px; right: 10px; background: rgba(0,0,0,0.7); border: none; color: white; padding: 8px; border-radius: 50%; cursor: pointer; transition: 0.3s; }
        .refresh-btn:hover { background: var(--accent-color); }
        
        /* Stats Grid */
        .stats-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }
        .stat-item { background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; }
        .stat-value { font-size: 20px; font-weight: bold; }
        .stat-label { font-size: 12px; color: var(--text-secondary); }
        
        /* Form Elements */
        .form-group { margin-bottom: 15px; }
        label { display: block; font-size: 14px; margin-bottom: 5px; color: var(--text-secondary); }
        input { width: 100%; background: rgba(255,255,255,0.05); border: 1px solid #444; color: white; padding: 12px; border-radius: 6px; outline: none; transition: 0.3s; }
        input:focus { border-color: var(--accent-color); }
        
        /* Buttons */
        .btn { width: 100%; padding: 12px; border: none; border-radius: 6px; font-weight: 600; cursor: pointer; transition: 0.3s; margin-top: 10px; }
        .btn-primary { background: linear-gradient(135deg, var(--accent-color), #008ba3); color: white; }
        .btn-primary:hover { transform: translateY(-2px); box-shadow: 0 4px 12px rgba(0, 188, 212, 0.3); }
        .btn-danger { background: var(--danger-color); color: white; width: auto; padding: 8px 16px; font-size: 14px; }
        
        /* Icons */
        svg { fill: none; stroke: currentColor; stroke-width: 2; stroke-linecap: round; stroke-linejoin: round; }
        
        /* Responsive */
        @media (max-width: 768px) {
            .dashboard-grid { grid-template-columns: 1fr; }
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <div class="brand">
                <svg viewBox="0 0 24 24" width="24" height="24"><path d="M23 19a2 2 0 0 1-2 2H3a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h4l2-3h6l2 3h4a2 2 0 0 1 2 2z"/><circle cx="12" cy="13" r="4"/></svg>
                Datum Dash
            </div>
            <div class="status-badge">
                <div class="dot" id="status-dot"></div>
                <span id="status-text">Disconnected</span>
            </div>
        </header>
        
        <div class="dashboard-grid">
            <!-- Camera Card -->
            <div class="card">
                <h2>
                    <svg viewBox="0 0 24 24"><path d="M15 10l5-5-5 5z"/><path d="M4 8V4a2 2 0 0 1 2-2h12a2 2 0 0 1 2 2v16a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2v-4"/></svg>
                    Live Preview
                </h2>
                <div class="preview-container">
                    <img id="camera-stream" src="/capture" onload="this.style.opacity=1" onerror="this.style.opacity=0.5">
                    <button class="refresh-btn" onclick="refreshImage()" >
    <svg viewBox = "0 0 24 24" width = "20" height = "20">
        <path d = "M23 4v6h-6" /><path d = "M1 20v-6h6" />
        <path d = "M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 "
                  "20.49 15" /></ svg></ button></ div></ div>

        <!--Stats Card--><div class = "card"><h2><svg viewBox = "0 0 24 24">
        <rect x = "2" y = "3" width = "20" height = "14" rx = "2" ry = "2" />
        <line x1 = "8" y1 = "21" x2 = "16" y2 = "21" />
        <line x1 = "12" y1 = "17" x2 = "12" y2 = "21" />
        </ svg> Device Info</ h2><div class = "stats-grid">
        <div class = "stat-item"><div class = "stat-value" id = "rssi"> -
        </ div><div class = "stat-label"> WiFi Signal</ div></ div>
        <div class = "stat-item"> <
    div class
    = "stat-value" id =
        "uptime" > 0s <
        / div > <div class = "stat-label"> Uptime</ div></ div>
                    <div class = "stat-item"><div class = "stat-value"> ESP32 -
                    S3</ div><div class = "stat-label"> Model</ div></ div>
                    <div class = "stat-item">
                    <div class = "stat-value" id = "firmware"> v2.0 <
        / div > <div class = "stat-label"> Firmware</ div></ div></ div>
        <div style = "margin-top: 20px;">
        <div class = "stat-label" style = "margin-bottom: 5px;">
            Device UID</ div>
        <div class = "stat-value" style =
             "font-size: 14px; font-family: monospace;" id = "uid">...</ div>
        </ div></ div>

        <!--Configuration Card-->
        <div class = "card" style = "grid-column: 1 / -1; max-width: 800px; "
                                    "margin: 0 auto; width: 100%;"><h2>
        <svg viewBox = "0 0 24 24"><circle cx = "12" cy = "12" r = "3" />
        <path d =
             "M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 "
             "0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 "
             "1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 "
             "19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 "
             "0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 "
             "0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 "
             "9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 "
             "2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 "
             "1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 "
             "1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 "
             "2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 "
             "1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z" />
        </ svg> Network Configuration</ h2>
        <form action = "/configure" method = "POST"><div class = "form-group">
        <label> Datum Server URL</ label>
        <input type = "url" name = "server_url" placeholder =
             "https://datum.example.com" required></ div>
        <div class = "form-group"><label> WiFi Network(SSID)</ label>
        <input type = "text" name = "wifi_ssid" placeholder =
             "Enter WiFi Name" required></ div><div class = "form-group">
        <label> WiFi Password</ label>
        <input type = "password" name = "wifi_pass" placeholder =
             "Enter WiFi Password"></ div>
        <button type = "submit" class = "btn btn-primary"> Save
            &Restart</ button></ form></ div>

        <!--Actions Card--><div class = "card"><h2><svg viewBox = "0 0 24 24">
        <path d = "M13 2L3 14h9l-1 8 10-12h-9l1-8z" /></ svg> Actions</ h2>
        <div style = "display: flex; gap: 10px;">
        <button onclick = "flashLED()" class = "btn" style =
             "background: #333; color: white;"> Test LED</ button>
        <button onclick = "restartDevice()" class = "btn btn-danger"> Restart<
            / button></ div></ div></ div></ div>

        <script>
        // Update dashboard data
        function updateStats() {
  fetch('/info')
      .then(response = > response.json())
      .then(data = >
                   {
                     document.getElementById('uid').textContent =
                         data.device_uid;
                     document.getElementById('firmware').textContent =
                         data.firmware_version;

                     if (data.status == = 'configured') {
                       document.getElementById('status-dot').className =
                           'dot online';
                       document.getElementById('status-text').textContent =
                           'Configured';
                     } else {
                       document.getElementById('status-dot').className = 'dot';
                       document.getElementById('status-text').textContent =
                           'Setup Mode';
                     }

                     // RSSI (Simulated for setup mode since AP typically
                     // doesn't show station RSSI easily here) In a real
                     // scenario we'd fetch actual RSSI if connected as STA
                     document.getElementById('rssi').textContent = 'AP Mode';
                   })
      .catch(err = > console.log('Stats error:', err));
}

// Auto Refresh
function refreshImage() {
  const img = document.getElementById('camera-stream');
  img.src = '/capture?t=' + new Date().getTime();
}

function flashLED() { fetch('/action?type=led').catch(e = > console.log(e)); }

function restartDevice() {
  if (confirm('Are you sure you want to restart?')) {
    fetch('/action?type=restart');
  }
}

// Init
setInterval(refreshImage, 2000); // 2s refresh for preview
setInterval(updateStats, 5000);  // 5s refresh for stats
updateStats();

// Simple uptime counter
let seconds = 0;
setInterval(() = >
                 {
                   seconds++;
                   const m = Math.floor(seconds / 60);
                   const s = seconds % 60;
                   document.getElementById('uptime').textContent = `${m} m ${
                       s} s`;
                 },
            1000);
    </script>
</body>
</html>
)";

void handleSetupRoot() {
      setupServer.send(200, "text/html", DASHBOARD_HTML);
    }

    void handleAction() {
      String type = setupServer.arg("type");
      if (type == "led") {
#ifdef LED_GPIO_NUM
        pinMode(LED_GPIO_NUM, OUTPUT);
        digitalWrite(LED_GPIO_NUM, HIGH);
        delay(200);
        digitalWrite(LED_GPIO_NUM, LOW);
#endif
        setupServer.send(200, "text/plain", "OK");
      } else if (type == "restart") {
        setupServer.send(200, "text/plain", "Restarting...");
        delay(100);
        ESP.restart();
      } else {
        setupServer.send(400, "text/plain", "Unknown action");
      }
    }

    void handleCapture() {
      camera_fb_t *fb = esp_camera_fb_get();
      if (!fb) {
        setupServer.send(500, "text/plain", "Camera capture failed");
        return;
      }
      setupServer.sendHeader("Content-Disposition",
                             "inline; filename=capture.jpg");
      setupServer.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
      esp_camera_fb_return(fb);
    }

    void handleDeviceInfo() {
      String json = "{";
      json += "\"device_uid\":\"" + deviceUID + "\",";
      json += "\"mac_address\":\"" + deviceMAC + "\",";
      json += "\"firmware_version\":\"" + String(FIRMWARE_VERSION) + "\",";
      json += "\"model\":\"" + String(DEVICE_MODEL) + "\",";
      json += "\"has_camera\":true,";
      json += "\"status\":\"" +
              String(isActivated() ? "configured" : "unconfigured") + "\"";
      json += "}";

      setupServer.sendHeader("Access-Control-Allow-Origin", "*");
      setupServer.send(200, "application/json", json);
    }

    void handleConfigure() {
      String newServerURL = setupServer.arg("server_url");
      String newWifiSSID = setupServer.arg("wifi_ssid");
      String newWifiPass = setupServer.arg("wifi_pass");

      if (newServerURL.length() == 0 || newWifiSSID.length() == 0) {
        setupServer.send(400, "text/plain", "Missing required fields");
        return;
      }

      saveCredentials(newServerURL, newWifiSSID, newWifiPass);

      // Success Page with matching theme
      String html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { background: #1b1b1b; color: white; font-family: sans-serif; display: flex; align-items: center; justify-content: center; height: 100vh; margin: 0; }
        .card { background: #2d2d2d; padding: 40px; border-radius: 12px; text-align: center; box-shadow: 0 4px 6px rgba(0,0,0,0.2); max-width: 400px; }
        .icon { font-size: 60px; margin-bottom: 20px; }
        p { color: #a0a0a0; }
        .spinner { width: 40px; height: 40px; border: 4px solid rgba(255,255,255,0.1); border-top-color: #4caf50; border-radius: 50%; animation: spin 1s linear infinite; margin: 20px auto; }
        @keyframes spin { to { transform: rotate(360deg); } }
    </style>
</head>
<body>
    <div class="card">
        <div class="icon">✅</div>
        <h2>Configuration Saved!</h2>
        <p>Connecting to WiFi: <strong>)" +
                    newWifiSSID + R"(</strong></p>
        <div class="spinner"></div>
        <p>The device will restart automatically.</p>
    </div>
</body>
</html>
)";
      setupServer.send(200, "text/html", html);
      delay(2000);
      ESP.restart();
    }

    void handleNotFound() {
      setupServer.sendHeader("Location", "/");
      setupServer.send(302, "text/plain", "Redirecting to setup...");
    }

    // ============================================================================
    // Setup Mode
    // ============================================================================
    void startSetupMode() {
      Serial.println("\n=== ENTERING SETUP MODE ===");
      currentState = STATE_SETUP_MODE;
      setupStartTime = millis();

      String apName = String(SETUP_AP_PREFIX) + deviceUID.substring(8);

      WiFi.mode(WIFI_AP);
      WiFi.softAP(apName.c_str(), SETUP_AP_PASSWORD);

      IPAddress apIP = WiFi.softAPIP();
      Serial.printf("AP Started: %s\n", apName.c_str());
      Serial.printf("AP IP: %s\n", apIP.toString().c_str());
      Serial.println("Connect to WiFi and open http://" + apIP.toString());

      setupServer.on("/", HTTP_GET, handleSetupRoot);
      setupServer.on("/capture", HTTP_GET, handleCapture);
      setupServer.on("/info", HTTP_GET, handleDeviceInfo);
      setupServer.on("/action", HTTP_GET, handleAction); // New action handler
      setupServer.on("/configure", HTTP_POST, handleConfigure);
      setupServer.onNotFound(handleNotFound);

      setupServer.begin();
      Serial.println("HTTP server started");
    }

    // ============================================================================
    // WiFi Connection
    // ============================================================================
    bool connectToWiFi() {
      Serial.printf("Connecting to WiFi: %s\n", wifiSSID.c_str());
      currentState = STATE_CONNECTING;

      WiFi.mode(WIFI_STA);
      WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());

      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        updateLED();
        attempts++;
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nWiFi connected! IP: %s\n",
                      WiFi.localIP().toString().c_str());
        return true;
      }
      Serial.println("\nWiFi connection failed!");
      return false;
    }

    // ============================================================================
    // Server Activation
    // ============================================================================
    bool activateWithServer() {
      Serial.println("Activating with server...");
      currentState = STATE_ACTIVATING;

      HTTPClient http;
      String activateURL = serverURL + "/provisioning/activate";

      http.begin(activateURL);
      http.addHeader("Content-Type", "application/json");

      String payload = "{\"device_uid\":\"" + deviceUID +
                       "\",\"firmware_version\":\"" + FIRMWARE_VERSION +
                       "\",\"model\":\"" + DEVICE_MODEL + "\"}";

      int httpCode = http.POST(payload);

      if (httpCode == 200) {
        String response = http.getString();
        Serial.println("Activation response: " + response);

        // Simple JSON parsing
        int devIdStart = response.indexOf("\"device_id\":\"") + 13;
        int devIdEnd = response.indexOf("\"", devIdStart);
        int apiKeyStart = response.indexOf("\"api_key\":\"") + 11;
        int apiKeyEnd = response.indexOf("\"", apiKeyStart);

        if (devIdStart > 13 && apiKeyStart > 11) {
          String newDeviceID = response.substring(devIdStart, devIdEnd);
          String newAPIKey = response.substring(apiKeyStart, apiKeyEnd);
          saveActivationCredentials(newDeviceID, newAPIKey);
          Serial.println("Activation successful!");
          http.end();
          return true;
        }
      }

      Serial.printf("Activation failed: HTTP %d\n", httpCode);
      http.end();
      return false;
    }

    // ============================================================================
    // Command Polling
    // ============================================================================
    void checkCommands() {
      if (millis() - lastCommandCheck < 5000)
        return;
      lastCommandCheck = millis();

      if (WiFi.status() != WL_CONNECTED || apiKey.length() == 0)
        return;

      String url = serverURL + "/device/" + deviceID + "/commands";

      httpCommand.begin(url);
      httpCommand.addHeader("Authorization", "Bearer " + apiKey);
      httpCommand.setTimeout(5000);

      int httpCode = httpCommand.GET();

      if (httpCode == 200) {
        String payload = httpCommand.getString();

        if (payload.indexOf("\"action\":\"start-stream\"") > 0) {
          Serial.println(">>> Starting stream");
          streaming = true;
          frameCount = 0;
        } else if (payload.indexOf("\"action\":\"stop-stream\"") > 0) {
          Serial.println(">>> Stopping stream");
          streaming = false;
        } else if (payload.indexOf("\"action\":\"capture-frame\"") > 0) {
          Serial.println(">>> Capturing frame");
          sendFrame();
        } else if (payload.indexOf("\"action\":\"restart\"") > 0) {
          Serial.println(">>> Restarting");
          delay(1000);
          ESP.restart();
        } else if (payload.indexOf("\"action\":\"factory-reset\"") > 0) {
          Serial.println(">>> Factory reset");
          factoryReset();
        }
      }

      httpCommand.end();
    }

    // ============================================================================
    // Frame Streaming
    // ============================================================================
    bool sendFrame() {
      if (WiFi.status() != WL_CONNECTED)
        return false;

      camera_fb_t *fb = esp_camera_fb_get();
      if (!fb)
        return false;

      if (fb->len < 100 || fb->buf[0] != 0xFF || fb->buf[1] != 0xD8) {
        esp_camera_fb_return(fb);
        return false;
      }

      String url = serverURL + "/device/" + deviceID + "/stream/frame";

      httpStream.begin(url);
      httpStream.addHeader("Authorization", "Bearer " + apiKey);
      httpStream.addHeader("Content-Type", "image/jpeg");
      httpStream.setTimeout(5000);

      int httpCode = httpStream.POST(fb->buf, fb->len);
      bool success = (httpCode >= 200 && httpCode < 300);

      if (success) {
        frameCount++;
        if (frameCount % 100 == 0) {
          Serial.printf("Frames: %d, Size: %d bytes\n", frameCount, fb->len);
        }
      }

      httpStream.end();
      esp_camera_fb_return(fb);
      return success;
    }

    void streamLoop() {
      if (!streaming)
        return;

      unsigned long frameInterval = 1000 / STREAM_FPS;
      if (millis() - lastFrameTime >= frameInterval) {
        lastFrameTime = millis();
        sendFrame();
      }
    }

    // ============================================================================
    // Setup
    // ============================================================================
    void setup() {
      Serial.begin(115200);
      delay(1000);

      Serial.println("\n");
      Serial.println("========================================");
      Serial.println("   Datum Camera - WiFi Provisioning");
      Serial.printf("   Firmware: %s\n", FIRMWARE_VERSION);
      Serial.printf("   Model: %s\n", DEVICE_MODEL);
      Serial.println("========================================\n");

#ifdef LED_GPIO_NUM
      pinMode(LED_GPIO_NUM, OUTPUT);
      digitalWrite(LED_GPIO_NUM, LOW);
#endif

      initDeviceUID();

      if (!initCamera()) {
        Serial.println("FATAL: Camera init failed!");
        while (1)
          delay(1000);
      }

      loadCredentials();

      if (!hasCredentials()) {
        Serial.println("No credentials, entering setup mode...");
        startSetupMode();
      } else {
        if (connectToWiFi()) {
          if (!isActivated()) {
            if (activateWithServer()) {
              currentState = STATE_ONLINE;
            } else {
              currentState = STATE_OFFLINE;
            }
          } else {
            currentState = STATE_ONLINE;
            Serial.println("Camera is online and ready!");
          }
        } else {
          Serial.println("WiFi failed, entering setup mode...");
          startSetupMode();
        }
      }
    }

    // ============================================================================
    // Main Loop
    // ============================================================================
    void loop() {
      updateLED();

      switch (currentState) {
      case STATE_SETUP_MODE:
        setupServer.handleClient();
        if (millis() - setupStartTime > SETUP_TIMEOUT_MS) {
          Serial.println("Setup timeout, restarting...");
          ESP.restart();
        }
        break;

      case STATE_ONLINE:
        checkCommands();
        streamLoop();
        break;

      case STATE_OFFLINE:
        if (millis() - lastCommandCheck >= 30000) {
          if (connectToWiFi() && (isActivated() || activateWithServer())) {
            currentState = STATE_ONLINE;
          }
          lastCommandCheck = millis();
        }
        break;

      default:
        break;
      }

      delay(1);
    }
