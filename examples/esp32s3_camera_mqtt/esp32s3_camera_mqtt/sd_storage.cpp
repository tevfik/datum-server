#include "sd_storage.h"
#include "esp_camera.h"
#include <Arduino.h>
#include <FS.h>
#include <HTTPClient.h>
#include <SD_MMC.h>
#include <time.h>

static bool sdInitialized = false;
static String sdStatus = "Not Initialized";

static String _userToken = "";
static String _serverURL = "";
static bool timeSynced = false;

void setUserToken(String token) { _userToken = token; }
void setServerURL(String url) { _serverURL = url; }

// Recording Globals
static File aviFile;
static bool isRecording = false;
static unsigned long recordingStartTime = 0;
static uint32_t frameCount = 0;
static uint32_t moviSize = 0;
static uint16_t avi_width = 0;
static uint16_t avi_height = 0;
static uint8_t avi_rate = 10;

void syncTime() {
  if (_serverURL.length() == 0)
    return;

  HTTPClient http;
  String url = _serverURL + "/system/time";

  // Christian's Algorithm: Measure RTT and compensate
  unsigned long t_start = millis();

  http.begin(url);
  int code = http.GET();

  if (code == 200) {
    String payload = http.getString();
    unsigned long t_end = millis();

    // Parse unix_ms for precise timing
    int unixMsIdx = payload.indexOf("\"unix_ms\":");
    if (unixMsIdx > 0) {
      int valStart = unixMsIdx + 10;
      int valEnd = payload.indexOf(",", valStart);
      if (valEnd < 0)
        valEnd = payload.indexOf("}", valStart);

      String unixMsStr = payload.substring(valStart, valEnd);
      uint64_t server_ms = strtoull(unixMsStr.c_str(), NULL, 10);

      // Apply Christian's Algorithm: server_time + RTT/2
      unsigned long rtt = t_end - t_start;
      uint64_t adjusted_ms = server_ms + (rtt / 2);

      struct timeval tv;
      tv.tv_sec = adjusted_ms / 1000;
      tv.tv_usec = (adjusted_ms % 1000) * 1000;
      settimeofday(&tv, NULL);

      timeSynced = true;

      // Print human-readable time
      time_t now_t = tv.tv_sec;
      struct tm *now_tm = localtime(&now_t);
      char buf[32];
      strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", now_tm);
      Serial.printf("[TIME] Synced via HTTP (RTT: %lu ms): %s\n", rtt, buf);
    } else {
      // Fallback to iso8601 parsing if unix_ms not available
      int isoIdx = payload.indexOf("\"iso8601\":\"");
      if (isoIdx > 0) {
        String iso = payload.substring(isoIdx + 11, isoIdx + 30);
        struct tm tm;
        tm.tm_year = iso.substring(0, 4).toInt() - 1900;
        tm.tm_mon = iso.substring(5, 7).toInt() - 1;
        tm.tm_mday = iso.substring(8, 10).toInt();
        tm.tm_hour = iso.substring(11, 13).toInt();
        tm.tm_min = iso.substring(14, 16).toInt();
        tm.tm_sec = iso.substring(17, 19).toInt();

        time_t t = mktime(&tm);
        struct timeval now = {.tv_sec = t};
        settimeofday(&now, NULL);

        timeSynced = true;
        Serial.println("[TIME] Synced (ISO8601): " + iso);
      }
    }
  } else {
    Serial.printf("[TIME] Sync failed: HTTP %d\n", code);
  }
  http.end();
}

bool initSD() {
  if (sdInitialized)
    return true;

  // Configure pins for SD_MMC 1-bit mode
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);

  // Mount SD Card (false = no format on fail, true = 1-bit mode, true = format
  // if failed)
  if (!SD_MMC.begin("/sdcard", true, true)) {
    Serial.println("[SD] Mount Failed! Check card inserted or formatting.");
    sdStatus = "Mount Failed";
    return false;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("[SD] No SD Card attached");
    sdStatus = "No Card";
    return false;
  }

  String typeStr = "Unknown";
  if (cardType == CARD_MMC)
    typeStr = "MMC";
  else if (cardType == CARD_SD)
    typeStr = "SDSC";
  else if (cardType == CARD_SDHC)
    typeStr = "SDHC";

  uint64_t sizeMB = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("[SD] SD Card Mounted. Type: %s, Size: %lluMB\n",
                typeStr.c_str(), sizeMB);

  sdStatus = typeStr + " (" + String(sizeMB) + " MB)";
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

String getSDStatus() { return sdStatus; }

bool saveFrameToSD(camera_fb_t *fb, String filename) {
  if (!sdInitialized)
    return false;

  // Auto-cleanup before saving
  checkSDSpace();

  // Generate Filename if empty
  if (filename.length() == 0) {
    if (timeSynced) {
      time_t now;
      time(&now);
      struct tm *ti = localtime(&now);
      char buf[64]; // Increased to 64 (Fixes Stack Smashing)
      // Use VID_ prefix so mixed JPG/AVI files sort correctly by date
      sprintf(buf, "/capture/VID_%04d%02d%02d_%02d%02d%02d.jpg",
              ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday, ti->tm_hour,
              ti->tm_min, ti->tm_sec);
      filename = String(buf);
    } else {
      // Fallback to sequential
      int i = 0;
      do {
        filename = "/capture/capture_" + String(i) + ".jpg";
        i++;
        if (i > 9999)
          i = 0; // Wrap around safely?
      } while (SD_MMC.exists(filename));
    }
  }

  // Use SD_MMC to write
  File file = SD_MMC.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("[SD] Failed to open file for writing: " + filename);
    return false;
  }

  file.write(fb->buf, fb->len);
  file.close();
  Serial.println("[SD] Saved: " + filename);
  return true;
}

String generateFileName(String ext) {
  time_t now;
  time(&now);
  struct tm *ti = localtime(&now);
  char buf[64];
  sprintf(buf, "/capture/VID_%04d%02d%02d_%02d%02d%02d.%s", ti->tm_year + 1900,
          ti->tm_mon + 1, ti->tm_mday, ti->tm_hour, ti->tm_min, ti->tm_sec,
          ext.c_str());
  return String(buf);
}

bool startRecording(String filename, uint16_t width, uint16_t height,
                    uint8_t rate) {
  if (!sdInitialized)
    return false;
  if (isRecording)
    stopRecording();

  // Generate filename if needed
  if (filename.length() == 0) {
    filename = generateFileName("avi");
  }

  aviFile = SD_MMC.open(filename, FILE_WRITE);
  if (!aviFile) {
    Serial.println("[AVI] Failed to open: " + filename);
    return false;
  }

  isRecording = true;
  frameCount = 0;
  moviSize = 0;
  recordingStartTime = millis();
  avi_width = width;
  avi_height = height;
  avi_rate = rate;

  // Placeholder Header (approx 240 bytes)
  // We will rewrite this on close
  uint8_t zero[240] = {0};
  aviFile.write(zero, 240);

  Serial.println("[AVI] Recording Started: " + filename);
  return true;
}

bool processRecording(camera_fb_t *fb) {
  if (!isRecording || !aviFile)
    return false;

  // Calculate padding for 4-byte alignment (JPEG data usually doesn't need
  // strict alignment in AVI chunks but good practice)
  size_t jpg_len = fb->len;
  size_t rem = jpg_len % 4;
  size_t pad = (rem > 0) ? 4 - rem : 0;

  // Write Chunk Header: "00dc" + size
  // 00dc = Compressed Video Frame
  const uint8_t dc[4] = {'0', '0', 'd', 'c'};
  aviFile.write(dc, 4);

  uint32_t s = jpg_len + pad;
  aviFile.write((uint8_t *)&s, 4);

  // Write Data
  aviFile.write(fb->buf, jpg_len);

  // Write Pad
  if (pad > 0) {
    uint8_t zero = 0;
    for (size_t i = 0; i < pad; i++)
      aviFile.write(&zero, 1);
  }

  moviSize += (8 + jpg_len + pad); // Chunk Header (8) + Data + Pad
  frameCount++;
  return true;
}

void stopRecording() {
  if (!isRecording || !aviFile)
    return;

  unsigned long duration = millis() - recordingStartTime;
  float fps = (duration > 0) ? (frameCount * 1000.0f / duration) : 0;
  Serial.printf("[AVI] Stopping. Frames: %d, Duration: %lums, FPS: %.1f\n",
                frameCount, duration, fps);

  // Re-calculate actual FPS for header if desired, or stick to requested
  // Let's use actual FPS ceiling for playback speed
  uint32_t usecs_per_frame =
      (frameCount > 0) ? (duration * 1000 / frameCount) : (1000000 / avi_rate);

  // Rewind and write header
  aviFile.seek(0);

  // RIFF Header
  uint32_t fileSize = aviFile.size();
  uint32_t riffSize = fileSize - 8;

  aviFile.write((const uint8_t *)"RIFF", 4);
  aviFile.write((uint8_t *)&riffSize, 4);
  aviFile.write((const uint8_t *)"AVI ", 4);

  // LIST hdrl
  uint32_t hdrlSize = 4 + 64 + 116; // LIST(4) + strl(116) + avih(64) ... approx
  // Let's explicitly construct it

  aviFile.write((const uint8_t *)"LIST", 4);
  uint32_t list1Size =
      192; // avih(64) + strl(116) + "hdrl"(4) + "strl"(4)... math is tricky
  // 192 + 8 (LIST movi header) + moviSize? No.
  // Standard simplified header size

  // Hardcoded Header Approach (easier)
  // Total Header ~224 bytes?
  uint32_t headers_size = 224;

  aviFile.write((uint8_t *)&headers_size, 4); // Size of LIST hdrl
  aviFile.write((const uint8_t *)"hdrl", 4);

  // avih
  aviFile.write((const uint8_t *)"avih", 4);
  uint32_t avihSize = 56;
  aviFile.write((uint8_t *)&avihSize, 4);
  aviFile.write((uint8_t *)&usecs_per_frame, 4); // Microsec per frame
  uint32_t max_bytes_per_sec = 1000000;          // dummy
  aviFile.write((uint8_t *)&max_bytes_per_sec, 4);
  uint32_t padding = 0;
  aviFile.write((uint8_t *)&padding, 4); // Padding
  uint32_t flags = 0;
  aviFile.write((uint8_t *)&flags, 4);
  aviFile.write((uint8_t *)&frameCount, 4); // Total Frames
  aviFile.write((uint8_t *)&padding, 4);    // Initial Frames
  uint32_t streams = 1;
  aviFile.write((uint8_t *)&streams, 4);
  aviFile.write((uint8_t *)&padding, 4); // Buffer Size
  aviFile.write((uint8_t *)&avi_width, 4);
  aviFile.write((uint8_t *)&avi_height, 4);
  aviFile.write((uint8_t *)&padding, 4); // Reserved
  aviFile.write((uint8_t *)&padding, 4);
  aviFile.write((uint8_t *)&padding, 4);
  aviFile.write((uint8_t *)&padding, 4);

  // LIST strl
  aviFile.write((const uint8_t *)"LIST", 4);
  uint32_t strlSize = 116;
  aviFile.write((uint8_t *)&strlSize, 4);
  aviFile.write((const uint8_t *)"strl", 4);

  // strh
  aviFile.write((const uint8_t *)"strh", 4);
  uint32_t strhSize = 56;
  aviFile.write((uint8_t *)&strhSize, 4);
  aviFile.write((const uint8_t *)"vids", 4); // Type
  aviFile.write((const uint8_t *)"MJPG", 4); // Handler
  aviFile.write((uint8_t *)&flags, 4);
  aviFile.write((uint8_t *)&padding, 4);
  aviFile.write((uint8_t *)&padding, 4);
  aviFile.write((uint8_t *)&usecs_per_frame,
                4); // Scale/Rate? -> Scale=usec, Rate=1000000?
  // Standard AVI: Scale=1, Rate=FPS. Or Scale=Microsec, Rate=1.
  uint32_t scale = 1;
  uint32_t rate_val =
      (frameCount > 0) ? (frameCount * 1000000 / duration) : avi_rate;
  if (rate_val == 0)
    rate_val = 1;
  aviFile.write((uint8_t *)&scale, 4);
  aviFile.write((uint8_t *)&rate_val, 4);

  aviFile.write((uint8_t *)&padding, 4);    // Start
  aviFile.write((uint8_t *)&frameCount, 4); // Length
  aviFile.write((uint8_t *)&padding, 4);    // Buffer
  uint32_t quality = 10000;
  aviFile.write((uint8_t *)&quality, 4);
  aviFile.write((uint8_t *)&padding, 4); // Sample Size
  int16_t frame_left = 0, frame_top = 0, frame_right = avi_width,
          frame_bottom = avi_height;
  aviFile.write((uint8_t *)&frame_left, 2);
  aviFile.write((uint8_t *)&frame_top, 2);
  aviFile.write((uint8_t *)&frame_right, 2);
  aviFile.write((uint8_t *)&frame_bottom, 2);

  // strf
  aviFile.write((const uint8_t *)"strf", 4);
  uint32_t strfSize = 40;
  aviFile.write((uint8_t *)&strfSize, 4);
  aviFile.write((uint8_t *)&strfSize, 4); // biSize
  aviFile.write((uint8_t *)&avi_width, 4);
  aviFile.write((uint8_t *)&avi_height, 4);
  uint16_t planes = 1;
  aviFile.write((uint8_t *)&planes, 2);
  uint16_t bitcount = 24;
  aviFile.write((uint8_t *)&bitcount, 2);
  aviFile.write((const uint8_t *)"MJPG", 4); // Compression
  uint32_t imageSize = avi_width * avi_height * 3;
  aviFile.write((uint8_t *)&imageSize, 4);
  aviFile.write((uint8_t *)&padding, 4); // XPels
  aviFile.write((uint8_t *)&padding, 4); // YPels
  aviFile.write((uint8_t *)&padding, 4); // ClrUsed
  aviFile.write((uint8_t *)&padding, 4); // ClrImportant

  // LIST movi
  aviFile.write((const uint8_t *)"LIST", 4);
  aviFile.write((uint8_t *)&moviSize, 4);
  aviFile.write((const uint8_t *)"movi", 4);

  aviFile.close();
  isRecording = false;
  Serial.println("[AVI] Recording Saved.");
}

bool isRecordingActive() { return isRecording; }

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

bool deleteSDFile(String path) {
  if (!sdInitialized)
    return false;

  if (SD_MMC.exists(path)) {
    Serial.println("[SD] Deleting: " + path);
    return SD_MMC.remove(path);
  }
  return false;
}

// Smarter checkSDSpace: Scans for the oldest file by name comparison
void checkSDSpace() {
  if (!sdInitialized)
    return;

  uint64_t total = SD_MMC.totalBytes();
  uint64_t used = SD_MMC.usedBytes();
  uint64_t free = total - used;
  uint64_t threshold = 50ULL * 1024 * 1024; // 50MB (Requested "Close to full")

  if (free < threshold) {
    Serial.printf("[SD] Low Space: %llu MB free. Cleaning up oldest file...\n",
                  free / (1024 * 1024));

    // Scan once, find the single oldest file
    File root = SD_MMC.open("/capture");
    if (!root || !root.isDirectory())
      return;

    String oldestName = "";

    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        String currentName = String(file.name());
        if (oldestName == "") {
          oldestName = currentName;
        } else {
          // Lexicographical comparison:
          // If currentName < oldestName, it is "older" (IMG < VID, 2025 < 2026)
          if (currentName.compareTo(oldestName) < 0) {
            oldestName = currentName;
          }
        }
      }
      file.close(); // Close to keep memory low
      file = root.openNextFile();
    }
    root.close();

    if (oldestName != "") {
      String fullPath = "/capture/" + oldestName;
      Serial.println("[SD] Auto-Cleaning (Oldest): " + fullPath);
      SD_MMC.remove(fullPath);
    }
  }
}
