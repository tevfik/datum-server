#include "DatumIoT.h"

DatumIoT::DatumIoT(Client &netClient)
    : _netClient(&netClient), _mqtt(netClient) {
  _serverUrl = "https://datum.bezg.in";
  _mqttHost = "datum.bezg.in";
  _mqtt.setBufferSize(2048); // Large buffer for commands
}

// Initial Setup (Manual)
void DatumIoT::begin(String serverParams, String deviceId, String apiKey) {
  _serverUrl = serverParams;
  _deviceId = deviceId;
  _apiKey = apiKey;

  // Parse Host from URL
  int start = _serverUrl.indexOf("://");
  if (start == -1)
    start = 0;
  else
    start += 3;
  int end = _serverUrl.indexOf("/", start);
  _mqttHost = (end == -1) ? _serverUrl.substring(start)
                          : _serverUrl.substring(start, end);
  int portIdx = _mqttHost.indexOf(":");
  if (portIdx != -1)
    _mqttHost = _mqttHost.substring(0, portIdx);

  _mqtt.setServer(_mqttHost.c_str(), _mqttPort);
  _mqtt.setCallback([this](char *topic, byte *payload, unsigned int length) {
    this->_mqttCallback(topic, payload, length);
  });
}

void DatumIoT::setServer(String url) {
  _serverUrl = url;
  // Recalculate host
  int start = _serverUrl.indexOf("://");
  if (start == -1)
    start = 0;
  else
    start += 3;
  int end = _serverUrl.indexOf("/", start);
  _mqttHost = (end == -1) ? _serverUrl.substring(start)
                          : _serverUrl.substring(start, end);
  int portIdx = _mqttHost.indexOf(":");
  if (portIdx != -1)
    _mqttHost = _mqttHost.substring(0, portIdx);

  _mqtt.setServer(_mqttHost.c_str(), _mqttPort);
}

bool DatumIoT::connect() {
  if (_mqtt.connected())
    return true;
  return _mqttConnect();
}

bool DatumIoT::_mqttConnect() {
  if (_deviceId.length() == 0 || _apiKey.length() == 0) {
    DATUM_LOG("Missing credentials!");
    return false;
  }

  String clientId = _deviceId; // + "_" + String(random(0xffff), HEX); ? No,
                               // server expects DeviceID as ID?
  // Based on example: ClientID = deviceID, User = deviceID, Pass = apiKey

  DATUM_LOG("Connecting MQTT to %s...", _mqttHost.c_str());

  if (_mqtt.connect(clientId.c_str(), _deviceId.c_str(), _apiKey.c_str())) {
    DATUM_LOG("MQTT Connected!");

    String topic = "dev/" + _deviceId + "/cmd";
    _mqtt.subscribe(topic.c_str());
    DATUM_LOG("Subscribed: %s", topic.c_str());
    return true;
  } else {
    DATUM_LOG("MQTT Fail: rc=%d", _mqtt.state());
    return false;
  }
}

void DatumIoT::loop() {
  if (!_mqtt.connected()) {
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > 5000) {
      lastReconnect = millis();
      connect();
    }
  } else {
    _mqtt.loop();
  }
}

// Provisioning
bool DatumIoT::registerDevice(String serverBaseUrl, String userToken,
                              String deviceName, String deviceType,
                              String deviceUid) {
  setServer(serverBaseUrl);

  if (deviceUid.length() == 0) {
    // Generate pseudo-UID from MAC if not provided
#ifdef ESP8266
    uint32_t chipid = ESP.getChipId();
    char uid[13];
    sprintf(uid, "%08X", chipid);
    deviceUid = String(uid);
#else
    uint64_t chipid = ESP.getEfuseMac();
    char uid[13];
    sprintf(uid, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
    deviceUid = String(uid);
#endif
  }

  HTTPClient http;
  String url = _serverUrl + "/dev";
  DATUM_LOG("Registering at %s...", url.c_str());

#ifdef ESP8266
  WiFiClient client;
  http.begin(client, url);
#else
  http.begin(url);
#endif
  http.addHeader("Content-Type", "application/json");
  // http.addHeader("Authorization", "Bearer " + userToken); // Usually Bearer?
  // Example code used: "Authorization: Bearer " + userToken (line 449
  // esp32s3...ino)
  http.addHeader("Authorization", "Bearer " + userToken);

  StaticJsonDocument<256> doc;
  doc["device_id"] = deviceUid; // Map deviceUid to device_id
  doc["name"] = deviceName;     // Map device_name to name
  doc["type"] = deviceType;     // Map device_type to type

  String payload;
  serializeJson(doc, payload);

  int code = http.POST(payload);
  String resp = http.getString();
  http.end();

  if (code == 200 || code == 201) {
    StaticJsonDocument<512> respDoc;
    deserializeJson(respDoc, resp);

    String did = respDoc["device_id"].as<String>();
    String key = respDoc["api_key"].as<String>();

    if (did.length() > 0 && key.length() > 0) {
      _deviceId = did;
      _apiKey = key;
      DATUM_LOG("Registration Success! ID: %s", _deviceId.c_str());
      return true;
    }
  }

  DATUM_LOG("Registration Failed: %d %s", code, resp.c_str());
  return false;
}

// Telemetry
bool DatumIoT::sendTelemetry(String key, float value) {
  StaticJsonDocument<128> doc;
  doc[key] = value;
  return sendTelemetryJson(doc.as<JsonObject>());
}

bool DatumIoT::sendTelemetry(String key, String value) {
  StaticJsonDocument<128> doc;
  doc[key] = value;
  return sendTelemetryJson(doc.as<JsonObject>());
}

bool DatumIoT::sendTelemetry(String key, bool value) {
  StaticJsonDocument<128> doc;
  doc[key] = value;
  return sendTelemetryJson(doc.as<JsonObject>());
}

bool DatumIoT::sendTelemetryJson(JsonObject json) {
  if (!_mqtt.connected())
    return false;

  String topic = "dev/" + _deviceId + "/data";
  String payload;
  serializeJson(json, payload);

  DATUM_LOG("Sending Telemetry: %s", payload.c_str());
  return _mqtt.publish(topic.c_str(), payload.c_str());
}

// Callbacks
void DatumIoT::onCommand(CommandCallback cb) { _cmdCallback = cb; }

void DatumIoT::_mqttCallback(char *topic, byte *payload, unsigned int length) {
  String pl;
  for (unsigned int i = 0; i < length; i++)
    pl += (char)payload[i];

  DATUM_LOG("CMD Recv: %s", pl.c_str());

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, pl);
  if (error) {
    DATUM_LOG("JSON Parse Error");
    return;
  }

  String cmdId = doc["id"] | doc["command_id"] | "";
  String action = doc["action"] | "";
  JsonObject params = doc["params"];

  if (cmdId.length() > 0) {
    _ackCommand(cmdId); // Auto-ACK

    if (_cmdCallback) {
      _cmdCallback(cmdId, action, params);
    }
  }
}

void DatumIoT::_ackCommand(String cmdId) {
  // Ack via HTTP (as per original logic, though MQTT Pub to ack topic would be
  // better if server supports it) Code says: POST /dev/:id/cmd/:id/ack
  HTTPClient http;
  WiFiClient client;
  String url = _serverUrl + "/dev/" + _deviceId + "/cmd/" + cmdId + "/ack";

#ifdef ESP8266
  http.begin(client, url);
#else
  http.begin(url);
#endif
  http.addHeader("Authorization", "Bearer " + _apiKey);
  http.addHeader("Content-Type", "application/json");
  http.POST("{\"status\":\"executed\"}");
  http.end();
}

bool DatumIoT::isConnected() { return _mqtt.connected(); }
