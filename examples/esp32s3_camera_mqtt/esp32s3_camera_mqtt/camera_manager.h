#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <Arduino.h>

// Shared Globals
extern SemaphoreHandle_t cameraMutex;
extern volatile bool streaming;

// Shared Frame Buffer Strategy (Producer: Core 1, Consumer: Core 0)
// This buffer holds a COPY of the latest frame for the web stream
void updateSharedFrame(camera_fb_t *fb);
// Consumer provided pointer to buffer pointer. It will be reallocated if too
// small.
void copySharedFrame(uint8_t **destBuf, size_t *destLen, size_t *destCapacity);

#include "eif_motion.h" // Include C-linkage globals

// Functions
bool initCamera();
void startCameraTask();
void startUploadTask(); // Async frame upload task
void handleSnap(String resolution = "", bool saveToCard = false);
void ignoreMotionFor(int frames);

// Helpers
framesize_t getFrameSizeFromName(String name);
String getFrameSizeName(framesize_t size);

#endif
