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
// Filename is optional, if empty it generates one based on timestamp
bool saveFrameToSD(camera_fb_t *fb, String filename = "");

void setUserToken(String token);
void setServerURL(String url);

// Sync Time from Server
void syncTime();

// Video Recording (AVI)
bool startRecording(String filename = "", uint16_t width = 640,
                    uint16_t height = 480, uint8_t rate = 10);
bool processRecording(camera_fb_t *fb);
void stopRecording();
bool isRecordingActive();

// Create standard folders
void createSDFolders();

// List files in a directory (returns JSON string)
String listSDFiles(String path = "/capture");
bool deleteSDFile(String path); // New delete function

// Check if SD is available
bool isSDAvailable();

// Generate Timestamped Filename
String generateFileName(String ext);

// Get Status Message
String getSDStatus();

#endif
