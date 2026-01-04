#ifndef INCLUDES_H
#define INCLUDES_H

#include <ArduinoJson.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClient.h>

// ======== HARDWARE CONFIG ========
// Relay Pins (Active Low/High depends on board, typically Low for Relay Boards)
#define GPIO_RELAY_0 16
#define GPIO_RELAY_1 5
#define GPIO_RELAY_2 4
#define GPIO_RELAY_3 0 // Be careful, GPIO0 is boot mode pin

// Battery Voltage ADC
#define ADC_BATTERY A0

// ======== FIRMWARE CONFIG ========
#define FIRMWARE_VER "1.0.0"
#define DEVICE_TYPE_NAME "relay_board"
#define DEVICE_FRIENDLY_NAME "ESP8266 Relay"

// ======== STORAGE CONFIG ========
#define EEPROM_SIZE 512
// Address Map
// 0-31: SSID (32 chars)
// 32-95: Pass (64 chars)
// 96-127: API Key (32 chars) - dk_... or empty
// 128-191: Server URL (64 chars) - https://...
// 192-255: User Token (64 chars) - Temporary provisioning token
// 256-287: Device Name (32 chars)

// Helper Struct for EEPROM
struct Config {
  char wifi_ssid[32];
  char wifi_pass[64];
  char api_key[33];
  char server_url[65];
  char user_token[65];
  char device_name[33];
  char device_id[37]; // UUID is 36 chars + null terminator
};

#endif
