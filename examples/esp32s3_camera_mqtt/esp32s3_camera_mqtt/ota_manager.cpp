#include "ota_manager.h"
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFi.h>

// Extern API Key from Main/Global
extern String apiKey;

void updateFirmware(String url) {
  if (url.length() == 0)
    return;

  Serial.println("Starting OTA Update...");
  Serial.print("Firmware URL: ");
  Serial.println(url);

  // Append auth token if needed
  if (url.indexOf("token=") == -1 && apiKey.length() > 0) {
    if (url.indexOf('?') == -1) {
      url += "?token=" + apiKey;
    } else {
      url += "&token=" + apiKey;
    }
  }

  // NOTE: esp_camera_deinit() is usually needed if camera uses PSRAM heavily,
  // but we can't access it here easily without including esp_camera.h.
  // Assuming camera stops or WDT handles it.
  // If needed, include "esp_camera.h" and call esp_camera_deinit().

  WiFiClient client;
  client.setTimeout(60);

  // Add progress callback
  httpUpdate.onProgress([](int cur, int total) {
    Serial.printf("OTA Progress: %d%%\n", (cur * 100) / total);
  });

  t_httpUpdate_return ret = httpUpdate.update(client, url);

  switch (ret) {
  case HTTP_UPDATE_FAILED:
    Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n",
                  httpUpdate.getLastError(),
                  httpUpdate.getLastErrorString().c_str());
    // Reboot to recover state
    ESP.restart();
    break;

  case HTTP_UPDATE_NO_UPDATES:
    Serial.println("HTTP_UPDATE_NO_UPDATES");
    break;

  case HTTP_UPDATE_OK:
    Serial.println("HTTP_UPDATE_OK");
    // Auto-restarts on success
    break;
  }
}
