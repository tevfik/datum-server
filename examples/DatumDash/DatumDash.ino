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

// -- Globals --
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
DNSServer dnsServer;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// -- Config Structure --
#define CONFIG_MAGIC 0xD4701105
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
};
Config config;

// -- Dash Globals --
unsigned long lastPollTime = 0;
unsigned long lastCarouselTime = 0;
bool isTargetOnline = false;
unsigned long lastReconnectAttempt = 0;
unsigned long lastTelemetryTime = 0;

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
<label>Datum User Token</label><input type="text" name="token" placeholder="Paste Token from Console">
<div style="text-align:center;margin:10px">- OR -</div>
<label>Datum Account Email</label><input type="email" name="email">
<label>Password</label><input type="password" name="user_pass">
<hr style="border-color:#444;margin:20px 0">
<label>Target Device ID (to Watch)</label><input type="text" name="target_id" placeholder="ID of device to display">
<label>Poll Interval (Seconds)</label><input type="number" name="poll_int" value="5" min="1">
<input type="hidden" name="server_url" value="https://datum.bezg.in">
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
    config.magic = CONFIG_MAGIC;
    strcpy(config.server_url, "https://datum.bezg.in");
    config.poll_interval = 5;
    saveConfig();
  }
}

void saveConfig() {
  config.magic = CONFIG_MAGIC;
  EEPROM.put(0, config);
  EEPROM.commit();
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

  WiFiClient *client = nullptr;
  if (String(config.server_url).startsWith("https")) {
    WiFiClientSecure *secureClient = new WiFiClientSecure();
    secureClient->setInsecure();
    client = secureClient;
  } else {
    client = new WiFiClient();
  }

  HTTPClient http;
  String url = String(config.server_url) + "/dev/" + config.target_device_id +
               "/stream/snapshot";
  http.begin(*client, url);
  http.addHeader("Authorization", "Bearer " + String(config.api_key));

  int httpCode = http.GET();
  if (httpCode == 200) {
    int len = http.getSize();
    // Safety Limit: ESP8266 has limited RAM.
    // QVGA is ~7-10KB JPG. Max buffer 20KB.
    if (len > 0 && len < 25000) {
      uint8_t *valBuffer = (uint8_t *)malloc(len);
      if (valBuffer) {
        WiFiClient *stream = http.getStreamPtr();
        int idx = 0;
        while (http.connected() && (len > 0 || len == -1)) {
          size_t size = stream->available();
          if (size) {
            int c = stream->readBytes(valBuffer + idx, size);
            idx += c;
            if (len > 0)
              len -= c;
          } else {
            delay(1);
          }
          if (len == 0)
            break;
        }

        // tft.setSwapBytes(true); // NOT SUPPORTED in Adafruit_GFX, handled by
        // TJpgDec.setSwapBytes(true) Draw centered-ish: -40, 0 usually works
        // for 320x240 on 240x240
        TJpgDec.drawJpg(-40, 0, valBuffer, idx);
        // tft.setSwapBytes(false); // Restore

        free(valBuffer);

        tft.setTextColor(ST77XX_GREEN);
        tft.setCursor(5, 5);
        tft.setTextSize(1);
        tft.print("LIVE");
      } else {
        showStatus("OOM: Img Too Big", ST77XX_RED);
      }
    } else {
      showStatus("Img Too Large", ST77XX_RED);
    }
  } else {
    showStatus("Cam Offline", ST77XX_RED);
  }
  http.end();
  delete client;

  tft.drawLine(0, 210, 240, 210, ST77XX_WHITE);
  tft.setCursor(10, 220);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("Target: ");
  tft.print(config.target_device_id);
}

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
    showStatus("Scanning...", ST77XX_BLUE);
    return;
  }

  // Carousel Logic
  if (currentPropIndex >= properties.size()) {
    if (hasCameraProp) {
      drawCameraView();
      currentPropIndex++;
      // Show cam for 2 cycles
      if (currentPropIndex > properties.size() + 2) {
        currentPropIndex = 0;
      }
      return;
    } else {
      currentPropIndex = 0;
    }
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

      sendLog("OTA Starting");
      showStatus("FW UPDATING...", ST77XX_BLUE);

      WiFiClientSecure client;
      client.setInsecure();
      client.setTimeout(10000);
      t_httpUpdate_return ret = ESPhttpUpdate.update(client, fwUrl);
      if (ret == HTTP_UPDATE_OK)
        ESP.restart();
    }
  } else if (action == "set_target") {
    String newTarget = doc["params"]["target_id"] | "";
    if (newTarget.length() > 0) {
      strncpy(config.target_device_id, newTarget.c_str(),
              sizeof(config.target_device_id));
      config.poll_interval =
          doc["params"]["poll_interval"] | config.poll_interval;
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
  mqttClient.setBufferSize(2048);
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
      memset(config.user_token, 0, sizeof(config.user_token));
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
  actions["update_firmware"]["title"] = "Update Firmware";

  String payload;
  serializeJson(doc, payload);
  http.PUT(payload);
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
    client = sc;
  } else {
    client = new WiFiClient();
  }

  HTTPClient http;
  String url = String(config.server_url) + "/dev/" + config.target_device_id;
  http.begin(*client, url);
  http.addHeader("Authorization", "Bearer " + String(config.api_key));

  int httpCode = http.GET();
  if (httpCode == 200) {
    // DEBUG: Print payload to see what we are receiving
    String payload = http.getString();
    Serial.printf("HTTP 200. Len: %d. Payload:\n", payload.length());
    Serial.println(payload);

    if (payload.length() == 0) {
      Serial.println("Error: Empty Payload");
    } else {
      DynamicJsonDocument doc(4096);
      DeserializationError error =
          deserializeJson(doc, payload); // Revert to string parse for debug

      if (!error) {
        isTargetOnline = (doc["status"] | "offline") == "online";
        // Only parse TD if we don't have properties yet
        if (doc.containsKey("thing_description") && properties.empty()) {
          parseThingDescription(doc["thing_description"]);
        }
        if (doc.containsKey("shadow_state")) {
          JsonObject shadow = doc["shadow_state"];
          for (JsonPair p : shadow)
            updateValueCache(p.key().c_str(), p.value().as<String>());
        }
      } else {
        Serial.print("JSON Parse Failed: ");
        Serial.println(error.c_str());
      }
    }
  } else {
    Serial.printf("Poll Failed: %d\n", httpCode);
    isTargetOnline = false;
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
<a href="/update" class="btn">☁️ Firmware Update</a>
<a href="/reset" class="btn btn-red" onclick="return confirm('Reset?')">⚠️ Factory Reset</a>
</div></body></html>
)rawliteral";
  html.replace("%DID%", String(config.device_id));
  html.replace("%TID%", String(config.target_device_id));
  server.send(200, "text/html", html);
}

void handleWebReset() {
  memset(&config, 0, sizeof(Config));
  config.magic = CONFIG_MAGIC;
  saveConfig();
  server.send(200, "text/html", "Resetting...");
  delay(1000);
  ESP.restart();
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
  tft.invertDisplay(true);
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);
  tft.setSPISpeed(20000000);

  // TJpg Dec Init
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
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

  showStatus("Connecting...", ST77XX_YELLOW);
  int r = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (++r > 40)
      ESP.restart();
  }

  config.boot_failures = 0;
  saveConfig();

  showStatus("Online!", ST77XX_GREEN);
  tft.print("IP: ");
  tft.println(WiFi.localIP());

  if (strlen(config.device_id) == 0 && strlen(config.user_token) > 0) {
    tft.println("\nRegistering...");
    if (registerDeviceManual(config.server_url, config.user_token)) {
      tft.println("Success!");
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

    if (now - lastPollTime > (unsigned long)config.poll_interval * 1000UL) {
      lastPollTime = now;
      pollDevice();
    }

    if (now - lastCarouselTime > 5000) {
      lastCarouselTime = now;
      updateDisplay();
    }

    if (now - lastTelemetryTime > 60000) {
      lastTelemetryTime = now;
      reportTelemetry();
    }
  }
}
