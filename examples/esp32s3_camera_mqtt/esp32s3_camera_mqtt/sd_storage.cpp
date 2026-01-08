#include "sd_storage.h"
#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>

static bool sdInitialized = false;

bool initSD() {
  if (sdInitialized)
    return true;

  // Configure pins for SD_MMC 1-bit mode
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);

  // Mount SD Card (false = no format on fail, true = 1-bit mode, true = format
  // if failed)
  if (!SD_MMC.begin("/sdcard", true, true)) {
    Serial.println("[SD] Mount Failed! Check card inserted or formatting.");
    return false;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("[SD] No SD Card attached");
    return false;
  }

  Serial.printf("[SD] SD Card Mounted. Size: %lluMB\n",
                SD_MMC.cardSize() / (1024 * 1024));
  sdInitialized = true;

  createSDFolders();
  return true;
}

void createSDFolders() {
  if (!sdInitialized)
    return;
  if (!SD_MMC.exists("/capture")) {
    SD_MMC.mkdir("/capture");
  }
}

bool isSDAvailable() { return sdInitialized; }

bool saveFrameToSD(camera_fb_t *fb, String filename) {
  if (!sdInitialized)
    return false;

  if (filename.length() == 0) {
    // Auto-generate filename based on time (millis if no time)
    // Format: /capture/img_MILLIS.jpg
    // In real usage, you'd want NTP time here if available
    unsigned long now = millis();
    filename = "/capture/img_" + String(now) + ".jpg";
  }

  File file = SD_MMC.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("[SD] Failed to open file for writing: " + filename);
    return false;
  }

  size_t len = fb->len;
  size_t written = file.write(fb->buf, len);
  file.close();

  if (written != len) {
    Serial.printf("[SD] Write failed! Written %d / %d bytes\n", written, len);
    return false;
  }

  Serial.println("[SD] Saved: " + filename);
  return true;
}

String listSDFiles(String path) {
  if (!sdInitialized)
    return "[]";

  File root = SD_MMC.open(path);
  if (!root)
    return "[]";
  if (!root.isDirectory()) {
    root.close();
    return "[]";
  }

  String json = "[";
  bool first = true;

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      if (!first)
        json += ",";
      first = false;

      json += "{\"name\":\"" + String(file.name()) + "\",";
      json += "\"size\":" + String(file.size()) + "}";
    }
    file = root.openNextFile();
  }
  json += "]";
  root.close(); // S3 SD_MMC file objects sometimes need explicit close logic
                // check
  return json;
}
