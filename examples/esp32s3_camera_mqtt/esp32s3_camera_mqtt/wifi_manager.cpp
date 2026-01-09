#include "wifi_manager.h"

// Extern helpers
extern void updateLED();
extern void startSetupMode(); // If we need to fallback to AP
extern bool initialBoot;      // in .ino

unsigned long lastWiFiCheck = 0;
bool wasConnected = false;

// Blocking connect for startup
bool connectToWiFiBlocking(int timeoutSeconds) {
  if (wifiSSID.length() == 0)
    return false;

  Serial.printf("Connecting to %s\n", wifiSSID.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true); // Ensure auto-reconnect is on

  // Explicit begin
  WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());

  int attempts = 0;
  // Convert seconds to 500ms intervals
  int maxAttempts = timeoutSeconds * 2;

  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    Serial.print(".");
    delay(500);
    updateLED(); // Keep LED logic alive
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());
    wasConnected = true;
    return true;
  }

  Serial.println("WiFi Connection Failed.");
  return false;
}

void startAPMode() {
  Serial.println("Starting AP Mode...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  Serial.print("AP Started: ");
  Serial.println(AP_SSID);
  Serial.println(WiFi.softAPIP());
}

void handleWiFiLoop() {
  unsigned long now = millis();

  // Check connection status
  if (WiFi.status() == WL_CONNECTED) {
    if (!wasConnected) {
      Serial.println("[WiFi] Reconnected!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      wasConnected = true;
    }
  } else {
    // Disconnected
    if (wasConnected) {
      Serial.println("[WiFi] Lost Connection!");
      wasConnected = false;
    }

    // Try to reconnect every 10 seconds
    if (now - lastWiFiCheck > 10000) {
      lastWiFiCheck = now;
      Serial.println(
          "[WiFi] Status: Disconnected. Attempting fresh connection...");

      // Force disconnect first to clear stuck states
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);
      delay(100);
      WiFi.mode(WIFI_STA);

      // Explicit begin is often more reliable than reconnect()
      WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
    }
  }
}

String getIPAddress() { return WiFi.localIP().toString(); }

int getRSSI() { return WiFi.RSSI(); }
