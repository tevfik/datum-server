#ifndef SD_STORAGE_H
#define SD_STORAGE_H

#include "esp_camera.h"
#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>

// SD Card Pins for Freenove / ESP32-S3-Cam (1-bit mode)
// CMD: 38, CLK: 39, D0: 40
#define SD_MMC_CMD 38
#define SD_MMC_CLK 39
#define SD_MMC_D0 40

// Initialize SD Card
bool initSD();

// Save camera framebuffer to SD Card
// Returns true on success
bool saveFrameToSD(camera_fb_t *fb, String filename = "");

// Create standard folders
void createSDFolders();

// List files in a directory (returns JSON string)
String listSDFiles(String path = "/capture");

// Check if SD is available
bool isSDAvailable();

#endif
