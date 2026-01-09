#include "mqtt_manager.h"
#include "camera_manager.h" // For frame size, snap, streaming constants
#include "eif_motion.h"     // For motion settings
#include <HTTPClient.h>     // For ackCommand
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
#define FIRMWARE_VERSION                                                       \
  "2.1.0" // Duplication risk but okay for refactor step 1

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

#define CAMERA_MODEL_FREENOVE_S3

#if defined(CAMERA_MODEL_FREENOVE_S3)
#define LED_GPIO_NUM 48
#elif defined(CAMERA_MODEL_AI_THINKER)
#define LED_GPIO_NUM 4
#else
#define LED_GPIO_NUM 21 // Fallback
#endif

// Forward declare neopixelWrite (Main has the implementation usually or
// standard?)
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
  http.begin(serverURL + "/devices/" + deviceID + "/commands/" + cmdId +
             "/ack");
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
    ackCommand(pid);

    if (action == "update_settings") {
      // Resolution
      String vres = extractJsonVal(paramsBlock, "vres");
      if (vres.length() == 0)
        vres = extractJsonVal(paramsBlock, "resolution");

      String ires = extractJsonVal(paramsBlock, "ires");
      if (ires.length() > 0) {
        prefs.begin("datum", false);
        prefs.putString("pref_ires", ires);
        prefs.end();
      }

      // LED Color
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
      if (paramsBlock.indexOf("\"led\":") != -1) {
        torchState = extractJsonBool(paramsBlock, "led");
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

    } else if (action == "stream") {
      String state = extractJsonVal(paramsBlock, "state");
      streaming = (state == "on");
      Serial.println(streaming ? "Streaming STARTED via MQTT"
                               : "Streaming STOPPED via MQTT");

    } else if (action == "snap") {
      String snapRes = extractJsonVal(paramsBlock, "resolution");
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
    String topic = "cmd/" + deviceID;
    mqttClient.subscribe(topic.c_str());
    Serial.println("[MQTT] Subscribed to " + topic);

    // Telemetry on connect
    reportTelemetry(false, true);
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
  json += "\"led_color\":\"" + String(hexColor) + "\",";
  json += "\"led_brightness\":" + String(savedBrightness) + ",";
  json += "\"led_on\":" + String(torchState ? "true" : "false") + ",";

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    json += "\"resolution\":\"" + getFrameSizeName(s->status.framesize) + "\",";
    json += "\"hmirror\":" + String(s->status.hmirror ? "true" : "false") + ",";
    json += "\"vflip\":" + String(s->status.vflip ? "true" : "false");
  } else {
    json += "\"resolution\":\"VGA\",\"hmirror\":false,\"vflip\":false";
  }

  json += "}";

  String topic = "data/" + deviceID;
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
