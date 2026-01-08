/**
 * @file eif_dsp_control.h
 * @brief Control System Utilities
 *
 * Provides lightweight algorithms for real-time control:
 * - Deadzone (joystick centering)
 * - Differentiator (rate of change)
 * - Integrator (with anti-windup)
 * - Zero-crossing detector
 * - Peak detector / envelope follower
 *
 * All filters are designed for embedded use with minimal memory.
 */

#ifndef EIF_DSP_CONTROL_H
#define EIF_DSP_CONTROL_H

#include <math.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Deadzone Filter
// =============================================================================

/**
 * @brief Deadzone filter
 *
 * Creates a "dead" region around zero where input is zeroed.
 * Perfect for joysticks, thumbsticks, and analog inputs.
 */
typedef struct {
  float threshold; ///< Values within +/- threshold become 0
  float scale;     ///< Output scaling factor (1.0 = no scaling)
} eif_deadzone_t;

/**
 * @brief Initialize deadzone filter
 * @param dz Filter state
 * @param threshold Dead zone threshold (e.g., 0.1 for 10%)
 */
static inline void eif_deadzone_init(eif_deadzone_t *dz, float threshold) {
  dz->threshold = threshold > 0 ? threshold : 0.1f;
  dz->scale = 1.0f / (1.0f - dz->threshold);
}

/**
 * @brief Apply deadzone to input
 * @return 0 if within deadzone, scaled value otherwise
 */
static inline float eif_deadzone_apply(eif_deadzone_t *dz, float input) {
  if (fabsf(input) < dz->threshold) {
    return 0.0f;
  }
  // Scale output to maintain full range
  if (input > 0) {
    return (input - dz->threshold) * dz->scale;
  } else {
    return (input + dz->threshold) * dz->scale;
  }
}

// =============================================================================
// Differentiator (Rate of Change)
// =============================================================================

/**
 * @brief Simple differentiator
 *
 * Calculates rate of change (derivative) of signal.
 * Uses backward difference: dy/dt ≈ (y[n] - y[n-1]) / dt
 */
typedef struct {
  float prev_value; ///< Previous sample
  float dt;         ///< Sample period (1/sample_rate)
  bool initialized;
} eif_differentiator_t;

/**
 * @brief Initialize differentiator
 * @param diff Differentiator state
 * @param sample_rate Sample rate in Hz
 */
static inline void eif_differentiator_init(eif_differentiator_t *diff,
                                           float sample_rate) {
  diff->prev_value = 0.0f;
  diff->dt = 1.0f / sample_rate;
  diff->initialized = false;
}

/**
 * @brief Calculate derivative
 * @return Rate of change (units per second)
 */
static inline float eif_differentiator_update(eif_differentiator_t *diff,
                                              float input) {
  float derivative = 0.0f;

  if (diff->initialized) {
    derivative = (input - diff->prev_value) / diff->dt;
  }

  diff->prev_value = input;
  diff->initialized = true;

  return derivative;
}

// =============================================================================
// Integrator (with Anti-Windup)
// =============================================================================

/**
 * @brief Simple integrator with clamping
 *
 * Accumulates signal over time with limits to prevent windup.
 * Uses trapezoidal integration: ∫y dt ≈ (y[n] + y[n-1]) * dt / 2
 */
typedef struct {
  float integral;   ///< Accumulated value
  float prev_value; ///< Previous sample
  float dt;         ///< Sample period
  float min_limit;  ///< Lower clamp
  float max_limit;  ///< Upper clamp
  bool initialized;
} eif_integrator_t;

/**
 * @brief Initialize integrator
 * @param integ Integrator state
 * @param sample_rate Sample rate in Hz
 * @param min_limit Lower limit for anti-windup
 * @param max_limit Upper limit for anti-windup
 */
static inline void eif_integrator_init(eif_integrator_t *integ,
                                       float sample_rate, float min_limit,
                                       float max_limit) {
  integ->integral = 0.0f;
  integ->prev_value = 0.0f;
  integ->dt = 1.0f / sample_rate;
  integ->min_limit = min_limit;
  integ->max_limit = max_limit;
  integ->initialized = false;
}

/**
 * @brief Accumulate sample
 * @return Current integral value
 */
static inline float eif_integrator_update(eif_integrator_t *integ,
                                          float input) {
  if (integ->initialized) {
    // Trapezoidal integration
    integ->integral += (input + integ->prev_value) * integ->dt * 0.5f;

    // Anti-windup clamping
    if (integ->integral > integ->max_limit) {
      integ->integral = integ->max_limit;
    } else if (integ->integral < integ->min_limit) {
      integ->integral = integ->min_limit;
    }
  }

  integ->prev_value = input;
  integ->initialized = true;

  return integ->integral;
}

/**
 * @brief Reset integrator
 */
static inline void eif_integrator_reset(eif_integrator_t *integ) {
  integ->integral = 0.0f;
  integ->initialized = false;
}

// =============================================================================
// Zero-Crossing Detector
// =============================================================================

/**
 * @brief Zero-crossing detector
 *
 * Detects when signal crosses zero (positive-to-negative or vice versa).
 * Useful for frequency measurement, phase detection.
 */
typedef struct {
  float prev_value;
  bool prev_positive;
  bool initialized;
  int crossing_count; ///< Total crossings detected
} eif_zero_cross_t;

/**
 * @brief Initialize zero-crossing detector
 */
static inline void eif_zero_cross_init(eif_zero_cross_t *zc) {
  zc->prev_value = 0.0f;
  zc->prev_positive = true;
  zc->initialized = false;
  zc->crossing_count = 0;
}

/**
 * @brief Check for zero crossing
 * @return 1 = rising crossing, -1 = falling crossing, 0 = no crossing
 */
static inline int eif_zero_cross_update(eif_zero_cross_t *zc, float input) {
  int result = 0;
  bool current_positive = (input >= 0.0f);

  if (zc->initialized && current_positive != zc->prev_positive) {
    result = current_positive ? 1 : -1; // Rising or falling
    zc->crossing_count++;
  }

  zc->prev_value = input;
  zc->prev_positive = current_positive;
  zc->initialized = true;

  return result;
}

// =============================================================================
// Peak Detector / Envelope Follower
// =============================================================================

/**
 * @brief Peak detector with decay
 *
 * Tracks peak values with configurable attack and decay times.
 * Perfect for audio level meters, envelope following.
 */
typedef struct {
  float peak;        ///< Current peak value
  float attack_coef; ///< Attack coefficient (0-1, higher = faster)
  float decay_coef;  ///< Decay coefficient (0-1, higher = faster)
} eif_peak_detector_t;

/**
 * @brief Initialize peak detector
 * @param pd Peak detector state
 * @param attack_ms Attack time in milliseconds
 * @param decay_ms Decay time in milliseconds
 * @param sample_rate Sample rate in Hz
 */
static inline void eif_peak_detector_init(eif_peak_detector_t *pd,
                                          float attack_ms, float decay_ms,
                                          float sample_rate) {
  pd->peak = 0.0f;
  // Convert time constants to coefficients
  // coef = 1 - exp(-2.2 / (time_ms * sample_rate / 1000))
  float attack_samples = attack_ms * sample_rate / 1000.0f;
  float decay_samples = decay_ms * sample_rate / 1000.0f;

  pd->attack_coef = 1.0f - expf(-2.2f / attack_samples);
  pd->decay_coef = 1.0f - expf(-2.2f / decay_samples);
}

/**
 * @brief Update peak detector
 * @param input Absolute input value (use fabsf for bipolar signals)
 * @return Current envelope level
 */
static inline float eif_peak_detector_update(eif_peak_detector_t *pd,
                                             float input) {
  float abs_input = fabsf(input);

  if (abs_input > pd->peak) {
    // Attack: fast rise to new peak
    pd->peak += pd->attack_coef * (abs_input - pd->peak);
  } else {
    // Decay: slow fall
    pd->peak += pd->decay_coef * (abs_input - pd->peak);
  }

  return pd->peak;
}

/**
 * @brief Reset peak detector
 */
static inline void eif_peak_detector_reset(eif_peak_detector_t *pd) {
  pd->peak = 0.0f;
}

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Clamp value to range [min, max]
 */
static inline float eif_clampf(float value, float min_val, float max_val) {
  if (value < min_val)
    return min_val;
  if (value > max_val)
    return max_val;
  return value;
}

/**
 * @brief Linear interpolation (lerp)
 * @param a Start value
 * @param b End value
 * @param t Interpolation factor (0-1)
 * @return a + (b - a) * t
 */
static inline float eif_lerpf(float a, float b, float t) {
  return a + (b - a) * t;
}

/**
 * @brief Map value from one range to another
 * @param x Input value
 * @param in_min Input range minimum
 * @param in_max Input range maximum
 * @param out_min Output range minimum
 * @param out_max Output range maximum
 * @return Mapped value
 */
static inline float eif_mapf(float x, float in_min, float in_max, float out_min,
                             float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/**
 * @brief Sign function
 * @return 1.0 if positive, -1.0 if negative, 0.0 if zero
 */
static inline float eif_signf(float x) {
  if (x > 0.0f)
    return 1.0f;
  if (x < 0.0f)
    return -1.0f;
  return 0.0f;
}

/**
 * @brief Smooth step (S-curve) interpolation
 * @param edge0 Lower edge
 * @param edge1 Upper edge
 * @param x Input value
 * @return Hermite interpolation between 0 and 1
 */
static inline float eif_smoothstepf(float edge0, float edge1, float x) {
  float t = eif_clampf((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

/**
 * @brief Wrap angle to [-PI, PI]
 */
static inline float eif_wrap_anglef(float angle) {
  while (angle > M_PI)
    angle -= 2.0f * M_PI;
  while (angle < -M_PI)
    angle += 2.0f * M_PI;
  return angle;
}

/**
 * @brief Simple low-pass filter coefficient calculation
 * @param cutoff_hz Cutoff frequency in Hz
 * @param sample_rate Sample rate in Hz
 * @return Alpha coefficient for EMA
 */
static inline float eif_calc_lpf_alpha(float cutoff_hz, float sample_rate) {
  float rc = 1.0f / (2.0f * M_PI * cutoff_hz);
  float dt = 1.0f / sample_rate;
  return dt / (rc + dt);
}

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_CONTROL_H
