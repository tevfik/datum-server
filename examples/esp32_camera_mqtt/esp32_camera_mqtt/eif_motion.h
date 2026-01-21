#ifndef EIF_MOTION_H
#define EIF_MOTION_H

#include "esp_camera.h"
#include <Arduino.h>

// Define types if header order is tricky, but Arduino.h handles uint8_t
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Configuration
extern bool motionEnabled;
extern int motionThreshold;
extern int motionPeriodMs;
extern float motionMinAreaPct;
extern unsigned long lastMotionTime;

/**
 * @brief Initialize motion detection buffers
 */
void eif_motion_init();

/**
 * @brief Check for motion in the provided frame
 *
 * @param fb Camera Frame Buffer
 * @return true if motion detected
 */
bool eif_motion_check(camera_fb_t *fb);

#ifdef __cplusplus
}
#endif

#endif // EIF_MOTION_H
