#include "mqtt_manager.h"
#include "camera_manager.h" // For frame size, snap, streaming constants
#include "eif_motion.h"     // For motion settings
#include "ota_manager.h"    // For updateFirmware
#include "wifi_manager.h"   // For getPublicIP
#include <ArduinoJson.h>
#include <HTTPClient.h> // For ackCommand
#include <Preferences.h>

// Extern Globals from Main
extern Preferences prefs;
extern String deviceUID;
extern String deviceName;
extern String apiKey;
extern String serverURL;
extern String wifiSSID;
extern String wifiPass;
extern String deviceID;
extern String userToken;
extern const char
    *FIRMWARE_VERSION; // Need to check if this is available or string
// FIRMWARE_VERSION is usually #define. If #defined in main, it's not visible
// here! I should probably define it in a shared header or redeclare it. For
// now, I'll use a hardcoded string or expect it to be passed? Better: create
// `shared_config.h` or similar? Or just redeclare #define here?
#define FIRMWARE_VERSION "2.1.1" // Bump version

// LED Globals (Extern from Main)
extern byte savedR;
extern byte savedG;
extern byte savedB;
extern int savedBrightness;
extern bool torchState;
extern void updateLED(); // Main usually updates LED, but callback sets state?
// logic in callback: neopixelWrite checks LED_GPIO_NUM
// We need LED_GPIO_NUM.
// It was defined in main via macros. I need those macros!
// I'll copy the board macros to a shared header later?
// Or copy them here?
// Since I already copied them to camera_manager.cpp, I should probably put them
// in `board_config.h`. But user wants quick refactor. I'll duplicate board
// macros here for safety. Note: This is technical debt.

#include "camera_pins.h"

// Extern Globals from Main
extern void neopixelWrite(uint8_t pin, uint8_t red, uint8_t green,
                          uint8_t blue);

// MQTT Objects
WiFiClient espClient;
PubSubClient mqttClient(espClient);
String mqttHost;

// Helpers for JSON parsing
String extractJsonVal(String json, String key) {
  int keyIdx = json.indexOf("\"" + key + "\"");
  if (keyIdx < 0)
    return "";
  int colonIdx = json.indexOf(":", keyIdx);
  if (colonIdx < 0)
    return "";
  int valStart = json.indexOf("\"", colonIdx);
  if (valStart < 0)
    return "";
  int valEnd = json.indexOf("\"", valStart + 1);
  if (valEnd < 0)
    return "";
  return json.substring(valStart + 1, valEnd);
}

int extractJsonInt(String json, String key) {
  int keyIdx = json.indexOf("\"" + key + "\"");
  if (keyIdx < 0)
    return -1;
  int colonIdx = json.indexOf(":", keyIdx);
  if (colonIdx < 0)
    return -1;
  int s = colonIdx + 1;
  while (s < json.length() && !isDigit(json.charAt(s)) && json.charAt(s) != '-')
    s++;
  if (s >= json.length())
    return -1;
  int e = s;
  while (e < json.length() &&
         (isDigit(json.charAt(e)) || json.charAt(e) == '-'))
    e++;
  return json.substring(s, e).toInt();
}

bool extractJsonBool(String json, String key) {
  int keyIdx = json.indexOf("\"" + key + "\"");
  if (keyIdx < 0)
    return false;
  int colonIdx = json.indexOf(":", keyIdx);
  if (colonIdx < 0)
    return false;
  String remainder = json.substring(colonIdx + 1);
  remainder.trim();
  if (remainder.startsWith("true") || remainder.startsWith("1"))
    return true;
  return false;
}

// ACK Command
void ackCommand(String cmdId) {
  if (deviceID.length() == 0 || apiKey.length() == 0)
    return;
  HTTPClient http;
  http.begin(serverURL + "/dev/" + deviceID + "/cmd/" + cmdId + "/ack");
  http.addHeader("Authorization", "Bearer " + apiKey);
  http.addHeader("Content-Type", "application/json");
  http.POST("{\"status\":\"executed\"}");
  http.end();
}

// MQTT Callback
void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String pl = "";
  for (unsigned int i = 0; i < length; i++) {
    pl += (char)payload[i];
  }

  Serial.print("[MQTT]-> ");
  Serial.println(pl); // Clean log

  String pid = extractJsonVal(pl, "id");
  if (pid.length() == 0)
    pid = extractJsonVal(pl, "command_id");
  String action = extractJsonVal(pl, "action");

  String paramsBlock = "";
  int pstart = pl.indexOf("\"params\":");
  if (pstart > 0) {
    int pvalStart = pl.indexOf('{', pstart);
    if (pvalStart > 0) {
      int pend = pvalStart;
      int pcount = 1;
      while (pcount > 0 && pend < pl.length() - 1) {
        pend++;
        if (pl.charAt(pend) == '{')
          pcount++;
        else if (pl.charAt(pend) == '}')
          pcount--;
      }
      if (pcount == 0)
        paramsBlock = pl.substring(pvalStart, pend + 1);
    }
  }

  if (pid.length() > 0 && action.length() > 0) {
    // Timeout Check (Stale Command Prevention)
    long msgTs = extractJsonInt(pl, "timestamp");
    if (msgTs > 1000000000) { // Check if timestamp looks valid (year > 2001)
      time_t now;
      time(&now);
      if (now > 1000000000) { // Check if system time is synced
        long age = (long)now - msgTs;
        if (age > 60 ||
            age <
                -60) { // Tolerance for clock skew (future stamps ignored too?)
          // If age is huge negative, maybe clock skew or wrong server time.
          // But strict timeout: > 60 seconds old.
          if (age > 60) {
            Serial.printf("[MQTT] Ignored STALE command (Age: %ld s)\n", age);
            return;
          }
        }
      }
    }

    ackCommand(pid);

    if (action == "update_settings") {
      // Stream Enable/Disable
      if (paramsBlock.indexOf("\"stream_enabled\":") != -1) {
        bool enabled = extractJsonBool(paramsBlock, "stream_enabled");
        if (enabled) {
          // Stream enabled pref only
          prefs.begin("datum", false);
          prefs.putBool("pref_stream_en", true);
          prefs.end();
          streaming = true;
          Serial.println("Streaming STARTED via property update");
        } else {
          // Stream disabled
          prefs.begin("datum", false);
          prefs.putBool("pref_stream_en", false);
          prefs.end();
          streaming = false; // Stop active stream loop if any
          Serial.println("Streaming STOPPED via property update");
        }
      }

      // Stream Resolution
      String vres = extractJsonVal(paramsBlock, "vres");
      if (vres.length() == 0)
        vres = extractJsonVal(paramsBlock, "stream_resolution");

      // Snapshot Resolution (Save only)
      String snapRes = extractJsonVal(paramsBlock, "snapshot_resolution");
      if (snapRes.length() > 0) {
        prefs.begin("datum", false);
        prefs.putString("pref_snap_res", snapRes);
        prefs.end();
      }

      String ires = extractJsonVal(paramsBlock, "ires");
      if (ires.length() > 0) {
        prefs.begin("datum", false);
        prefs.putString("pref_ires", ires);
        prefs.end();
      }

      // LED Color
      String color = extractJsonVal(paramsBlock, "led_color");
      if (color.length() > 0) {
        // Assuming 'status' is an existing object or global variable
        // status.color = color;
        // setLEDColor(color); // Assuming setLEDColor is defined elsewhere
      }

      String motSens = extractJsonVal(paramsBlock, "motion_sensitivity");
      if (motSens.length() > 0) {
        prefs.begin("datum", false);
        prefs.putInt("pref_motion_sens", motSens.toInt());
        prefs.end();
      }
      String lcol = extractJsonVal(paramsBlock, "lcol");
      if (lcol.length() == 0)
        lcol = extractJsonVal(paramsBlock, "led_color");
      if (lcol.length() > 0) {
        if (lcol.startsWith("#")) {
          long number = strtol(&lcol.c_str()[1], NULL, 16);
          savedR = number >> 16;
          savedG = number >> 8 & 0xFF;
          savedB = number & 0xFF;
          prefs.begin("datum", false);
          prefs.putString("pref_lcol", lcol);
          prefs.end();
        }
      }

      // LED Brightness
      int lbri = extractJsonInt(paramsBlock, "lbri");
      if (lbri == -1)
        lbri = extractJsonInt(paramsBlock, "led_brightness");
      if (lbri != -1) {
        savedBrightness = lbri;
        prefs.begin("datum", false);
        prefs.putInt("pref_lbri", savedBrightness);
        prefs.end();
      }

      // LED Power
      bool ledChanged = false;
      if (paramsBlock.indexOf("\"led\":") != -1) {
        torchState = extractJsonBool(paramsBlock, "led");
        ledChanged = true;
      } else if (paramsBlock.indexOf("\"led_on\":") != -1) {
        torchState = extractJsonBool(paramsBlock, "led_on");
        ledChanged = true;
      }

      // Persist LED state to survive reboots
      if (ledChanged) {
        prefs.begin("datum", false);
        prefs.putBool("pref_led_on", torchState);
        prefs.end();
        Serial.printf("[MQTT] LED State saved: %s\n",
                      torchState ? "ON" : "OFF");
      }

// Apply LED
#ifdef LED_GPIO_NUM
      int r = 0, g = 0, b = 0;
      if (torchState) {
        r = (savedR * savedBrightness) / 100;
        g = (savedG * savedBrightness) / 100;
        b = (savedB * savedBrightness) / 100;
      }
#if LED_GPIO_NUM == 48
      neopixelWrite(LED_GPIO_NUM, r, g, b);
#else
      digitalWrite(LED_GPIO_NUM, (r + g + b > 0) ? HIGH : LOW);
#endif
#endif

      // Motion
      if (paramsBlock.indexOf("\"mot\":") != -1) {
        motionEnabled = extractJsonBool(paramsBlock, "mot");
        prefs.begin("datum", false);
        prefs.putBool("pref_mot", motionEnabled);
        prefs.end();
      } else if (paramsBlock.indexOf("\"motion_enabled\":") != -1) {
        motionEnabled = extractJsonBool(paramsBlock, "motion_enabled");
        prefs.begin("datum", false);
        prefs.putBool("pref_mot", motionEnabled);
        prefs.end();
      }

      int msens = extractJsonInt(paramsBlock, "msens");
      if (msens != -1) {
        motionThreshold = map(msens, 0, 100, 60, 5);
        float areaMin = 1.0;
        float areaMax = 20.0;
        motionMinAreaPct =
            areaMax - ((float)msens / 100.0 * (areaMax - areaMin));
        prefs.begin("datum", false);
        prefs.putInt("pref_msens", msens);
        prefs.end();
      }

      int mper = extractJsonInt(paramsBlock, "mper");
      if (mper != -1) {
        motionPeriodMs = mper * 1000;
        if (motionPeriodMs < 500)
          motionPeriodMs = 500;
        prefs.begin("datum", false);
        prefs.putInt("pref_mper", mper);
        prefs.end();
      }

      // Orientation
      bool hasMirror = false, mirrorVal = false;
      if (paramsBlock.indexOf("\"imir\":") != -1) {
        hasMirror = true;
        mirrorVal = extractJsonBool(paramsBlock, "imir");
      } else if (paramsBlock.indexOf("\"hmirror\":") != -1) {
        hasMirror = true;
        mirrorVal = extractJsonBool(paramsBlock, "hmirror");
      }

      bool hasFlip = false, flipVal = false;
      if (paramsBlock.indexOf("\"iflip\":") != -1) {
        hasFlip = true;
        flipVal = extractJsonBool(paramsBlock, "iflip");
      } else if (paramsBlock.indexOf("\"vflip\":") != -1) {
        hasFlip = true;
        flipVal = extractJsonBool(paramsBlock, "vflip");
      }

      sensor_t *s = esp_camera_sensor_get();
      if (s) {
        if (vres.length() > 0) {
          framesize_t newSize = getFrameSizeFromName(vres);
          if (s->status.framesize != newSize) {
            // Mutex Lock for Sensor Change (Using extern from camera_manager.h)
            // cameraMutex is extern from camera_manager.h
            // Wait, cameraMutex calls in camera_manager.cpp usage are internal
            // or exposed? camera_manager.h has `extern SemaphoreHandle_t
            // cameraMutex;` So we can use it!

            if (cameraMutex != NULL)
              xSemaphoreTake(cameraMutex, portMAX_DELAY);

            bool wasStreaming = streaming;
            streaming = false;
            delay(100);
            s->set_framesize(s, newSize);
            streaming = wasStreaming;

            prefs.begin("datum", false);
            prefs.putString("pref_vres", vres);
            prefs.end();
            if (cameraMutex != NULL)
              xSemaphoreGive(cameraMutex);
          }
        }
        if (hasMirror) {
          Serial.printf("[MQTT] Set Mirror: %d\n", mirrorVal);
          s->set_hmirror(s, mirrorVal ? 1 : 0);
          // Manually update status in case driver doesn't
          s->status.hmirror = mirrorVal ? 1 : 0;

          ignoreMotionFor(10); // Suppress motion detection

          prefs.begin("datum", false);
          prefs.putBool("pref_imir", mirrorVal);
          prefs.end();
        }
        if (hasFlip) {
          Serial.printf("[MQTT] Set Flip: %d\n", flipVal);
          s->set_vflip(s, flipVal ? 1 : 0);
          // Manually update status in case driver doesn't
          s->status.vflip = flipVal ? 1 : 0;

          ignoreMotionFor(10); // Suppress motion detection

          prefs.begin("datum", false);
          prefs.putBool("pref_iflip", flipVal);
          prefs.end();
        }
      }
      reportTelemetry(false, false); // Sync state immediately

    } else if (action == "stream") {
      String state = extractJsonVal(paramsBlock, "state");
      streaming = (state == "on");
      Serial.println(streaming ? "Streaming STARTED via MQTT"
                               : "Streaming STOPPED via MQTT");

    } else if (action == "snap") {
      String snapRes = extractJsonVal(paramsBlock, "resolution");
      if (snapRes.length() == 0) {
        prefs.begin("datum", true);
        snapRes = prefs.getString("pref_snap_res", "SVGA"); // safe default
        prefs.end();
      }
      handleSnap(snapRes);

    } else if (action == "restart") {
      ESP.restart();

    } else if (action == "led") {
#ifdef LED_GPIO_NUM
#if LED_GPIO_NUM == 48
      torchState = !torchState;
      neopixelWrite(LED_GPIO_NUM, torchState ? 255 : 0, torchState ? 255 : 0,
                    torchState ? 255 : 0);
#endif
#endif
    } else if (action == "update_firmware" || action == "update") {
      String url = extractJsonVal(paramsBlock, "url");
      // If URL is empty, try to construct it from version via server?
      // For now, assume URL is passed.
      if (url.length() > 0) {
        ackCommand(pid); // Ack before update (it will reboot)
        delay(500);
        updateFirmware(url);
      } else {
        Serial.println("[MQTT] Update command missing URL");
      }
    }
  }
}

String getMQTTHost() {
  int start = serverURL.indexOf("://");
  if (start == -1)
    start = 0;
  else
    start += 3;
  int end = serverURL.indexOf("/", start);
  String host = (end == -1) ? serverURL.substring(start)
                            : serverURL.substring(start, end);
  int portIdx = host.indexOf(":");
  if (portIdx != -1)
    host = host.substring(0, portIdx);
  return host;
}

void setupMQTT() {
  mqttHost = getMQTTHost();
  Serial.println("MQTT Host: " + mqttHost);
  espClient.setTimeout(10000);
  mqttClient.setServer(mqttHost.c_str(), 1883);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024);
  mqttClient.setKeepAlive(60);
}

bool reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED)
    return false;

  // Sanitize credentials
  deviceID.trim();
  apiKey.trim();
  deviceUID.trim();

  String clientId = deviceID; // Git version used deviceID not deviceUID?

  // Debug Credentials (Masked)
  Serial.println("[MQTT] Connecting...");
  Serial.println("  ID: " + clientId);
  Serial.println("  User: " + deviceID); // Git version used deviceID as user
  // Mask API Key
  String maskedKey =
      (apiKey.length() > 4) ? apiKey.substring(0, 4) + "****" : "N/A";
  Serial.println("  Pass: " + maskedKey);

  // Authenticate as User=deviceID, Pass=apiKey
  if (mqttClient.connect(clientId.c_str(), deviceID.c_str(), apiKey.c_str())) {
    Serial.println("[MQTT] Connected");

    // Subscribe to cmd/{id}
    String topic = "dev/" + deviceID + "/cmd";
    mqttClient.subscribe(topic.c_str());
    Serial.println("[MQTT] Subscribed to " + topic);

    // Telemetry on connect
    reportTelemetry(false, true);
    sendThingDescription(); // Send WoT Description
    return true;
  } else {
    Serial.print("[MQTT] Failed, rc=");
    Serial.print(mqttClient.state());
    Serial.println(" try again in 5s");
    return false;
  }
}

void processMqttLoop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      unsigned long now = millis();
      static unsigned long lastReconnectAttempt = 0;
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        if (reconnectMQTT()) {
          lastReconnectAttempt = 0;
        }
      }
    } else {
      mqttClient.loop();

      // Periodic Telemetry (Heartbeat)
      static unsigned long lastTelemetry = 0;
      unsigned long now = millis();
      if (now - lastTelemetry > 60000) { // 60 Seconds
        lastTelemetry = now;             // Reset timer
        reportTelemetry(false, false);
      }
    }
  }
}

String getResetReasonString() {
  esp_reset_reason_t reason = esp_reset_reason();
  switch (reason) {
  case ESP_RST_POWERON:
    return "Power On";
  case ESP_RST_SW:
    return "Software Reset";
  case ESP_RST_PANIC:
    return "Exception/Panic";
  case ESP_RST_INT_WDT:
    return "Watchdog (Interrupt)";
  case ESP_RST_TASK_WDT:
    return "Watchdog (Task)";
  case ESP_RST_WDT:
    return "Watchdog (Other)";
  case ESP_RST_DEEPSLEEP:
    return "Deep Sleep";
  case ESP_RST_BROWNOUT:
    return "Brownout";
  case ESP_RST_SDIO:
    return "SDIO";
  default:
    return "Unknown";
  }
}

void reportTelemetry(bool isBoot, bool isConnect) {
  if (deviceID.length() == 0 || apiKey.length() == 0)
    return;
  if (!WiFi.isConnected())
    return;

  String json = "{";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"status\":\"online\"";

  if (isConnect) {
    json += ",";
    json += "\"local_ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"ssid\":\"" + WiFi.SSID() + "\",";
    json += "\"bssid\":\"" + WiFi.BSSIDstr() + "\",";
    json += "\"channel\":" + String(WiFi.channel()) + "";
  }

  json += ",";
  if (isBoot) {
    json += "\"fw_ver\":\"" + String(FIRMWARE_VERSION) + "\",";
    json += "\"reset_reason\":\"" + getResetReasonString() + "\",";
  }

  char hexColor[8];
  sprintf(hexColor, "#%02X%02X%02X", savedR, savedG, savedB);
  json += "\"led_on\":" + String(torchState ? "true" : "false") + "," +
          "\"led_brightness\":" + String(savedBrightness) + "," +
          "\"led_color\":\"" + String(hexColor) + "\",";

  prefs.begin("datum", true);
  int sens = prefs.getInt("pref_motion_sens", 50); // Default 50?
  prefs.end();

  json += "\"motion_enabled\":" + String(motionEnabled ? "true" : "false") +
          "," + "\"motion_sensitivity\":" + String(sens) + ",";

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    json += "\"stream_resolution\":\"" + getFrameSizeName(s->status.framesize) +
            "\",";
    // Get Snap Res from Prefs (or default to Stream?)
    String snapRes = prefs.getString("pref_snap_res", "SVGA");
    json += "\"snapshot_resolution\":\"" + snapRes + "\",";

    json += "\"hmirror\":" + String(s->status.hmirror ? "true" : "false") + ",";
    json += "\"vflip\":" + String(s->status.vflip ? "true" : "false") + ",";
    json += "\"stream_enabled\":" + String(streaming ? "true" : "false");
  } else {
    json += "\"stream_resolution\":\"VGA\",\"snapshot_resolution\":\"SVGA\","
            "\"hmirror\":false,\"vflip\":false";
  }

  String pubIP = getPublicIP();
  if (pubIP.length() == 0) {
    pubIP =
        WiFi.localIP().toString(); // Fallback to local IP if public check fails
  }
  json += ",\"public_ip\":\"" + pubIP + "\"";
  json += "}";

  String topic = "dev/" + deviceID + "/data";
  // Ensure topic is clean
  topic.trim();

  if (mqttClient.connected()) {
    Serial.print("[MQTT]<- ");
    Serial.println(json);

    if (mqttClient.publish(topic.c_str(), json.c_str())) {
      Serial.println("[MQTT] Publish OK to " + topic);
    } else {
      Serial.println("[MQTT] Publish FAILED! Check buffer/connection");
    }
  } else {
    Serial.println("MQTT Not Connected! Telemetry skipped.");
  }
}

void sendThingDescription() {
  if (deviceID.length() == 0 || apiKey.length() == 0)
    return;

  HTTPClient http;
  http.begin(serverURL + "/dev/" + deviceID + "/thing-description");
  http.addHeader("Authorization", "Bearer " + apiKey);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(4096);
  doc["@context"] = "https://www.w3.org/2019/wot/td/v1";
  doc["id"] = "urn:dev:datum:" + deviceID;
  doc["title"] = deviceName.length() > 0 ? deviceName : "ESP32-S3 Camera";
  doc["description"] = "Smart AI-capable Camera with Motion Detection";

  // -- Properties --
  JsonObject props = doc.createNestedObject("properties");

  // Read-Write Properties
  JsonObject pRes = props.createNestedObject("stream_resolution");
  pRes["title"] = "Stream Resolution";
  pRes["type"] = "string";
  pRes["enum"].add("QXGA");
  pRes["enum"].add("UXGA");
  pRes["enum"].add("SXGA");
  pRes["enum"].add("XGA");
  pRes["enum"].add("SVGA");
  pRes["enum"].add("VGA");
  pRes["enum"].add("CIF");
  pRes["enum"].add("QVGA");
  pRes["enum"].add("HQVGA");
  pRes["enum"].add("QQVGA");
  pRes["readOnly"] = false;

  JsonObject pStream = props.createNestedObject("stream_enabled");
  pStream["title"] = "Stream Active";
  pStream["type"] = "boolean";
  pStream["ui:widget"] = "switch";
  pStream["readOnly"] = false;

  JsonObject pSnapRes = props.createNestedObject("snapshot_resolution");
  pSnapRes["title"] = "Snapshot Resolution";
  pSnapRes["type"] = "string";
  pSnapRes["enum"].add("QXGA");
  pSnapRes["enum"].add("UXGA");
  pSnapRes["enum"].add("SXGA");
  pSnapRes["enum"].add("XGA");
  pSnapRes["enum"].add("SVGA");
  pSnapRes["enum"].add("VGA");
  pSnapRes["enum"].add("CIF");
  pSnapRes["enum"].add("QVGA");
  pSnapRes["enum"].add("HQVGA");
  pSnapRes["enum"].add("QQVGA");
  pSnapRes["readOnly"] = false;

  JsonObject pLed = props.createNestedObject("led_on");
  pLed["title"] = "LED Flash";
  pLed["type"] = "boolean";
  pLed["ui:widget"] = "switch";
  pLed["readOnly"] = false;

  JsonObject pBri = props.createNestedObject("led_brightness");
  pBri["title"] = "LED Brightness";
  pBri["type"] = "integer";
  pBri["minimum"] = 0;
  pBri["maximum"] = 100;
  pBri["unit"] = "%";
  pBri["ui:widget"] = "slider";
  pBri["readOnly"] = false;

  JsonObject pMot = props.createNestedObject("motion_enabled");
  pMot["title"] = "Motion Detection";
  pMot["type"] = "boolean";
  pMot["ui:widget"] = "switch";
  pMot["readOnly"] = false;

  JsonObject pMotSen = props.createNestedObject("motion_sensitivity");
  pMotSen["title"] = "Motion Sensitivity";
  pMotSen["type"] = "integer";
  pMotSen["minimum"] = 0;
  pMotSen["maximum"] = 100;
  pMotSen["unit"] = "%";
  pMotSen["ui:widget"] = "slider";
  pMotSen["readOnly"] = false;

  JsonObject pCol = props.createNestedObject("led_color");
  pCol["title"] = "LED Color";
  pCol["type"] = "string";
  pCol["ui:widget"] = "color";
  pCol["readOnly"] = false;

  JsonObject pHm = props.createNestedObject("hmirror");
  pHm["title"] = "Horizontal Mirror";
  pHm["type"] = "boolean";
  pHm["readOnly"] = false;

  JsonObject pVf = props.createNestedObject("vflip");
  pVf["title"] = "Vertical Flip";
  pVf["type"] = "boolean";
  pVf["readOnly"] = false;

  // Read-Only
  JsonObject pRssi = props.createNestedObject("rssi");
  pRssi["title"] = "WiFi Signal";
  pRssi["type"] = "integer";
  pRssi["unit"] = "dBm";
  pRssi["readOnly"] = true;

  JsonObject pUp = props.createNestedObject("uptime");
  pUp["title"] = "Uptime";
  pUp["type"] = "integer";
  pUp["unit"] = "s";
  pUp["readOnly"] = true;

  JsonObject pIp = props.createNestedObject("public_ip");
  pIp["title"] = "Public IP";
  pIp["type"] = "string";
  pIp["readOnly"] = true;

  // -- Actions --
  JsonObject actions = doc.createNestedObject("actions");

  JsonObject aSnap = actions.createNestedObject("snap");
  aSnap["title"] = "Take Snapshot";
  aSnap["description"] = "Captures a still image (uses configured resolution)";

  // Stream action deprecated in favor of stream_enabled property,
  // but kept for compatibility/explicit trigger if needed.
  JsonObject aStream = actions.createNestedObject("stream");
  aStream["title"] = "Start Stream";
  aStream["description"] =
      "Starts the video stream (uses configured resolution)";

  JsonObject aUpdate = actions.createNestedObject("update_firmware");
  aUpdate["title"] = "Update Firmware";
  JsonObject aUpdateIn = aUpdate.createNestedObject("input");
  aUpdateIn["type"] = "object";
  aUpdateIn["properties"]["url"]["type"] = "string";
  aUpdateIn["properties"]["url"]["title"] = "Firmware URL";

  JsonObject aRestart = actions.createNestedObject("restart");
  aRestart["title"] = "Restart Device";

  String payload;
  serializeJson(doc, payload);

  int code = http.PUT(payload);
  Serial.printf("[TD] Upload Code: %d\n", code);
  http.end();
}

void publishMotionEvent() {
  if (deviceID.length() == 0 || !mqttClient.connected())
    return;

  DynamicJsonDocument doc(512);
  doc["type"] = "event";
  doc["event"] = "motion";
  doc["timestamp"] = time(NULL);
  doc["description"] = "Motion detected!";

  String payload;
  serializeJson(doc, payload);

  String topic = "dev/" + deviceID + "/data";
  mqttClient.publish(topic.c_str(), payload.c_str());
  Serial.println("[MQTT] Published Motion Event");
}
