#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <TJpg_Decoder.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <vector>

// -- Hardware Pin Definitions for GeekMagic TV (ESP8266) --
#define TFT_MOSI 13 // D7
#define TFT_SCLK 14 // D5
#define TFT_CS -1   // Not connected
#define TFT_DC 0    // D3
#define TFT_RST 2   // D4
#define TFT_BL 5    // D1

#define ST77XX_GREY 0x8410
#define ST77XX_DARKGREY 0x4208

// -- Globals --
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
DNSServer dnsServer;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// -- Config Structure --
#define CONFIG_MAGIC 0xDD04
#define FIRMWARE_VER "1.1.2" // Bumped version
#define DEVICE_TYPE_NAME "display"

struct Config {
  uint32_t magic;
  char ssid[32];
  char password[64];
  char user_token[1024];
  char device_id[37];
  char api_key[64];
  char server_url[128];
  char target_device_id[37];
  int poll_interval;
  int boot_failures;
  char overlay_filter[64];
  int slide_interval;
};
Config config;

// -- Dash Globals --
unsigned long lastPollTime = 0;
unsigned long lastCarouselTime = 0;
bool isTargetOnline = false;
unsigned long lastReconnectAttempt = 0;
// -- FPS Globals --
unsigned long lastFrameTime = 0;
int frameCount = 0;
float fps = 0.0;
unsigned long lastFpsTime = 0;
unsigned long lastTelemetryTime = 0;
String lastScanError = ""; // Diagnosis helper

struct Property {
  String key;
  String title;
  String unit;
};
std::vector<Property> properties;
int currentPropIndex = 0;
bool hasCameraProp = false; // Flag if target is a camera

struct ValueCache {
  String key;
  String value;
};
std::vector<ValueCache> valueCache;

// -- Forward Declarations --
void saveConfig();
void showStatus(String msg, uint16_t color);
void sendLog(String msg);
void startSetupMode();

// -- HTML --
const char PROVISION_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>DatumDash Setup</title>
<style>
body{background:#1b1b1b;color:white;font-family:sans-serif;padding:20px}
.card{background:#2d2d2d;padding:20px;border-radius:8px;max-width:400px;margin:0 auto}
input,select{width:100%;padding:10px;margin:5px 0 15px;box-sizing:border-box;background:#444;border:none;color:white;border-radius:4px}
button{background:#00bcd4;color:white;border:none;padding:12px;width:100%;border-radius:4px;font-size:16px;cursor:pointer}
h2{text-align:center;color:#00bcd4}
.note{font-size:12px;color:#aaa;margin-top:-10px;margin-bottom:10px}
</style></head><body>
<div class="card">
<h2>🚀 DatumDash Setup</h2>
<div style="text-align:right;margin-bottom:10px">
  <a href="/update" style="color:#00bcd4;text-decoration:none;font-size:14px;border:1px solid #444;padding:5px 10px;border-radius:4px">☁️ Update FW</a>
</div>
<form action="/save" method="POST">
<label>Select WiFi Network</label>
<select id="ssid" name="ssid"><option value="">Scanning...</option></select>
<label>Or Type SSID Manually</label>
<input type="text" name="custom_ssid" placeholder="Enter SSID if not listed">
<label>WiFi Password</label><input type="password" name="password">
<hr style="border-color:#444;margin:20px 0">
<label>Datum User Token (REQUIRED)</label><input type="text" name="token" placeholder="Paste Token from Web Console (Settings -> Keys)">
<div class="note"><b>Note:</b> Login via Email/Pass is removed. Please generate a token from the Web Console and paste it here.</div>
<hr style="border-color:#444;margin:20px 0">
<label>Target Device ID (to Watch)</label><input type="text" name="target_id" placeholder="ID of device to display">
<label>Poll Interval (Seconds)</label><input type="number" name="poll_int" value="5" min="1">
<label>Slide Interval (ms, 0=Fast)</label><input type="number" name="slide_int" value="3000" min="0">
<label>Server URL (Default: https://datum.bezg.in)</label>
<input type="text" name="server_url" value="https://datum.bezg.in" placeholder="http://ip:8000">
<button type="submit">Save & Connect</button>
</form>
</div>
<script>
window.onload = function() {
  fetch('/scan').then(function(r){ return r.json(); }).then(function(d){
    var s = document.getElementById('ssid'); 
    s.innerHTML='<option value="">Select Network...</option>';
    d.forEach(function(n){
      var o = document.createElement('option');
      o.value = n.ssid;
      o.innerText = n.ssid + (n.auth ? ' 🔒' : '');
      s.appendChild(o);
    });
  }).catch(function(e){ console.log(e); });
};
</script></body></html>
)rawliteral";

// -- Config Functions --

void loadConfig() {
  EEPROM.begin(2048);
  EEPROM.get(0, config);
  if (config.magic != CONFIG_MAGIC) {
    Serial.println("Invalid/Old Config. Resetting defaults.");
    memset(&config, 0, sizeof(Config));
    Serial.println("Config Cleared.");
    config.magic = CONFIG_MAGIC;
    strncpy(config.device_id, "", sizeof(config.device_id));
    // Default Overlay
    strncpy(config.overlay_filter, "temp,volt,curr,pow",
            sizeof(config.overlay_filter));
    config.slide_interval = 3000;
    config.boot_failures = 0;
    strcpy(config.server_url, "https://datum.bezg.in");
    config.poll_interval = 5;
    saveConfig();
  } else {
    Serial.print("Loaded Config. Magic: 0x");
    Serial.println(config.magic, HEX);
    Serial.print("User Token Len: ");
    Serial.println(strlen(config.user_token));
  }
}

void saveConfig() {
  config.magic = CONFIG_MAGIC;
  EEPROM.put(0, config);
  EEPROM.commit();
}

String getHostFromUrl(String url) {
  int index = url.indexOf("://");
  if (index != -1) {
    url = url.substring(index + 3);
  }
  index = url.indexOf("/");
  if (index != -1) {
    url = url.substring(0, index);
  }
  index = url.indexOf(":");
  if (index != -1) {
    url = url.substring(0, index);
  }
  return url;
}

// -- Display Functions --

void showStatus(String msg, uint16_t color) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 10);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.println("DatumDash TV");
  tft.setTextSize(1);
  tft.println(FIRMWARE_VER);
  tft.println("");

  tft.setTextColor(color);
  tft.setTextSize(2);
  tft.println(msg);
}

// TJpg_Decoder Callback
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h,
                uint16_t *bitmap) {
  if (y >= tft.height())
    return 0;
  tft.drawRGBBitmap(x, y, bitmap, w, h);
  return 1;
}

void drawCameraView() {
  if (strlen(config.target_device_id) == 0)
    return;

  // Persistent Client State
  static WiFiClientSecure *streamClient = nullptr;
  static HTTPClient
      http; // Keep HTTPClient instance to manage the connection wrapper
  static bool isConnected = false;

  // 1. Connection Management
  if (!isConnected || streamClient == nullptr || !streamClient->connected()) {
    // Cleanup if needed
    if (streamClient) {
      delete streamClient;
      streamClient = nullptr;
    }
    isConnected = false;

    // Allocate new client
    streamClient = new WiFiClientSecure();
    streamClient->setInsecure();
    streamClient->setBufferSizes(4096, 512); // Optimized buffer

    String url = String(config.server_url) + "/dev/" + config.target_device_id +
                 "/stream/mjpeg";

    // Manual HTTP Request for Stream
    // We can't use HTTPClient easily for continuous stream processing with
    // manual parsing flexibility needed here So we connect manually using the
    // Secure Client

    String host = getHostFromUrl(config.server_url);
    int port = 443;
    if (config.server_url[4] != 's')
      port = 80; // Simple http check

    Serial.print("Connecting to stream: ");
    Serial.println(url);

    if (streamClient->connect(host.c_str(), port)) {
      // Send HTTP Request
      streamClient->print("GET /dev/" + String(config.target_device_id) +
                          "/stream/mjpeg HTTP/1.1\r\n");
      streamClient->print("Host: " + host + "\r\n");
      streamClient->print("Authorization: Bearer " + String(config.api_key) +
                          "\r\n");
      streamClient->print("User-Agent: DatumDash\r\n");
      streamClient->print("Connection: keep-alive\r\n\r\n");

      isConnected = true;
    } else {
      Serial.println("Stream Connection Failed");
      delay(100); // Backoff
      return;
    }
  }

  // 2. Stream Parsing (Multipart)
  // Logic: Read line by line looking for Content-Length
  // This is a blocking read for the header, then block read for body
  // To keep loop responsive, we depend on valid stream data arriving

  if (streamClient->available()) {
    static int violence_counter = 0; // Prevent infinite loops

    String line = streamClient->readStringUntil('\n');
    line.trim();

    // Simple Parse Logic for "Content-Length: <size>"
    if (line.startsWith("Content-Length:")) {
      int len = line.substring(15).toInt();
      if (len > 0 && len < 40000) {
        // Skip empty line after headers
        streamClient->readStringUntil('\n');

        // Read Image Data
        if (ESP.getFreeHeap() > len + 2000) {
          uint8_t *valBuffer = (uint8_t *)malloc(len);
          if (valBuffer) {
            int bytesRead = 0;
            while (bytesRead < len && streamClient->connected()) {
              int c =
                  streamClient->read(valBuffer + bytesRead, len - bytesRead);
              if (c > 0)
                bytesRead += c;
              else
                delay(1);
            }

            if (bytesRead == len) {
              // Draw!
              TJpgDec.drawJpg(40, 0, valBuffer, len);

              // Clear surrounding areas logic moved to Mode Switch
            }
            free(valBuffer);
          }
        }
      }
    }
  } else {
    // Keep-Alive check?
    if (!streamClient->connected())
      isConnected = false;
  }

  // 3. Draw Data in Bottom Area (Carousel) with Smart Redraw
  // Filter first
  std::vector<ValueCache> filtered;
  for (auto &v : valueCache) {
    if (config.overlay_filter[0] == 0 ||
        strstr(config.overlay_filter, v.key.c_str()) != NULL) {
      filtered.push_back(v);
    }
  }

  if (!filtered.empty()) {
    unsigned long now = millis();
    int interval = config.slide_interval > 0 ? config.slide_interval : 3000;
    if (interval < 100)
      interval = 500;

    // Calculate index based on time
    // Calculate index based on time
    // Use LOCAL static timer, independent of global display loop
    static unsigned long lastSlideTime = 0;
    static int carouselIndex = 0;
    if (now - lastSlideTime > interval) {
      carouselIndex = (carouselIndex + 1) % filtered.size();
      lastSlideTime = now;
    }
    // Handle resize
    if (carouselIndex >= filtered.size())
      carouselIndex = 0;

    ValueCache &v = filtered[carouselIndex];

    // Smart Redraw: Only draw if changed
    static int lastDrawnIndex = -1;
    static String lastDrawnVal = "";

    if (carouselIndex != lastDrawnIndex || v.value != lastDrawnVal) {
      // Clear Bottom
      tft.fillRect(0, 120, 240, 120, ST77XX_BLACK);

      // Draw Key
      tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
      tft.setTextSize(2);
      int16_t x1, y1;
      uint16_t w, h;
      String keyStr = v.key;
      keyStr.toUpperCase();
      tft.getTextBounds(keyStr, 0, 0, &x1, &y1, &w, &h);
      tft.setCursor((240 - w) / 2, 140);
      tft.print(keyStr);

      // Draw Value
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setTextSize(3);
      tft.getTextBounds(v.value, 0, 0, &x1, &y1, &w, &h);
      if (w > 220)
        tft.setTextSize(2); // Auto-scale
      tft.getTextBounds(v.value, 0, 0, &x1, &y1, &w, &h);
      tft.setCursor((240 - w) / 2, 180);
      tft.print(v.value);

      // Pagination
      tft.setTextSize(1);
      tft.setTextColor(ST77XX_DARKGREY, ST77XX_BLACK);
      String pag = String(carouselIndex + 1) + "/" + String(filtered.size());
      tft.getTextBounds(pag, 0, 0, &x1, &y1, &w, &h);
      tft.setCursor((240 - w) / 2, 220);
      tft.print(pag);

      lastDrawnIndex = carouselIndex;
      lastDrawnVal = v.value;
    }
  } else {
    // No Data case
    static bool noDataDrawn = false;
    if (!noDataDrawn) {
      tft.fillRect(0, 120, 240, 120, ST77XX_BLACK);
      tft.setTextColor(ST77XX_GREY, ST77XX_BLACK);
      tft.setCursor(60, 170);
      tft.setTextSize(2);
      tft.print("No Data");
      noDataDrawn = true;
    }
  }

  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setCursor(5, 5);
  tft.setTextSize(1);
  tft.print("LIVE");
}
// End of drawCameraView

String getCachedValue(String key) {
  for (auto &v : valueCache) {
    if (v.key == key)
      return v.value;
  }
  return "--";
}

void updateValueCache(String key, String val) {
  for (auto &v : valueCache) {
    if (v.key == key) {
      v.value = val;
      return;
    }
  }
  valueCache.push_back({key, val});
}

void drawCard(String title, String value, String unit) {
  tft.fillScreen(ST77XX_BLACK);
  tft.fillRect(0, 0, 240, 40, ST77XX_BLUE);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);

  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, 12);
  tft.print(title);

  tft.setTextSize(4);
  tft.getTextBounds(value, 0, 0, &x1, &y1, &w, &h);
  if (w > 220)
    tft.setTextSize(3);
  tft.getTextBounds(value, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, 100);
  tft.setTextColor(ST77XX_GREEN);
  tft.print(value);

  if (unit.length() > 0) {
    tft.setTextSize(2);
    tft.setTextColor(0xAAAA);
    tft.getTextBounds(unit, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((240 - w) / 2, 150);
    tft.print(unit);
  }

  tft.drawLine(0, 210, 240, 210, ST77XX_WHITE);
  tft.setCursor(10, 220);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("Target: ");
  tft.print(config.target_device_id);

  tft.setCursor(180, 220);
  tft.setTextColor(isTargetOnline ? ST77XX_GREEN : ST77XX_RED);
  tft.print(isTargetOnline ? "ONLINE" : "OFFLINE");
}

void updateDisplay() {
  if (strlen(config.target_device_id) == 0) {
    showStatus("No Target Configured", ST77XX_YELLOW);
    return;
  }
  if (properties.empty()) {
    if (lastScanError.length() > 0) {
      showStatus(lastScanError, ST77XX_RED);
    } else {
      showStatus("Scanning...", ST77XX_BLUE);
    }
    return;
  }

  // Carousel Logic
  // User Request: Prioritize Camera Stream if available
  if (hasCameraProp && getCachedValue("stream_enabled") == "true") {
    drawCameraView();
    return; // Skip property cards completely
  }

  if (currentPropIndex >= properties.size()) {
    currentPropIndex = 0;
  }

  if (currentPropIndex < properties.size()) {
    Property &p = properties[currentPropIndex];
    drawCard(p.title, getCachedValue(p.key), p.unit);
    currentPropIndex++;
  } else {
    currentPropIndex = 0; // Fallback
  }
}

// -- MQTT & Network Functions --

// -- MQTT & Network Functions --

void sendLog(String msg) {
  if (mqttClient.connected()) {
    String topic = "dev/" + String(config.device_id) + "/logs";
    String payload =
        "{\"msg\":\"" + msg + "\", \"ts\":" + String(millis() / 1000) + "}";
    mqttClient.publish(topic.c_str(), payload.c_str());
    Serial.println("LOG: " + msg);
    mqttClient.loop();
  } else {
    Serial.println("LOG (No MQTT): " + msg);
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error)
    return;

  const char *actionStr = doc["action"];
  String action = String(actionStr ? actionStr : doc["type"] | "");

  if (action == "update_firmware") {
    String fwUrl = doc["params"]["url"] | doc["url"] | "";
    if (fwUrl.length() > 0) {
      if (fwUrl.indexOf('?') == -1)
        fwUrl += "?token=" + String(config.api_key);
      else
        fwUrl += "&token=" + String(config.api_key);

      showStatus("FW UPDATING...", ST77XX_BLUE);

      // WDT Disable explicitly for blocking update
      ESP.wdtDisable();

      Serial.printf("Pre-OTA Free Heap: %d\n", ESP.getFreeHeap());

      // Register Progress Callback
      ESPhttpUpdate.onProgress([](int cur, int total) {
        ESP.wdtFeed(); // Feed the dog during download!
        static int lastP = 0;
        int p = (cur * 100) / total;
        if (p != lastP && p % 10 == 0) { // Update every 10%
          lastP = p;
          Serial.printf("OTA Progress: %d%%\n", p);
          tft.setCursor(10, 140);
          tft.setTextColor(ST77XX_WHITE, ST77XX_BLUE);
          tft.setTextSize(2);
          tft.printf("DL: %d%%", p);
        }
      });

      t_httpUpdate_return ret;
      // Force HTTP for OTA to avoid TLS/OOM issues on ESP8266
      if (fwUrl.startsWith("https://")) {
        fwUrl.replace("https://", "http://");
        Serial.println("Downgrading OTA to HTTP for reliability");
      }

      Serial.print("Final OTA URL: ");
      Serial.println(fwUrl);

      WiFiClient client;
      client.setTimeout(20000);
      ESP.wdtFeed();
      ret = ESPhttpUpdate.update(client, fwUrl);

      // Re-enable WDT if update failed/returned
      ESP.wdtEnable(1000);

      if (ret == HTTP_UPDATE_OK) {
        ESP.restart();
      } else {
        Serial.printf("OTA Error: %d\n", ESPhttpUpdate.getLastError());
        Serial.println("Error String: " + ESPhttpUpdate.getLastErrorString());
        sendLog("OTA Failed: " + String(ESPhttpUpdate.getLastError()));
      }
    }
  } else if (action == "set_target") {
    String newTarget = doc["params"]["target_id"] | "";
    if (newTarget.length() > 0) {
      strncpy(config.target_device_id, newTarget.c_str(),
              sizeof(config.target_device_id));
      config.poll_interval =
          doc["params"]["poll_interval"] | config.poll_interval;
      config.slide_interval =
          doc["params"]["slide_interval"] | config.slide_interval;

      saveConfig();
      sendLog("Target Set: " + newTarget);
      showStatus("Target Updated!", ST77XX_GREEN);
      delay(1000);
      properties.clear();
      lastPollTime = 0;
    }
  }
}

boolean connectMQTT() {
  if (mqttClient.connected())
    return true;

  String host = getHostFromUrl(config.server_url);
  mqttClient.setServer(host.c_str(), 1883);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(4096); // Increase buffer for large payloads check
  mqttClient.setKeepAlive(60);

  if (strlen(config.api_key) == 0)
    return false;

  if (mqttClient.connect(config.device_id, config.device_id, config.api_key)) {
    String topic = "dev/" + String(config.device_id) + "/cmd";
    mqttClient.subscribe(topic.c_str());
    return true;
  }
  return false;
}

bool registerDeviceManual(String serverUrl, String userToken) {
  serverUrl.trim();
  userToken.trim();
  if (userToken.length() == 0)
    return false;

  String url = serverUrl + (serverUrl.endsWith("/") ? "dev" : "/dev");

  std::unique_ptr<WiFiClient> client;
  if (url.startsWith("https://")) {
    WiFiClientSecure *secureClient = new WiFiClientSecure();
    secureClient->setInsecure();
    client.reset(secureClient);
  } else {
    client.reset(new WiFiClient());
  }

  HTTPClient http;
  if (!http.begin(*client, url))
    return false;

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + userToken);

  String chipId = String(ESP.getChipId());
  String payload = "{\"name\":\"DatumDash\",\"type\":\"display\",\"uid\":\"" +
                   chipId + "\"}";
  int code = http.POST(payload);
  String resp = http.getString();
  http.end();

  if (code == 200 || code == 201) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, resp);
    String did = doc["device_id"] | doc["id"] | "";
    String key = doc["api_key"] | "";
    if (did.length() > 0 && key.length() > 0) {
      strncpy(config.device_id, did.c_str(), sizeof(config.device_id));
      strncpy(config.api_key, key.c_str(), sizeof(config.api_key));
      // memset(config.user_token, 0, sizeof(config.user_token)); // Keep User
      // Token for Polling!
      saveConfig();
      return true;
    }
  }
  return false;
}

void reportTelemetry() {
  if (!mqttClient.connected())
    return;
  DynamicJsonDocument doc(512);
  doc["uptime"] = millis() / 1000;
  doc["wifi_ssid"] = String(config.ssid);
  doc["ip_address"] = WiFi.localIP().toString();
  doc["target_device_id"] = String(config.target_device_id);
  doc["poll_interval"] = config.poll_interval;
  doc["fw_ver"] = FIRMWARE_VER;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["fps"] = int(fps);
  String payload;
  serializeJson(doc, payload);
  mqttClient.publish(("dev/" + String(config.device_id) + "/data").c_str(),
                     payload.c_str());
}

void sendThingDescription() {
  if (strlen(config.device_id) == 0 || strlen(config.api_key) == 0)
    return;

  WiFiClient *client = nullptr;
  if (String(config.server_url).startsWith("https")) {
    WiFiClientSecure *sc = new WiFiClientSecure();
    sc->setInsecure();
    client = sc;
  } else {
    client = new WiFiClient();
  }

  HTTPClient http;
  String srv = String(config.server_url);
  if (srv.endsWith("/"))
    srv.remove(srv.length() - 1);
  String url = srv + "/dev/" + config.device_id + "/thing-description";

  http.begin(*client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(config.api_key));

  DynamicJsonDocument doc(2048);
  doc["@context"] = "https://www.w3.org/2019/wot/td/v1";
  doc["id"] = "urn:dev:datum:" + String(config.device_id);
  doc["title"] = "DatumDash";
  doc["description"] = "Smart IoT Display";

  JsonObject props = doc.createNestedObject("properties");
  props["target_device_id"]["title"] = "Target Device";
  props["target_device_id"]["type"] = "string";
  props["poll_interval"]["title"] = "Poll Interval";
  props["poll_interval"]["type"] = "integer";
  props["wifi_ssid"]["title"] = "WiFi Network";
  props["wifi_ssid"]["type"] = "string";
  props["wifi_ssid"]["readOnly"] = true;

  JsonObject actions = doc.createNestedObject("actions");
  actions["set_target"]["title"] = "Set Target Device";
  JsonObject inputST = actions["set_target"].createNestedObject("input");
  inputST["type"] = "object";
  JsonObject propsST = inputST.createNestedObject("properties");
  propsST["target_id"]["type"] = "string";
  propsST["target_id"]["title"] = "Target Camera ID";

  actions["update_firmware"]["title"] = "Update Firmware";
  JsonObject inputUF = actions["update_firmware"].createNestedObject("input");
  inputUF["type"] = "object";
  JsonObject propsUF = inputUF.createNestedObject("properties");
  propsUF["url"]["type"] = "string";
  propsUF["url"]["title"] = "Firmware URL";

  String payload;
  serializeJson(doc, payload);
  int httpCode = http.PUT(payload);
  // Serial.print("TD Update Code: "); Serial.println(httpCode); // Clean logs
  // if (httpCode > 0) Serial.println(http.getString());
  http.end();
  delete client;
}

void parseThingDescription(JsonObject td) {
  if (!td.containsKey("properties"))
    return;
  properties.clear();
  hasCameraProp = false;

  JsonObject props = td["properties"];
  for (JsonPair p : props) {
    String key = p.key().c_str();

    // Detect Camera
    if (key == "snapshot_resolution" || key == "stream_resolution") {
      hasCameraProp = true;
    }

    JsonObject details = p.value().as<JsonObject>();
    String type = details["type"] | "string";
    if (type == "object" || type == "array")
      continue;

    Property prop;
    prop.key = key;
    prop.title = details["title"] | key;
    prop.unit = details["unit"] | "";
    properties.push_back(prop);
  }
}

void pollDevice() {
  if (strlen(config.target_device_id) == 0)
    return;

  WiFiClient *client = nullptr;
  if (String(config.server_url).startsWith("https")) {
    WiFiClientSecure *sc = new WiFiClientSecure();
    sc->setInsecure();
    // Critical: Increase RX buffer to handle larger TLS records and prevent
    // truncation
    sc->setBufferSizes(4096, 512);
    client = sc;
  } else {
    client = new WiFiClient();
  }

  HTTPClient http;
  String url = String(config.server_url) + "/dev/" + config.target_device_id +
               "?token=" + String(config.user_token);
  // Serial.print("Polling URL: "); Serial.println(url); // Clean logs

  http.begin(*client, url);
  http.addHeader("Authorization", "Bearer " + String(config.user_token));
  http.setTimeout(3000); // Reduce timeout to prevent WDT hang

  int httpCode = http.GET();
  // Serial.print("Poll Code: "); Serial.println(httpCode); // Clean logs
  if (httpCode == 200) {
    lastScanError = ""; // Clear error on success

    // FPS Calculation
    frameCount++;
    unsigned long now = millis();
    if (now - lastFpsTime >= 1000) {
      fps = frameCount * 1000.0 / (now - lastFpsTime);
      frameCount = 0;
      lastFpsTime = now;
      Serial.printf("FPS: %.2f\n", fps);
    }
    // Read payload into String (now reliable with larger buffer)
    String payload = http.getString();
    // Serial.printf("Payload Len: %d\n", payload.length()); // Clean logs

    // Safety check for empty payload
    if (payload.length() == 0) {
      Serial.println("Error: Empty Payload");
    } else {
      // Optimization: Filter JSON to only keep what we need
      StaticJsonDocument<200> filter;
      filter["status"] = true;
      filter["thing_description"] = true;
      filter["shadow_state"] = true;

      // 3KB buffer for the doc
      DynamicJsonDocument doc(3072);
      DeserializationError error =
          deserializeJson(doc, payload, DeserializationOption::Filter(filter));

      if (!error) {
        // Serial.print("Filtered Doc: ");
        // serializeJson(doc, Serial);
        // Serial.println();

        isTargetOnline = (doc["status"] | "offline") == "online";
        // Serial.printf("Status: %s\n", isTargetOnline ? "Online" : "Offline");
        // // Clean logs

        if (doc.containsKey("thing_description")) {
          // Serial.println("TD found");
          if (properties.empty()) {
            parseThingDescription(doc["thing_description"]);
            // Serial.printf("Props loaded: %d\n", properties.size()); // Clean
            // logs
          }
        }

        if (doc.containsKey("shadow_state")) {
          JsonObject shadow = doc["shadow_state"];
          for (JsonPair p : shadow) {
            String valStr;
            if (p.value().is<bool>()) {
              valStr = p.value().as<bool>() ? "true" : "false";
            } else {
              valStr = p.value().as<String>();
            }
            updateValueCache(p.key().c_str(), valStr);
          }
        }
      } else {
        Serial.print("JSON Parse Failed: ");
        Serial.println(error.c_str());
        // Debug: Print preview if parse fails
        if (payload.length() > 100)
          Serial.println("Prev: " + payload.substring(0, 100));
      }
    }
  } else {
    if (httpCode != 200 && httpCode != -1) // Only log real errors
      Serial.printf("Poll Failed: %d\n", httpCode);
    isTargetOnline = false;
  }
  if (httpCode != 200) {
    lastScanError = "Err: " + String(httpCode);
  }
  http.end();
  delete client;
}

// -- Web Handlers --

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>DatumDash Status</title>
<style>
body{background:#1b1b1b;color:white;font-family:sans-serif;padding:20px;text-align:center}
.card{background:#2d2d2d;padding:20px;border-radius:8px;max-width:400px;margin:0 auto}
h2{color:#00bcd4}
p{font-size:18px}
.btn{background:#00bcd4;color:white;text-decoration:none;padding:10px 20px;border-radius:4px;display:inline-block;margin:5px}
.btn-red{background:#f44336}
</style></head><body>
<div class="card">
<h2>🟢 System Online</h2>
<p><b>Device ID:</b> %DID%</p>
<p><b>Target:</b> %TID%</p>
<hr style="border-color:#444">
<form action="/save-settings" method="POST">
<h3>⚙️ Settings</h3>
<input type="text" name="target_id" placeholder="Target Device ID" value="%TID%">
<p style="text-align:left;font-size:14px;color:#aaa">Select Overlay Data:</p>
<div style="text-align:left;padding:0 20px;">
%CHECKBOXES%
</div>
<h3>Carousel Speed</h3>
<label>Slide Interval (ms): </label>
<input type="number" name="slide_interval" value="%SLIDE_VAL%" min="0" style="width:80px">
<br><br>
<button type="submit" class="btn">Save Settings</button>
</form>
<hr style="border-color:#444">
<a href="/update" class="btn">☁️ Firmware Update</a>
<a href="/reset" class="btn btn-red" onclick="return confirm('Reset?')">⚠️ Factory Reset</a>
</div></body></html>

)rawliteral";
  html.replace("%DID%", String(config.device_id));
  html.replace("%TID%", String(config.target_device_id));
  html.replace("%DID%", String(config.device_id));
  html.replace("%TID%", String(config.target_device_id));
  html.replace("%SLIDE_VAL%", String(config.slide_interval));

  // Generate Checkboxes
  String checkboxes = "";
  if (properties.empty()) {
    checkboxes = "<i>No properties found. Connect to target first.</i>";
  } else {
    for (auto &p : properties) {
      // Only show relevant props (numbers mostly)
      checkboxes +=
          "<label><input type='checkbox' name='overlay' value='" + p.key + "'";
      if (strstr(config.overlay_filter, p.key.c_str())) {
        checkboxes += " checked";
      }
      checkboxes += "> " + p.title + " (" + p.key + ")</label><br>";
    }
  }
  html.replace("%CHECKBOXES%", checkboxes);

  server.send(200, "text/html", html);
}

void handleWebReset() {
  memset(&config, 0, sizeof(Config));
  config.magic = CONFIG_MAGIC;
  saveConfig();
  server.send(200, "text/html", "Resetting...");
  delay(1000);
  ESP.restart();
  ESP.restart();
}

void handleSaveSettings() {
  String t = server.arg("target_id");
  if (t.length() > 0)
    strncpy(config.target_device_id, t.c_str(),
            sizeof(config.target_device_id));

  // Parse multiple overlay checkboxes
  String newOverlay = "";
  for (int i = 0; i < server.args(); i++) {
    if (server.argName(i) == "overlay") {
      if (newOverlay.length() > 0)
        newOverlay += ",";
      newOverlay += server.arg(i);
    }
  }

  // Always update filter (empty if none selected)
  strncpy(config.overlay_filter, newOverlay.c_str(),
          sizeof(config.overlay_filter));

  if (server.hasArg("slide_interval")) {
    config.slide_interval = server.arg("slide_interval").toInt();
  }

  saveConfig();

  server.send(200, "text/html",
              "<html><head><meta name='viewport' "
              "content='width=device-width'><style>body{background:#1b1b1b;"
              "color:white;font-family:sans-serif;text-align:center;padding:"
              "50px;}</style></head><body><h1>✅ "
              "Saved!</h1><p>Redirecting...</"
              "p><script>setTimeout(function(){window.location.href='/';}, "
              "1500);</script></body></html>");

  // Reset state to force refresh
  properties.clear();
  lastPollTime = 0;
}

void startSetupMode() {
  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  String apName = "DatumDash-" + String(ESP.getChipId(), HEX);
  WiFi.softAP(apName.c_str());

  server.enableCORS(true);
  httpUpdater.setup(&server);
  dnsServer.start(53, "*", WiFi.softAPIP());

  showStatus("SETUP MODE", ST77XX_BLUE);
  tft.setCursor(10, 100);
  tft.print("AP: ");
  tft.println(apName);
  tft.print("IP: ");
  tft.println(WiFi.softAPIP());

  server.on("/", []() { server.send(200, "text/html", PROVISION_HTML); });

  server.on("/scan", []() {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; ++i) {
      if (i)
        json += ",";
      json += "{\"ssid\":\"" + WiFi.SSID(i) +
              "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]";
    server.send(200, "application/json", json);
  });

  // -- Mobile App Endpoints --
  server.on("/info", []() {
    String json = "{\"device_uid\":\"" + String(ESP.getChipId()) +
                  "\",\"firmware_version\":\"" + FIRMWARE_VER +
                  "\",\"device_type\":\"" + DEVICE_TYPE_NAME + "\"}";
    server.send(200, "application/json", json);
  });

  server.on("/configure", HTTP_POST, []() {
    String ssid = server.arg("wifi_ssid");
    String pass = server.arg("wifi_pass");
    String token = server.arg("user_token"); // Mobile app sends 'user_token'
    String url = server.arg("server_url");
    String devName =
        server.arg("device_name"); // Not used yet, but good to know

    if (url.length() > 0)
      strncpy(config.server_url, url.c_str(), sizeof(config.server_url));

    if (ssid.length() > 0)
      strncpy(config.ssid, ssid.c_str(), sizeof(config.ssid));
    if (pass.length() > 0)
      strncpy(config.password, pass.c_str(), sizeof(config.password));
    if (token.length() > 0) {
      strncpy(config.user_token, token.c_str(), sizeof(config.user_token));
      Serial.print("Saving User Token: ");
      Serial.println(config.user_token);
    }

    config.device_id[0] = 0;
    config.api_key[0] = 0;
    saveConfig();

    server.send(200, "application/json", "{\"status\":\"success\"}");
    delay(1000);
    ESP.restart();
  });

  server.on("/save", HTTP_POST, []() {
    String ssid = server.arg("custom_ssid");
    if (ssid.length() == 0)
      ssid = server.arg("ssid");
    String pass = server.arg("password");
    String token = server.arg("token");
    String email = server.arg("email");
    String upass = server.arg("user_pass");
    String url = server.arg("server_url");
    String target = server.arg("target_id");

    if (url.length() > 0)
      strncpy(config.server_url, url.c_str(), sizeof(config.server_url));
    if (target.length() > 0)
      strncpy(config.target_device_id, target.c_str(),
              sizeof(config.target_device_id));
    if (server.arg("poll_int").length() > 0)
      config.poll_interval = server.arg("poll_int").toInt();
    if (server.arg("slide_int").length() > 0)
      config.slide_interval = server.arg("slide_int").toInt();

    if (token.length() == 0 && email.length() > 0) {
      HTTPClient http;
      WiFiClient client;
      http.begin(client, String(config.server_url) + "/auth/login");
      http.addHeader("Content-Type", "application/json");
      int c = http.POST("{\"email\":\"" + email + "\",\"password\":\"" + upass +
                        "\"}");
      if (c == 200) {
        DynamicJsonDocument doc(512);
        deserializeJson(doc, http.getString());
        token = doc["token"].as<String>();
      }
      http.end();
    }

    if (ssid.length() > 0)
      strncpy(config.ssid, ssid.c_str(), sizeof(config.ssid));
    if (pass.length() > 0)
      strncpy(config.password, pass.c_str(), sizeof(config.password));
    if (token.length() > 0)
      strncpy(config.user_token, token.c_str(), sizeof(config.user_token));

    config.device_id[0] = 0;
    config.api_key[0] = 0;
    saveConfig();
    server.send(200, "text/html", "Saved! Restarting...");
    delay(1000);
    ESP.restart();
  });

  server.begin();
  while (true) {
    dnsServer.processNextRequest();
    server.handleClient();
  }
}

// -- Main Setup & Loop --

void setup() {
  Serial.begin(115200);

  // Manual Reset for TFT
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);
  delay(10);
  digitalWrite(TFT_RST, LOW);
  delay(10);
  digitalWrite(TFT_RST, HIGH);
  delay(10);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW); // Max Brightness

  tft.init(240, 240, SPI_MODE3);
  tft.setSPISpeed(40000000); // Boost to 40MHz
  tft.invertDisplay(true);
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);

  // TJpg Dec Init
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false); // Fix Color Mismatch (RGB vs BGR)
  TJpgDec.setCallback(tft_output);

  // Splash
  tft.setTextSize(3);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds("DatumDash", 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, (240 - h) / 2);
  tft.setTextColor(ST77XX_CYAN);
  tft.print("DatumDash");
  delay(2000);

  loadConfig();
  config.boot_failures++;
  saveConfig();

  if (config.boot_failures >= 5) {
    tft.fillScreen(ST77XX_RED);
    tft.setCursor(10, 100);
    tft.println("Boot Failures! Resetting...");
    memset(&config, 0, sizeof(Config));
    config.magic = CONFIG_MAGIC;
    saveConfig();
    delay(2000);
  }

  if (strlen(config.ssid) == 0)
    startSetupMode();

  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.password);
  Serial.print("Connecting to WiFi: ");
  Serial.println(config.ssid);

  showStatus("Connecting...", ST77XX_YELLOW);
  int r = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (++r > 40) {
      Serial.println("\nWiFi Timeout! Restarting...");
      ESP.restart();
    }
  }
  Serial.println("\nWiFi Connected!");

  config.boot_failures = 0;
  saveConfig();

  showStatus("Online!", ST77XX_GREEN);
  tft.print("IP: ");
  tft.println(WiFi.localIP());
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  if (strlen(config.device_id) == 0 && strlen(config.user_token) > 0) {
    tft.println("\nRegistering...");
    Serial.println("Registering Device...");
    if (registerDeviceManual(config.server_url, config.user_token)) {
      tft.println("Success!");
      Serial.println("Registration Success!");
      sendThingDescription();
    } else {
      tft.println("Failed!");
      delay(2000);
      startSetupMode();
    }
  } else {
    sendThingDescription();
  }

  httpUpdater.setup(&server);
  server.on("/", handleRoot);
  server.on("/save-settings", HTTP_POST, handleSaveSettings);
  server.on("/reset", handleWebReset);
  server.begin();

  connectMQTT();
}

void loop() {
  server.handleClient();
  if (WiFi.status() == WL_CONNECTED) {
    unsigned long now = millis();

    if (!mqttClient.connected()) {
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        if (connectMQTT())
          lastReconnectAttempt = 0;
      }
    } else {
      mqttClient.loop();
    }

    // Poll Device Logic
    // If poll_interval is 0, default to 5 seconds to prevent network
    // flooding/FPS drop
    unsigned long pInt = config.poll_interval > 0
                             ? (unsigned long)config.poll_interval * 1000UL
                             : 5000UL;

    if (now - lastPollTime > pInt) {
      lastPollTime = now;
      pollDevice();
    }

    // FPS Display
    tft.setCursor(0, 0);
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.print("FPS:");
    tft.print(int(fps));

    // Dynamic Display Update Frequency
    // If showing Camera, update as fast as possible (0 delay).
    // If showing Status Cards, rotate every 5000ms.
    // Dynamic Display Update Frequency
    // If showing Camera, update as fast as possible (0 delay).
    // If showing Status Cards, rotate every 5000ms.
    bool streamActive =
        (hasCameraProp && getCachedValue("stream_enabled") == "true");

    // Check for mode switch to clear screen
    static bool lastStreamActive = false;
    if (streamActive != lastStreamActive) {
      tft.fillScreen(ST77XX_BLACK);
      lastStreamActive = streamActive;
    }

    unsigned long displayInterval = streamActive ? 0 : 5000;

    // Use a separate timer for display refresh to not conflict with carousel
    static unsigned long lastDisplayTime = 0;
    if (now - lastDisplayTime > displayInterval) {
      lastDisplayTime = now;
      updateDisplay();
    }

    if (now - lastTelemetryTime > 10000) { // 10s Telemetry Report
      lastTelemetryTime = now;
      reportTelemetry();
    }
  }
}
