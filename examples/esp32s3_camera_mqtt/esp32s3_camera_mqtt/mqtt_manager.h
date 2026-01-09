#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

// Globals
extern PubSubClient mqttClient;
extern String serverURL;
extern String deviceID;
extern String apiKey;
extern String deviceName;
extern String mqttHost;

// Functions
void setupMQTT();
bool connectMQTT(); // or reconnectMQTT
bool reconnectMQTT();
void startMQTTTask(); // If we were to use a task, but sticking to loop() for
                      // now
void processMqttLoop();

void reportTelemetry(bool isBoot, bool isConnect);
void ackCommand(String cmdId);

// JSON Helpers
String extractJsonVal(String json, String key);
int extractJsonInt(String json, String key);
bool extractJsonBool(String json, String key);

// Helper for resetting reason
String getResetReasonString();

#endif
