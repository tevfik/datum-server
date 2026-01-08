#include "eif_motion.h"
#include "img_converters.h" // For jpg2rgb565
#include <Arduino.h>
#include <stdlib.h> // for free, abs

// Globals
bool motionEnabled = true;
int motionThreshold = 30;
int motionPeriodMs = 1000;
unsigned long lastMotionTime = 0;

static uint8_t *prevFrameBuffer = NULL;
static size_t prevFrameLen = 0;

// Debug Macros (matching main file)
#define DEBUG_MODE
#ifdef DEBUG_MODE
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#define DEBUG_PRINTLN(...)
#endif

void eif_motion_init() {
  if (prevFrameBuffer) {
    free(prevFrameBuffer);
    prevFrameBuffer = NULL;
  }
  prevFrameLen = 0;
}

bool eif_motion_check(camera_fb_t *fb) {
  if (!motionEnabled)
    return false;

  // 1. Calculate Target Dimensions (1/4 Scale for speed/memory)
  int outW = fb->width / 4;
  int outH = fb->height / 4;

  if (outW < 1 || outH < 1)
    return false;

  size_t rgbLen = outW * outH * 2; // RGB565 is 2 bytes/pixel
  size_t grayLen = outW * outH;    // 1 byte/pixel for Grayscale

  // 2. Allocate Temp RGB565 Buffer
  uint8_t *rgbBuf = (uint8_t *)ps_malloc(rgbLen);
  if (!rgbBuf)
    return false; // OOM

  // 3. Decode JPEG to RGB565 (Scale 1/4)
  if (!jpg2rgb565(fb->buf, fb->len, rgbBuf, JPG_SCALE_4X)) {
    free(rgbBuf);
    return false;
  }

  // 4. Allocate/Reallocate Previous Frame Buffer (Grayscale)
  if (prevFrameBuffer == NULL || prevFrameLen != grayLen) {
    if (prevFrameBuffer)
      free(prevFrameBuffer);
    prevFrameBuffer = (uint8_t *)ps_malloc(grayLen);
    prevFrameLen = grayLen;

    if (!prevFrameBuffer) { // OOM
      free(rgbBuf);
      return false;
    }

    // Fill first frame with converted gray
    uint16_t *pixPtr = (uint16_t *)rgbBuf;
    for (size_t i = 0; i < grayLen; i++) {
      uint16_t pixel = pixPtr[i];
      // Green channel as luminance proxy
      prevFrameBuffer[i] = ((pixel >> 5) & 0x3F) << 2;
    }
    free(rgbBuf);
    return false;
  }

  // 5. Compare
  int changes = 0;
  int skip = 2; // Pixel skip
  uint16_t *pixPtr = (uint16_t *)rgbBuf;
  bool triggered = false;

  for (size_t i = 0; i < grayLen; i += skip) {
    uint16_t pixel = pixPtr[i];
    uint8_t currentGray = ((pixel >> 5) & 0x3F) << 2;
    uint8_t prevGray = prevFrameBuffer[i];

    if (abs(currentGray - prevGray) > motionThreshold) {
      changes++;
    }
    // Update history
    prevFrameBuffer[i] = currentGray;
  }

  free(rgbBuf);

  // 6. Threshold Check
  int totalChecked = grayLen / skip;
  float changePct = (float)changes * 100.0 / totalChecked;

  // Debug: Log only significant motion (> 1.0%) to avoid spam
  if (changePct > 1.0) {
    DEBUG_PRINTF("Motion Activity: %.2f%% (Alarm Threshold: >5.00%%)\n",
                 changePct);
  }

  if (changePct > 5.0) {
    DEBUG_PRINTLN("!!! MOTION ALARM TRIGGERED !!!");
    lastMotionTime = millis();
    triggered = true;
  }

  return triggered;
}
