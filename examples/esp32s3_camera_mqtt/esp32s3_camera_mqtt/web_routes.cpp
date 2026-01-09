#include "web_routes.h"
#include "camera_manager.h"
#include "camera_pins.h"
#include "mqtt_manager.h" // For extractJsonVal
#include "sd_storage.h"
#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Globals from main.ino
extern String deviceUID;
extern String deviceMAC;
extern String deviceName;
extern String deviceID;
extern bool torchState;
extern void neopixelWrite(uint8_t pin, uint8_t red, uint8_t green,
                          uint8_t blue);
extern void saveCredentials(String u, String s, String p, String token,
                            String name);
extern bool isActivated();

// Constants
static const char *WWW_USERNAME = "admin";
static String WWW_PASSWORD = "admin"; // Default password

// Dashboard HTML (Kept as is)
const char DASHBOARD_HTML[] PROGMEM =
    R"rawliteral(<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>Datum Camera</title><style>body{background:#1b1b1b;color:white;font-family:sans-serif;margin:0;padding:20px}.card{background:#2d2d2d;padding:20px;margin-bottom:20px;border-radius:8px}.btn{background:#00bcd4;color:white;border:none;padding:10px;width:100%;border-radius:4px;font-size:16px;cursor:pointer;margin-top:5px}.btn-dan{background:#f44336}.btn-gry{background:#555}input{width:100%;padding:10px;margin:5px 0 15px;box-sizing:border-box;background:#444;border:none;color:white;border-radius:4px}img{width:100%;max-width:640px;display:block;margin:0 auto;background:black;border-radius:4px;min-height:240px;object-fit:contain}.info{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:10px}.info div{background:#333;padding:10px;border-radius:4px;text-align:center}label{display:block;margin-bottom:5px;color:#aaa;font-size:14px}</style></head><body><h1>📷 Datum Camera</h1><div class="card"><img id="stream" src=""><button id="btnStream" class="btn btn-gry" onclick="toggleStream()">Start Preview</button><div class="info"><div>Signal<br><b id="rssi">-</b></div><div>Uptime<br><b id="uptime">0s</b></div></div><div class="info" style="margin-top:0"><div>SD Card<br><b id="sd_status">Checking...</b></div><div>IP Address<br><b id="ip_addr">...</b></div></div></div><div class="card"><h2>📡 Configuration</h2><form action="/configure" method="POST"><label>Server URL</label><input type="url" name="server_url" placeholder="https://..." required><label>WiFi SSID</label><input type="text" name="wifi_ssid" required><label>WiFi Password</label><input type="password" name="wifi_pass"><hr style="border-color:#444;margin:20px 0"><label>User Email</label><input type="email" name="user_email" required><label>User Password</label><input type="password" name="user_password" required><button type="submit" class="btn">Save & Restart</button></form></div><div class="card"><h2>⚡ Controls</h2><div style="display:flex;gap:10px"><button class="btn" onclick="fetch('/action?type=led')">Toggle LED</button><button class="btn btn-dan" onclick="if(confirm('Reboot?')) fetch('/action?type=restart')">Restart</button><a href="/sd" class="btn btn-gry" style="text-decoration:none;text-align:center;line-height:40px">SD Gallery</a></div></div><script>
    const i=document.getElementById('stream');
    const btn=document.getElementById('btnStream');
    let active=false;
    
    // Initial static snapshot
    i.src = '/capture?t=' + Date.now();

    const loadNext = () => {
      if (!active) return;
      // 200ms delay for ~5 FPS max
      setTimeout(() => {
        if (!active) return;
        i.src = '/capture?t=' + Date.now();
      }, 200);
    };

    i.onload = loadNext;
    i.onerror = loadNext;

    function toggleStream() {
      active = !active;
      if (active) {
        btn.innerText = "Stop Preview";
        btn.className = "btn btn-dan";
        loadNext(); 
        // Trigger first load manually if it was static
        i.src = '/capture?t=' + Date.now();
      } else {
        btn.innerText = "Start Preview";
        btn.className = "btn btn-gry";
      }
    }

    // Auto-pause when tab is hidden to save resources
    document.addEventListener("visibilitychange", () => {
      if (document.hidden && active) {
        toggleStream(); 
        console.log("Stream paused (tab hidden)");
      }
    });

    document.querySelector('form').onsubmit=(e)=>{e.preventDefault();const b=document.querySelector('button[type=submit]');b.disabled=true;b.innerText='Saving...';fetch('/configure',{method:'POST',body:new FormData(e.target)}).then(()=>{document.body.innerHTML='<div style="text-align:center;margin-top:50px"><h1>🔄 Restarting...</h1><p>Please connect to your WiFi network.</p></div>'}).catch(e=>alert('Error: '+e))};function update(){fetch('/info').then(r=>r.json()).then(d=>{document.getElementById('rssi').innerText='Active';if(d.sd_status)document.getElementById('sd_status').innerText=d.sd_status;if(d.ip)document.getElementById('ip_addr').innerText=d.ip}).catch(e=>console.log(e))}let s=0;setInterval(()=>{document.getElementById('uptime').innerText=Math.floor(++s/60)+'m '+(s%60)+'s'},1000);setInterval(update,5000);update()</script></body></html>)rawliteral";

// ============================================================================
// Endpoint Implementations
// ============================================================================

void handleOnboardingRoot(WebServer &server) {
  server.send(200, "text/html", DASHBOARD_HTML);
}

extern void processMqttLoop();  // Ensure it's available
extern void checkCommands();    // Defined in main
extern volatile bool streaming; // Defined in camera_manager.h
extern void copySharedFrame(uint8_t **destBuf, size_t *destLen,
                            size_t *destCapacity); // New API

void handleStream(WebServer &server) {
  Serial.println("[STREAM] >>> handleStream ENTERED <<<");
  WiFiClient client = server.client();

  Serial.printf("[STREAM] Client connected: %d, available: %d\n",
                client.connected(), client.available());

  String response = "HTTP/1.1 200 OK\r\nContent-Type: "
                    "multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  streaming = true;

  // Persistent buffer for this client session to avoid malloc thrashing
  uint8_t *frameBuf = NULL;
  size_t frameLen = 0;
  size_t frameCapacity = 0;
  unsigned long loopCount = 0;
  unsigned long lastFrameTime = millis();

  Serial.printf("[STREAM] Entering loop. Client still connected: %d\n",
                client.connected());

  while (client.connected()) {
    loopCount++;
    // Keep Network & MQTT Alive
    processMqttLoop();
    checkCommands(); // Process incoming commands

    // Use Zero-Copy Logic (Reuses frameBuf if capacity fits)
    copySharedFrame(&frameBuf, &frameLen, &frameCapacity);

    if (frameLen > 0 && frameBuf != NULL) {
      String head = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
                    String(frameLen) + "\r\n\r\n";
      server.sendContent(head);
      client.write(frameBuf, frameLen);
      server.sendContent("\r\n");
      lastFrameTime = millis();
    } else {
      // Check for stall
      if (millis() - lastFrameTime > 5000) {
        Serial.println("[STREAM] ERROR: No frames for 5 seconds!");
      }
      delay(5); // Wait for frame
    }

    if (loopCount % 100 == 0) {
      Serial.printf(
          "[STREAM] Loop: %lu, FrameLen: %d, Cap: %d, Connected: %d\n",
          loopCount, frameLen, frameCapacity, client.connected());
    }

    // Throttle to 20 FPS max (50ms interval) to allow other tasks to run
    delay(20);
  }

  Serial.printf(
      "[STREAM] Loop exited after %lu iterations. Client connected: %d\n",
      loopCount, client.connected());

  if (frameBuf)
    free(frameBuf); // Cleanup
  streaming = false;
}

void handleCapture(WebServer &server) {
  // 1. Acquire Lock
  if (cameraMutex != NULL) {
    if (!xSemaphoreTake(cameraMutex, 200 / portTICK_PERIOD_MS)) {
      server.send(503, "text/plain", "Camera Busy");
      return;
    }
  }

  // 2. Get Frame
  camera_fb_t *fb = esp_camera_fb_get();

  // 3. Release Lock ASAP
  if (cameraMutex != NULL)
    xSemaphoreGive(cameraMutex);

  if (!fb) {
    server.send(500, "text/plain", "Frame Capture Failed");
    return;
  }

  // 4. Send Frame
  server.client().print("HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: "
                        "*\r\nContent-Type: image/jpeg\r\nContent-Length: " +
                        String(fb->len) + "\r\n\r\n");
  server.client().write((const char *)fb->buf, fb->len);

  // 5. Return FB
  esp_camera_fb_return(fb);
}

void handleScan(WebServer &server) {
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
  server.send(200, "application/json", json);
}

void handleConfigure(WebServer &server) {
  String u = server.arg("server_url");
  String s = server.arg("wifi_ssid");
  String p = server.arg("wifi_pass");
  String token = server.arg("user_token");
  String name = server.arg("device_name");

  if (u.length() == 0 || s.length() == 0 || token.length() == 0) {
    server.send(400, "text/plain", "Missing fields");
    return;
  }

  saveCredentials(u, s, p, token, name);

  String html =
      R"(<html><body style='background:#1b1b1b;color:white;display:flex;justify-content:center;align-items:center;height:100vh;font-family:sans-serif'>
        <div style='background:#2d2d2d;padding:40px;border-radius:12px;text-align:center'>
        <h1>✅ Saved!</h1><p>Restarting...</p></div></body></html>)";
  server.send(200, "text/html", html);
  delay(2000);
  ESP.restart();
}

void handleProvision(WebServer &server) {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  String body = server.arg("plain");
  String u = extractJsonVal(body, "server_url");
  String s = extractJsonVal(body, "wifi_ssid");
  String p = extractJsonVal(body, "wifi_pass");

  if (u.length() == 0 || s.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"Missing fields\"}");
    return;
  }

  saveCredentials(u, s, p, "", "");

  server.send(200, "application/json",
              "{\"status\":\"success\",\"message\":\"Credentials saved. "
              "Restarting...\"}");
  delay(500);
  ESP.restart();
}

void handleDeviceInfo(WebServer &server) {
  String json = "{";
  json += "\"device_uid\":\"" + deviceUID + "\",";
  json += "\"mac_address\":\"" + deviceMAC + "\",";
  json +=
      "\"firmware_version\":\"2.1.0\","; // Fixed version for now or extern it
  json += "\"device_type\":\"camera\",";
  json += "\"status\":\"" +
          String(isActivated() ? "configured" : "unconfigured") + "\",";
  json += "\"sd_status\":\"" + getSDStatus() + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleThingDescription(WebServer &server) {
  String json = "{";
  json += "\"@context\": \"https://www.w3.org/2019/wot/td/v1\",";
  json += "\"id\": \"urn:dev:ops:" + deviceUID + "\",";
  json += "\"title\": \"Datum Camera\",";
  json += "\"device_type\": \"camera\",";
  json +=
      "\"securityDefinitions\": {\"bearer_sec\": {\"scheme\": \"bearer\"}},";
  json += "\"security\": \"bearer_sec\",";
  json += "\"properties\": {";
  json += "  \"status\": {\"type\": \"string\", \"description\": \"Device "
          "Status\"},";
  json += "  \"rssi\": {\"type\": \"integer\", \"description\": \"WiFi Signal "
          "Strength\"}";
  json += "},";
  json += "\"actions\": {";
  json += "  \"snapshot\": {\"description\": \"Take a photo\"},";
  json += "  \"stream\": {\"description\": \"Start video stream\"},";
  json += "  \"toggle_led\": {\"description\": \"Toggle Flashlight\"},";
  json += "  \"update_firmware\": {\"description\": \"OTA Update\"}";
  json += "}";
  json += "}";
  server.send(200, "application/td+json", json);
}

void handleAction(WebServer &server) {
  String type = server.arg("type");
  if (type == "led") {
#ifdef LED_GPIO_NUM
#if LED_GPIO_NUM == 48
    torchState = !torchState;
    // Freenove define colors, extern from main or use basic
    neopixelWrite(LED_GPIO_NUM, torchState ? 255 : 0, torchState ? 255 : 0,
                  torchState ? 255 : 0);
#else
    digitalWrite(LED_GPIO_NUM, HIGH);
    delay(200);
    digitalWrite(LED_GPIO_NUM, LOW);
#endif
#endif
    server.send(200, "text/plain", "OK");
  } else if (type == "restart") {
    server.send(200, "text/plain", "Restarting...");
    delay(100);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Unknown");
  }
}

// SD Card Handlers
void handleSDList(WebServer &server) {
  if (!server.authenticate(WWW_USERNAME, WWW_PASSWORD.c_str())) {
    return server.requestAuthentication();
  }
  String json = listSDFiles("/capture");
  server.send(200, "application/json", json);
}

void handleSDDownload(WebServer &server) {
  if (!server.authenticate(WWW_USERNAME, WWW_PASSWORD.c_str())) {
    return server.requestAuthentication();
  }
  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing 'file' parameter");
    return;
  }
  String filePath = "/capture/" + server.arg("file");
  if (SD_MMC.exists(filePath)) {
    File file = SD_MMC.open(filePath, "r");
    server.streamFile(file, "image/jpeg");
    file.close();
  } else {
    server.send(404, "text/plain", "File not found");
  }
}

void handleSDDelete(WebServer &server) {
  if (!server.authenticate(WWW_USERNAME, WWW_PASSWORD.c_str())) {
    return server.requestAuthentication();
  }
  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing 'file' parameter");
    return;
  }
  String filePath = "/capture/" + server.arg("file");
  if (deleteSDFile(filePath)) {
    server.send(200, "text/plain", "File deleted");
  } else {
    server.send(500, "text/plain", "Delete failed");
  }
}

void handleSDGallery(WebServer &server) {
  if (!server.authenticate(WWW_USERNAME, WWW_PASSWORD.c_str())) {
    return server.requestAuthentication();
  }
  String html = "<!DOCTYPE html><html><head><title>SD Gallery</title>";
  html +=
      "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:sans-serif;background:#222;color:#fff} "
          "img{max-width:100%;height:auto;border:1px solid #555;margin:5px} "
          ".grid{display:grid;grid-template-columns:repeat(auto-fill,minmax("
          "150px,1fr));gap:10px} a{color:#4da3ff}</style>";
  html += "<script>";
  html += "let allFiles = []; let visibleCount = 0; const CHUNK = 12; ";
  html += "function loadFiles() { "
          "fetch('/sd/list').then(r=>r.json()).then(files => {";
  html += "  // Sort descend (newest first)\n";
  html += "  allFiles = files.sort((a,b) => b.name.localeCompare(a.name));";
  html += "  renderChunk();";
  html += "}); }";
  html += "function renderChunk() {";
  html += "  if(visibleCount >= allFiles.length) return;";
  html += "  const slice = allFiles.slice(visibleCount, visibleCount + CHUNK);";
  html += "  const grid = document.getElementById('grid');";
  html += "  const html = slice.map(f => `<div "
          "style='text-align:center;position:relative'><a "
          "href='/sd/download?file=${f.name}' "
          "target='_blank'><img src='/sd/download?file=${f.name}' "
          "loading='lazy'></a><br><small>${f.name}</small><br>"
          "<button onclick=\"deleteFile('${f.name}')\" "
          "style='background:#f44336;color:white;border:none;padding:5px;"
          "cursor:pointer;margin-top:5px;border-radius:4px'>DELETE</button></"
          "div>`).join('');";
  html += "  // Append HTML\n";
  html += "  const temp = document.createElement('div'); temp.innerHTML = "
          "html; while(temp.firstChild) grid.appendChild(temp.firstChild);";
  html += "  visibleCount += slice.length;";
  html += "  updateBtn();";
  html += "}";
  html += "function updateBtn() {";
  html += "  const btn = document.getElementById('btnLoad');";
  html += "  if(btn) btn.style.display = (visibleCount >= allFiles.length) ? "
          "'none' : 'inline-block';";
  html += "}";
  html += "function deleteFile(f) { if(confirm('Delete ' + f + '?')) { "
          "fetch('/sd/delete?file=' + f, {method:'POST'}).then(r => { "
          "if(r.ok) { location.reload(); } else alert('Failed'); }); } }";
  html += "window.onload=loadFiles;";
  html += "</script></head><body>";
  html +=
      "<h2>SD Card Gallery</h2><p><a href='/'>&larr; Back to Stream</a></p>";
  html += "<div id='grid' class='grid'></div>";
  html += "<div style='text-align:center;margin:20px'><button id='btnLoad' "
          "class='btn btn-gry' style='width:auto;padding:10px 20px' "
          "onclick='renderChunk()'>Load More</button></div>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleInfo(WebServer &server) {
  String json = "{";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"sd_status\":\"" +
          String(SD_MMC.cardSize()
                     ? (String(SD_MMC.usedBytes() / 1024 / 1024) + "MB Used")
                     : "No Card") +
          "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// ============================================================================
// Main Setup Function
// ============================================================================
void setupWebRoutes(WebServer &server, const String &apiKey) {
  // Routes
  server.on("/", HTTP_GET, [&server]() { handleOnboardingRoot(server); });
  server.on("/stream", HTTP_GET, [&server]() { handleStream(server); });
  server.on("/capture", HTTP_GET, [&server]() { handleCapture(server); });
  server.on("/scan", HTTP_GET, [&server]() { handleScan(server); });
  server.on("/configure", HTTP_POST, [&server]() { handleConfigure(server); });
  server.on("/provision", HTTP_POST,
            [&server]() { handleProvision(server); }); // Added /provision
  server.on("/info", HTTP_GET,
            [&server]() { handleDeviceInfo(server); }); // Unified Info?
  // Wait, handleInfo vs handleDeviceInfo. handleDeviceInfo returns JSON usage
  // for app. handleInfo returns small stats for dashboard. Let's use handleInfo
  // for /info (dashboard) and handleDeviceInfo for /device-info or /info? In
  // .ino, /info pointed to handleDeviceInfo. In .cpp, /info pointed to
  // handleInfo. Let's overwrite /info to handleDeviceInfo since it's more
  // comprehensive. Actually, handleDeviceInfo calls getSDStatus, etc.

  // Re-mapping:
  server.on("/info", HTTP_GET, [&server]() { handleDeviceInfo(server); });
  server.on("/.well-known/wot-thing-description", HTTP_GET,
            [&server]() { handleThingDescription(server); });
  server.on("/action", HTTP_GET, [&server]() { handleAction(server); });

  // SD Routes
  server.on("/sd/list", HTTP_GET, [&server]() { handleSDList(server); });
  server.on("/sd/download", HTTP_GET,
            [&server]() { handleSDDownload(server); });
  server.on("/sd/delete", [&server]() { handleSDDelete(server); });
  server.on("/sd", HTTP_GET, [&server]() { handleSDGallery(server); });

  server.onNotFound([&server]() {
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Redirect");
  });
}
