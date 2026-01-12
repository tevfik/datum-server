#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <DatumIoT.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>       // Explicit include for ESP8266
#include <ESP8266HTTPUpdateServer.h> // Web-based OTA
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>

// -- Hardware Pin Definitions for GeekMagic TV (ESP8266) --
#define TFT_MOSI 13 // D7
#define TFT_SCLK 14 // D5
#define TFT_CS -1
#define TFT_DC 0  // D3
#define TFT_RST 2 // D4
#define TFT_BL 5  // D1

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// -- Globals --
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
DNSServer dnsServer;
WiFiClient espClient;
DatumIoT datum(espClient);

struct Config {
  char ssid[32];
  char password[64];
  char user_token[64] = ""; // User Token for Datum
  char device_id[32] = "";
  char api_key[64] = "";
  char server_url[64] = "https://datum.bezg.in";
  int boot_failures = 0; // Track consecutive failures
};
Config config;

// HTML for Onboarding Portal
const char PROVISION_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>GeekMagic Setup</title>
<style>
body{background:#1b1b1b;color:white;font-family:sans-serif;padding:20px}
.card{background:#2d2d2d;padding:20px;border-radius:8px;max-width:400px;margin:0 auto}
input,select{width:100%;padding:10px;margin:5px 0 15px;box-sizing:border-box;background:#444;border:none;color:white;border-radius:4px}
button{background:#00bcd4;color:white;border:none;padding:12px;width:100%;border-radius:4px;font-size:16px;cursor:pointer}
h2{text-align:center;color:#00bcd4}
</style></head><body>
<div class="card">
<h2>🚀 GeekMagic Setup</h2>
<form action="/save" method="POST">
<label>WiFi SSID</label>
<select id="ssid" name="ssid" onchange="checkCustom(this)"><option value="">Scanning...</option><option value="__custom__">Custom...</option></select>
<input type="text" id="custom_ssid" name="custom_ssid" placeholder="Enter SSID" style="display:none">
<label>WiFi Password</label><input type="password" name="password">
<hr style="border-color:#444;margin:20px 0">
<label>Datum User Token</label><input type="text" name="token" placeholder="Paste Token from Console">
<div style="text-align:center;margin:10px">- OR -</div>
<label>Datum Account Email</label><input type="email" name="email">
<label>Password</label><input type="password" name="user_pass">
<input type="hidden" name="server_url" value="https://datum.bezg.in">
<button type="submit">Save & Connect</button>
</form>
</div>
<script>
fetch('/scan').then(r=>r.json()).then(d=>{
  const s=document.getElementById('ssid'); s.innerHTML='';
  d.forEach(n=>{const o=document.createElement('option');o.value=n.ssid;o.innerText=n.ssid+(n.auth?' 🔒':'');s.appendChild(o)});
  const o=document.createElement('option');o.value='__custom__';o.innerText='Type Manually...';s.appendChild(o);
});
function checkCustom(e){
  document.getElementById('custom_ssid').style.display = (e.value==='__custom__')?'block':'none';
}
</script></body></html>
)rawliteral";

// -- Helper Functions --
void loadConfig() {
  EEPROM.begin(512);
  EEPROM.get(0, config);
  // Simple check if uninitialized (SSID starting with 0xFF)
  if (config.ssid[0] == 0xFF) {
    config.ssid[0] = 0; // Invalid
    config.boot_failures = 0;
  }
}

void saveConfig() {
  EEPROM.put(0, config);
  EEPROM.commit();
}

void showStatus(String msg, uint16_t color) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 10);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.println("GeekMagic TV");
  tft.setTextSize(1);
  tft.println("Datum IoT");
  tft.println("");

  tft.setTextColor(color);
  tft.setTextSize(2);
  tft.println(msg);
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>GeekMagic Status</title>
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
<p><b>IP Address:</b> %IP%</p>
<hr style="border-color:#444">
<a href="/update" class="btn">☁️ Firmware Update</a>
<a href="/reset" class="btn btn-red" onclick="return confirm('Reset WiFi settings?')">⚠️ Factory Reset</a>
</div></body></html>
)rawliteral";

  html.replace("%DID%", String(config.device_id));
  html.replace("%IP%", WiFi.localIP().toString());
  server.send(200, "text/html", html);
}

void handleWebReset() { // Renamed to avoid confusing with handleRoot
                        // Clear Config
  config.ssid[0] = 0;
  config.password[0] = 0;
  config.boot_failures = 0;
  saveConfig();
  server.send(
      200, "text/html",
      "<h1>Resetting...</h1><p>Device will reboot into Setup Mode.</p>");
  delay(1000);
  ESP.restart();
}

void startSetupMode() {
  WiFi.mode(WIFI_AP);
  // Reset boot failures so we don't start in loop if user reboots during setup
  if (config.boot_failures > 0) {
    config.boot_failures = 0;
    saveConfig();
  }

  String apName = "GeekMagic-" + String(ESP.getChipId(), HEX);
  WiFi.softAP(apName.c_str());

  dnsServer.start(53, "*", WiFi.softAPIP());

  showStatus("SETUP MODE", ST77XX_BLUE);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.println("");
  tft.print("AP: ");
  tft.println(apName);
  tft.print("IP: ");
  tft.println(WiFi.softAPIP());
  tft.println("");
  tft.println("Connect to WiFi");
  tft.println("to configure.");

  server.on("/", []() { server.send(200, "text/html", PROVISION_HTML); });

  server.on("/scan", []() {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; ++i) {
      if (i)
        json += ",";
      json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"auth\":" +
              (WiFi.encryptionType(i) == ENC_TYPE_NONE ? "0" : "1") + "}";
    }
    json += "]";
    server.send(200, "application/json", json);
  });

  server.on("/save", HTTP_POST, []() {
    // ... (Save logic matches previous) ...
    String ssid = server.arg("ssid");
    if (ssid == "__custom__")
      ssid = server.arg("custom_ssid");
    String pass = server.arg("password");
    String token = server.arg("token");
    String email = server.arg("email");
    String upass = server.arg("user_pass");
    String url = server.arg("server_url");

    if (url.length() > 0)
      strncpy(config.server_url, url.c_str(), sizeof(config.server_url));

    if (token.length() == 0 && email.length() > 0 && upass.length() > 0) {
      HTTPClient http;
      WiFiClient client;
      http.begin(client, String(config.server_url) + "/auth/login");
      http.addHeader("Content-Type", "application/json");
      String payload =
          "{\"email\":\"" + email + "\",\"password\":\"" + upass + "\"}";
      int code = http.POST(payload);
      if (code == 200) {
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
    config.boot_failures = 0;

    saveConfig();
    server.send(200, "text/html", "<h1>Saved!</h1><p>Restarting...</p>");
    delay(1000);
    ESP.restart();
  });

  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();

  while (true) {
    dnsServer.processNextRequest();
    server.handleClient();
    delay(10);
  }
}

void setup() {
  Serial.begin(115200);

  // -- Init Display --
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, 128);
  tft.init(240, 240);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);

  loadConfig();

  // -- Boot Failure Logic --
  config.boot_failures++;
  saveConfig();
  Serial.printf("Boot Failure Count: %d\n", config.boot_failures);

  if (config.boot_failures >= 5) {
    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(2);
    tft.setCursor(10, 50);
    tft.println("RESETTING...");
    tft.setTextSize(1);
    tft.println("5 failed boots detected.");
    config.ssid[0] = 0;
    config.password[0] = 0;
    config.boot_failures = 0;
    saveConfig();
    delay(2000);
  }

  if (String(config.ssid).length() == 0) {
    startSetupMode();
  }

  // Connect WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.password);

  showStatus("Connecting...", ST77XX_YELLOW);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (retry > 60) {
      ESP.restart();
    }
    retry++;
  }

  if (config.boot_failures > 0) {
    config.boot_failures = 0;
    saveConfig();
  }

  showStatus("Online!", ST77XX_GREEN);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("IP: ");
  tft.println(WiFi.localIP());

  // OTA (Arduino IDE)
  ArduinoOTA.setHostname("geekmagic-tv");
  ArduinoOTA.onStart([]() {
    tft.fillScreen(ST77XX_BLUE);
    tft.setCursor(10, 50);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(3);
    tft.println("OTA UPDATE");
    tft.setTextSize(1);
    tft.setCursor(10, 100);
    tft.println("Please wait...");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int p = (progress * 100) / total;
    tft.fillRect(20, 140, 200, 20, ST77XX_WHITE);
    tft.fillRect(22, 142, map(p, 0, 100, 0, 196), 16, ST77XX_GREEN);
  });
  ArduinoOTA.begin();

  // WEB OTA & Status Page
  httpUpdater.setup(&server); // Adds /update endpoint
  server.on("/", handleRoot);
  server.on("/reset", handleWebReset);
  server.begin(); // START WEB SERVER
  Serial.println("Web Server Started");

  // Datum
  datum.setServer(config.server_url);

  // Register if needed
  if (strlen(config.device_id) == 0 && strlen(config.user_token) > 0) {
    tft.println("Registering...");
    if (datum.registerDevice(config.server_url, config.user_token,
                             "GeekMagicTV", "display")) {
      strncpy(config.device_id, datum.getDeviceId().c_str(),
              sizeof(config.device_id));
      strncpy(config.api_key, datum.getApiKey().c_str(),
              sizeof(config.api_key));
      saveConfig();
      tft.println("Registered!");
    } else {
      tft.println("Reg Failed!");
      delay(3000);
    }
  }

  if (strlen(config.device_id) > 0) {
    datum.begin(config.server_url, config.device_id, config.api_key);
  }
}

void loop() {
  ArduinoOTA.handle();   // Check IDE OTA
  server.handleClient(); // Check Web Requests
  datum.loop();          // Check MQTT/Datum

  static unsigned long last = 0;
  if (millis() - last > 1000) {
    last = millis();
    tft.fillCircle(220, 10, 4, ST77XX_GREEN);
    delay(100);
    tft.fillCircle(220, 10, 4, ST77XX_BLACK);
  }
}
