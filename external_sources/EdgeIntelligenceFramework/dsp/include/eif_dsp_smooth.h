/**
 * @file eif_dsp_smooth.h
 * @brief Smoothing and Noise Reduction Filters
 *
 * Provides lightweight algorithms for real-time signal smoothing:
 * - Exponential Moving Average (EMA)
 * - Median Filter
 * - Moving Average
 *
 * All filters are designed for embedded use with minimal memory.
 */

#ifndef EIF_DSP_SMOOTH_H
#define EIF_DSP_SMOOTH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Exponential Moving Average (EMA)
// =============================================================================

/**
 * @brief EMA filter state
 *
 * Y[n] = alpha * X[n] + (1 - alpha) * Y[n-1]
 */
typedef struct {
  float alpha;      ///< Smoothing factor (0-1), higher = less smoothing
  float output;     ///< Current filtered output
  bool initialized; ///< True after first sample
} eif_ema_t;

/**
 * @brief Initialize EMA filter
 * @param ema Filter state
 * @param alpha Smoothing factor (0-1). Common values: 0.1 (heavy), 0.3
 * (medium), 0.5 (light)
 */
static inline void eif_ema_init(eif_ema_t *ema, float alpha) {
  ema->alpha = alpha;
  ema->output = 0.0f;
  ema->initialized = false;
}

/**
 * @brief Process new sample through EMA
 * @param ema Filter state
 * @param input New input sample
 * @return Filtered output
 */
static inline float eif_ema_update(eif_ema_t *ema, float input) {
  if (!ema->initialized) {
    ema->output = input;
    ema->initialized = true;
  } else {
    ema->output = ema->alpha * input + (1.0f - ema->alpha) * ema->output;
  }
  return ema->output;
}

/**
 * @brief Reset EMA state
 */
static inline void eif_ema_reset(eif_ema_t *ema) {
  ema->output = 0.0f;
  ema->initialized = false;
}

// =============================================================================
// Median Filter
// =============================================================================

/**
 * @brief Median filter state (fixed-size buffer)
 *
 * Median filters are excellent for removing impulse noise (spikes)
 * while preserving edges in the signal.
 */
typedef struct {
  float buffer[7]; ///< Circular buffer (max size 7)
  int size;        ///< Filter window size (3, 5, or 7)
  int index;       ///< Current buffer index
  int count;       ///< Number of samples received
} eif_median_t;

/**
 * @brief Initialize median filter
 * @param mf Filter state
 * @param size Window size (3, 5, or 7)
 */
void eif_median_init(eif_median_t *mf, int size);

/**
 * @brief Process new sample through median filter
 * @param mf Filter state
 * @param input New input sample
 * @return Median of window (or partial median if buffer not full)
 */
float eif_median_update(eif_median_t *mf, float input);

/**
 * @brief Reset median filter
 */
void eif_median_reset(eif_median_t *mf);

// =============================================================================
// Moving Average
// =============================================================================

/**
 * @brief Simple Moving Average filter state
 */
typedef struct {
  float buffer[16]; ///< Circular buffer (max size 16)
  int size;         ///< Window size
  int index;        ///< Current index
  int count;        ///< Samples received
  float sum;        ///< Running sum for efficiency
} eif_ma_t;

/**
 * @brief Initialize moving average filter
 * @param ma Filter state
 * @param size Window size (1-16)
 */
void eif_ma_init(eif_ma_t *ma, int size);

/**
 * @brief Process new sample
 * @return Moving average
 */
float eif_ma_update(eif_ma_t *ma, float input);

/**
 * @brief Reset moving average filter
 */
void eif_ma_reset(eif_ma_t *ma);

// =============================================================================
// Rate Limiter (Slew Rate Control)
// =============================================================================

/**
 * @brief Rate limiter state
 *
 * Limits how fast a signal can change per sample.
 * Useful for smooth servo/motor control and noise rejection.
 */
typedef struct {
  float output;   ///< Current output value
  float max_rate; ///< Maximum change per sample
  bool initialized;
} eif_rate_limiter_t;

/**
 * @brief Initialize rate limiter
 * @param rl Rate limiter state
 * @param max_rate Maximum change per sample (e.g., 0.1 for 10% max change)
 */
static inline void eif_rate_limiter_init(eif_rate_limiter_t *rl,
                                         float max_rate) {
  rl->output = 0.0f;
  rl->max_rate = max_rate;
  rl->initialized = false;
}

/**
 * @brief Process sample through rate limiter
 */
static inline float eif_rate_limiter_update(eif_rate_limiter_t *rl,
                                            float input) {
  if (!rl->initialized) {
    rl->output = input;
    rl->initialized = true;
    return input;
  }

  float delta = input - rl->output;

  // Clamp delta to max_rate
  if (delta > rl->max_rate) {
    delta = rl->max_rate;
  } else if (delta < -rl->max_rate) {
    delta = -rl->max_rate;
  }

  rl->output += delta;
  return rl->output;
}

// =============================================================================
// Hysteresis Filter (Schmitt Trigger)
// =============================================================================

/**
 * @brief Hysteresis filter state
 *
 * Two-threshold switching - prevents rapid toggling near threshold.
 * Used for: temperature control, button debounce, level detection.
 */
typedef struct {
  float low_threshold;  ///< Switch to LOW below this
  float high_threshold; ///< Switch to HIGH above this
  bool state;           ///< Current output state (true = HIGH)
} eif_hysteresis_t;

/**
 * @brief Initialize hysteresis filter
 * @param hyst Filter state
 * @param low_threshold Value below which output goes LOW
 * @param high_threshold Value above which output goes HIGH
 */
static inline void eif_hysteresis_init(eif_hysteresis_t *hyst,
                                       float low_threshold,
                                       float high_threshold) {
  hyst->low_threshold = low_threshold;
  hyst->high_threshold = high_threshold;
  hyst->state = false;
}

/**
 * @brief Update hysteresis filter
 * @return true if HIGH, false if LOW
 */
static inline bool eif_hysteresis_update(eif_hysteresis_t *hyst, float input) {
  if (hyst->state) {
    // Currently HIGH, switch to LOW if below low_threshold
    if (input < hyst->low_threshold) {
      hyst->state = false;
    }
  } else {
    // Currently LOW, switch to HIGH if above high_threshold
    if (input > hyst->high_threshold) {
      hyst->state = true;
    }
  }
  return hyst->state;
}

// =============================================================================
// Debounce Filter
// =============================================================================

/**
 * @brief Debounce filter for digital inputs
 *
 * Requires N consecutive samples of same value before changing state.
 * Perfect for mechanical buttons and switches.
 */
typedef struct {
  bool stable_state; ///< Current stable output
  bool last_input;   ///< Last raw input
  int count;         ///< Consecutive sample count
  int threshold;     ///< Required consecutive samples
} eif_debounce_t;

/**
 * @brief Initialize debounce filter
 * @param db Filter state
 * @param threshold Number of consecutive samples required (e.g., 5)
 */
static inline void eif_debounce_init(eif_debounce_t *db, int threshold) {
  db->stable_state = false;
  db->last_input = false;
  db->count = 0;
  db->threshold = threshold;
}

/**
 * @brief Update debounce filter
 * @param input Raw digital input
 * @return Debounced output
 */
static inline bool eif_debounce_update(eif_debounce_t *db, bool input) {
  if (input == db->last_input) {
    db->count++;
    if (db->count >= db->threshold && input != db->stable_state) {
      db->stable_state = input;
    }
  } else {
    db->count = 1;
  }
  db->last_input = input;
  return db->stable_state;
}

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_SMOOTH_H
