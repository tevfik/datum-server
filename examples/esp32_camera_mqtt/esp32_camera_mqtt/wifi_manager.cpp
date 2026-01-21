#include "wifi_manager.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Extern helpers
extern void updateLED();
extern void startSetupMode(); // If we need to fallback to AP
extern bool initialBoot;      // in .ino
extern String serverURL;      // in .ino

unsigned long lastWiFiCheck = 0;
bool wasConnected = false;

// Blocking connect for startup
bool connectToWiFiBlocking(int timeoutSeconds, void (*onLoopCallback)()) {
  if (wifiSSID.length() == 0)
    return false;

  Serial.printf("Connecting to %s\n", wifiSSID.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);         // Enable Modem Sleep for Power Saving
  WiFi.setAutoReconnect(true); // Ensure auto-reconnect is on

  // Explicit begin
  WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());

  int attempts = 0;
  // Convert seconds to 50ms intervals for responsive button
  int maxAttempts = timeoutSeconds * 20;

  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    if (attempts % 10 == 0)
      Serial.print("."); // Print dot every 500ms

    // Call the callback (Button Handler)
    if (onLoopCallback)
      onLoopCallback();

    updateLED();
    delay(50); // Faster polling
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());
    wasConnected = true;
    updatePublicIP();
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
      updatePublicIP();
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

String publicIP = "";

void updatePublicIP() {
  if (WiFi.status() == WL_CONNECTED) {
    if (publicIP.length() > 0)
      return; // Keep existing if valid

    Serial.println("[WiFi] Querying Public IP...");
    // WiFiClientSecure client; // Not needed if serverURL handles protocol, but
    // wait HTTPClient needs begin(url). If serverURL is https, we need
    // ClientSecure or validation? HTTPClient handles https if given a url, but
    // might need setInsecure if cert is self-signed? The previous code used
    // api.ipify.org (https). Let's assume serverURL might be http or https.

    // We already have extern String serverURL; in wifi_manager.h? No, in
    // mqtt_manager.h. wifi_manager.cpp doesn't include mqtt_manager.h but
    // accesses globals? Wait, wifi_manager.cpp does not see serverURL extern!
    // serverURL is defined in main.ino.
    // I need to check if 'serverURL' is visible here.
    // In wifi_manager.cpp lines 1-10, there is NO extern String serverURL.
    // I MUST ADD extern String serverURL here first.

    HTTPClient http;
    http.setTimeout(5000);
    // Use the serverURL global
    String url = serverURL + "/sys/ip";

    // Check if https
    if (url.startsWith("https")) {
      WiFiClientSecure client;
      client.setInsecure();
      http.begin(client, url);
      int httpCode = http.GET();
      if (httpCode > 0) {
        String payload = http.getString();
        payload.trim();
        if (payload.length() > 0) {
          publicIP = payload;
          Serial.println("[WiFi] Public IP: " + publicIP);
        }
      }
      http.end();
    } else {
      http.begin(url);
      int httpCode = http.GET();
      if (httpCode > 0) {
        String payload = http.getString();
        payload.trim();
        if (payload.length() > 0) {
          publicIP = payload;
          Serial.println("[WiFi] Public IP: " + publicIP);
        }
      }
      http.end();
    }
  }
}

String getPublicIP() { return publicIP; }
