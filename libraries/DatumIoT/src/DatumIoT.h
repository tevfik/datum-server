#ifndef DATUM_IOT_H
#define DATUM_IOT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <PubSubClient.h>

// Debug Macros
#define DATUM_DEBUG
#ifdef DATUM_DEBUG
#define DATUM_LOG(...)                                                         \
  Serial.printf("[Datum] " __VA_ARGS__);                                       \
  Serial.println()
#else
#define DATUM_LOG(...)
#endif

// Callback types
typedef std::function<void(String cmdId, String action, JsonObject params)>
    CommandCallback;
typedef std::function<void(String topic, String payload)> MqttCallback;

class DatumIoT {
public:
  DatumIoT(Client &netClient);

  // Initialization
  void begin(String serverParams, String deviceId,
             String apiKey); // Legacy manual init

  // Provisioning
  bool registerDevice(String serverBaseUrl, String userToken, String deviceName,
                      String deviceType, String deviceUid = "");

  // Connection
  void setServer(String url);
  void connectWiFi(String ssid, String pass); // Helper blocking connect
  bool connect();                             // Find MQTT/active connection
  void loop();

  // Telemetry
  bool sendTelemetry(String key, float value);
  bool sendTelemetry(String key, String value);
  bool sendTelemetry(String key, bool value);
  bool sendTelemetryJson(JsonObject json);

  // Commands
  void onCommand(CommandCallback cb);

  // Utils
  String getDeviceId() const { return _deviceId; }
  String getApiKey() const { return _apiKey; }
  bool isConnected();

private:
  Client *_netClient;
  PubSubClient _mqtt;
  String _serverUrl;
  String _deviceId;
  String _apiKey;
  String _mqttHost;
  int _mqttPort = 1883;

  CommandCallback _cmdCallback;

  // Internal Helpers
  bool _mqttConnect();
  void _mqttCallback(char *topic, byte *payload, unsigned int length);
  void _ackCommand(String cmdId);
  void _handleCommand(String payload);
  String _extractJsonVal(String json, String key);
};

#endif
