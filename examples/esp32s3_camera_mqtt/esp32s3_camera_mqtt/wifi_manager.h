#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

// Globals to extern (Credentials)
extern String wifiSSID;
extern String wifiPass;
extern const char *AP_SSID;

// Functions
void setupWiFi();                                    // Initial setup
bool connectToWiFiBlocking(int timeoutSeconds = 20); // Startup blocking connect
void handleWiFiLoop(); // Loop checker for reconnection
void startAPMode();    // If connection fails
String getIPAddress();
int getRSSI();

#endif
