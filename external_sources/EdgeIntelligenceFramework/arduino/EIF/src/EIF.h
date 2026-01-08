/**
 * @file EIF.h
 * @brief Edge Intelligence Framework - Arduino Main Header
 *
 * Include this header to access all EIF functionality.
 *
 * For memory-constrained boards (AVR), include only the headers you need.
 */

#ifndef EIF_H
#define EIF_H

// Arduino compatibility
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// DSP - Signal Processing
// =============================================================================

// Smoothing and filtering
#include "eif_dsp_smooth.h"

// FIR Filters
#include "eif_dsp_fir.h"
#include "eif_dsp_fir_fixed.h"

// IIR/Biquad Filters
#include "eif_dsp_biquad.h"
#include "eif_dsp_biquad_fixed.h"

// =============================================================================
// ML - Machine Learning (for boards with more RAM)
// =============================================================================

#if defined(__arm__) || defined(ESP32) || defined(ESP8266)
// Adaptive thresholds
#include "eif_adaptive_threshold.h"

// Sensor fusion
#include "eif_sensor_fusion.h"

// Activity recognition
#include "eif_activity.h"

// Predictive maintenance
#include "eif_predictive_maintenance.h"
#endif

// =============================================================================
// Utility Macros
// =============================================================================

// Q15 conversion helpers
#define EIF_FLOAT_TO_Q15(x) ((int16_t)((x) * 32767.0f))
#define EIF_Q15_TO_FLOAT(x) ((float)(x) / 32767.0f)

// Clamp macro
#define EIF_CLAMP(x, min, max)                                                 \
  ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

// =============================================================================
// Arduino-specific Helpers
// =============================================================================

#ifdef ARDUINO

/**
 * @brief Read analog and convert to float (0.0 - 1.0)
 */
static inline float eif_analog_read_float(uint8_t pin) {
  return analogRead(pin) / 1023.0f;
}

/**
 * @brief Read analog centered around zero (-1.0 to 1.0)
 */
static inline float eif_analog_read_centered(uint8_t pin) {
  return (analogRead(pin) - 512) / 512.0f;
}

/**
 * @brief Read analog as Q15 fixed-point
 */
static inline int16_t eif_analog_read_q15(uint8_t pin) {
  return (analogRead(pin) - 512) << 5; // Scale 10-bit to Q15
}

#endif // ARDUINO

#endif // EIF_H
