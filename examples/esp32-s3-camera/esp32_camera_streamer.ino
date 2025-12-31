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

// ============================================================================
// Board Selection
// ============================================================================
// #define CAMERA_MODEL_AI_THINKER
#define CAMERA_MODEL_ESP32S3_CAM
// #define CAMERA_MODEL_FREENOVE_S3

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

// ============================================================================
// Web Interface (ESP-DASH Style)
// ============================================================================
const char *DASHBOARD_HTML = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Datum Device Dashboard</title>
    <style>
        :root { --bg-color: #1b1b1b; --card-bg: #2d2d2d; --text-primary: #ffffff; --text-secondary: #a0a0a0; --accent-color: #00bcd4; --success-color: #4caf50; --danger-color: #f44336; }
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
        body { background-color: var(--bg-color); color: var(--text-primary); padding: 20px; }
        .container { max-width: 1200px; margin: 0 auto; }
        header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 30px; border-bottom: 1px solid #333; padding-bottom: 15px; }
        .brand { font-size: 24px; font-weight: 700; color: var(--accent-color); display: flex; align-items: center; gap: 10px; }
        .status-badge { background: var(--card-bg); padding: 5px 12px; border-radius: 20px; font-size: 14px; display: flex; align-items: center; gap: 8px; }
        .dot { width: 8px; height: 8px; border-radius: 50%; background: var(--text-secondary); }
        .dot.online { background: var(--success-color); box-shadow: 0 0 10px var(--success-color); }
        .dashboard-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
        .card { background: var(--card-bg); border-radius: 12px; padding: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
        .card h2 { font-size: 18px; color: var(--text-secondary); margin-bottom: 15px; display: flex; align-items: center; gap: 10px; }
        .preview-container { text-align: center; background: #000; border-radius: 8px; overflow: hidden; position: relative; min-height: 240px; display: flex; align-items: center; justify-content: center; }
        img#camera-stream { width: 100%; height: auto; display: block; }
        .stats-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }
        .stat-item { background: rgba(255,255,255,0.05); padding: 15px; border-radius: 8px; }
        .stat-value { font-size: 20px; font-weight: bold; }
        .stat-label { font-size: 12px; color: var(--text-secondary); }
        .form-group { margin-bottom: 15px; }
        label { display: block; font-size: 14px; margin-bottom: 5px; color: var(--text-secondary); }
        input { width: 100%; background: rgba(255,255,255,0.05); border: 1px solid #444; color: white; padding: 12px; border-radius: 6px; outline: none; color: white; }
        input:focus { border-color: var(--accent-color); }
        .btn { width: 100%; padding: 12px; border: none; border-radius: 6px; font-weight: 600; cursor: pointer; margin-top: 10px; }
        .btn-primary { background: linear-gradient(135deg, var(--accent-color), #008ba3); color: white; }
        .btn-danger { background: var(--danger-color); color: white; width: auto; padding: 8px 16px; font-size: 14px; }
        svg { fill: none; stroke: currentColor; stroke-width: 2; stroke-linecap: round; stroke-linejoin: round; }
        @media (max-width: 768px) { .dashboard-grid { grid-template-columns: 1fr; } }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <div class="brand">
                <svg viewBox="0 0 24 24" width="24" height="24"><path d="M23 19a2 2 0 0 1-2 2H3a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h4l2-3h6l2 3h4a2 2 0 0 1 2 2z"/><circle cx="12" cy="13" r="4"/></svg>
                Datum Dash
            </div>
            <div class="status-badge"><div class="dot" id="status-dot"></div><span id="status-text">Disconnected</span></div>
        </header>

        <div class="dashboard-grid">
            <div class="card">
                <h2><svg viewBox="0 0 24 24"><path d="M15 10l5-5-5 5z"/><path d="M4 8V4a2 2 0 0 1 2-2h12a2 2 0 0 1 2 2v16a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2v-4"/></svg> Live Stream</h2>
                <div class="preview-container">
                    <img id="camera-stream" src="/stream" onload="this.style.opacity=1" onerror="this.style.opacity=0.5; setTimeout(reloadStream, 1000);">
                    <div style="position: absolute; bottom: 10px; left: 10px; background: rgba(0,0,0,0.7); padding: 4px 8px; border-radius: 4px; font-size: 12px;">MJPEG Local</div>
                </div>
            </div>

            <div class="card">
                <h2><svg viewBox="0 0 24 24"><rect x="2" y="3" width="20" height="14" rx="2" ry="2"/><line x1="8" y1="21" x2="16" y2="21"/><line x1="12" y1="17" x2="12" y2="21"/></svg> Device Info</h2>
                <div class="stats-grid">
                    <div class="stat-item"><div class="stat-value" id="rssi">-</div><div class="stat-label">WiFi Signal</div></div>
                    <div class="stat-item"><div class="stat-value" id="uptime">0s</div><div class="stat-label">Uptime</div></div>
                    <div class="stat-item"><div class="stat-value">ESP32-S3</div><div class="stat-label">Model</div></div>
                    <div class="stat-item"><div class="stat-value" id="firmware">v2.1</div><div class="stat-label">Firmware</div></div>
                </div>
                <div style="margin-top: 20px;"><div class="stat-label" style="margin-bottom: 5px;">Device UID</div><div class="stat-value" style="font-size: 14px; font-family: monospace;" id="uid">...</div></div>
            </div>

            <div class="card" style="grid-column: 1/-1;">
                <h2><svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg> Network Configuration</h2>
                <form action="/configure" method="POST">
                    <div class="form-group"><label>Datum Server URL</label><input type="url" name="server_url" placeholder="https://datum.example.com" required></div>
                    <div class="form-group"><label>WiFi Network (SSID)</label><input type="text" name="wifi_ssid" placeholder="Enter WiFi Name" required></div>
                    <div class="form-group"><label>WiFi Password</label><input type="password" name="wifi_pass" placeholder="Enter WiFi Password"></div>
                    <button type="submit" class="btn btn-primary">Save & Restart</button>
                </form>
            </div>

            <div class="card">
                <h2><svg viewBox="0 0 24 24"><path d="M13 2L3 14h9l-1 8 10-12h-9l1-8z"/></svg> Actions</h2>
                <div style="display: flex; gap: 10px;">
                    <button onclick="flashLED()" class
    = "btn" style =
        "background: #333; color: white;" > Test LED</ button>
        <button onclick = "restartDevice()" class = "btn btn-danger">
            Restart</ button></ div></ div></ div></ div>
        <script> function updateStats() {
  fetch('/info')
      .then(r = > r.json())
      .then(d = >
                {
                  document.getElementById('uid').textContent = d.device_uid;
                  document.getElementById('firmware').textContent =
                      d.firmware_version;
                  if (d.status == = 'configured') {
                    document.getElementById('status-dot').className =
                        'dot online';
                    document.getElementById('status-text').textContent =
                        'Configured';
                  }
                  document.getElementById('rssi').textContent = 'Active';
                })
      .catch(console.log);
}
function reloadStream() {
  document.getElementById('camera-stream').src = '/stream?t=' + Date.now();
}
function flashLED() { fetch('/action?type=led'); }
function restartDevice() {
  if (confirm('Restart?'))
    fetch('/action?type=restart');
}

setInterval(updateStats, 5000);
updateStats();
let s = 0;
setInterval(() = >
                 {
                   s++;
                   document.getElementById('uptime').textContent =`${
                       Math.floor(s / 60)} m ${s % 60} s`
                 },
            1000);
    </script>
</body>
</html>
)";

// ============================================================================
// Core Functions
// ============================================================================
void updateLED() {
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
      prefs.end();
    }

    void saveCredentials(String u, String s, String p) {
      prefs.begin("datum", false);
      prefs.putString("server_url", u);
      prefs.putString("wifi_ssid", s);
      prefs.putString("wifi_pass", p);
      prefs.end();
      serverURL = u;
      wifiSSID = s;
      wifiPass = p;
    }

    void saveActivation(String id, String key) {
      prefs.begin("datum", false);
      prefs.putString("device_id", id);
      prefs.putString("api_key", key);
      prefs.end();
      deviceID = id;
      apiKey = key;
    }

    void initDeviceUID() {
      uint8_t mac[6];
      esp_efuse_mac_get_default(mac);
      char buf[13];
      sprintf(buf, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3],
              mac[4], mac[5]);
      deviceUID = String(buf);
      char buf2[18];
      sprintf(buf2, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
              mac[3], mac[4], mac[5]);
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
    void handleSetupRoot() {
      setupServer.send(200, "text/html", DASHBOARD_HTML);
    }

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

        String head =
            "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
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
      setupServer.sendHeader("Content-Disposition",
                             "inline; filename=capture.jpg");
      setupServer.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
      esp_camera_fb_return(fb);
    }

    void handleConfigure() {
      String u = setupServer.arg("server_url");
      String s = setupServer.arg("wifi_ssid");
      String p = setupServer.arg("wifi_pass");
      if (u.length() == 0 || s.length() == 0) {
        setupServer.send(400, "text/plain", "Missing fields");
        return;
      }
      saveCredentials(u, s, p);
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
      Serial.println("Starting Setup Mode");
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
        delay(500);
        updateLED();
        attempts++;
      }
      return WiFi.status() == WL_CONNECTED;
    }

    bool activateWithServer() {
      currentState = STATE_ACTIVATING;
      HTTPClient http;
      http.begin(serverURL + "/provisioning/activate");
      http.addHeader("Content-Type", "application/json");
      String pl = "{\"device_uid\":\"" + deviceUID +
                  "\",\"firmware_version\":\"" + FIRMWARE_VERSION +
                  "\",\"model\":\"" + DEVICE_MODEL + "\"}";
      int code = http.POST(pl);
      if (code == 200) {
        String resp = http.getString();
        int ds = resp.indexOf("\"device_id\":\"") + 13;
        int de = resp.indexOf("\"", ds);
        int ks = resp.indexOf("\"api_key\":\"") + 11;
        int ke = resp.indexOf("\"", ks);
        if (ds > 13 && ks > 11) {
          saveActivation(resp.substring(ds, de), resp.substring(ks, ke));
          http.end();
          return true;
        }
      }
      http.end();
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

    void setup() {
      Serial.begin(115200);
#ifdef LED_GPIO_NUM
      pinMode(LED_GPIO_NUM, OUTPUT);
#endif
      initDeviceUID();
      initCamera();
      loadCredentials();

      if (wifiSSID.length() == 0)
        startSetupMode();
      else if (connectToWiFi()) {
        if (apiKey.length() > 0 || activateWithServer()) {
          currentState = STATE_ONLINE;
          startSetupMode(); // Keep dashboard accessible even when online
        } else
          currentState = STATE_OFFLINE;
      } else
        startSetupMode();
    }

    void loop() {
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
