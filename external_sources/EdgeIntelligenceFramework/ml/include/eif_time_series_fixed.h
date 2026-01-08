/**
 * @file eif_time_series_fixed.h
 * @brief Time Series Forecasting (Fixed Point Q15)
 */

#ifndef EIF_TIME_SERIES_FIXED_H
#define EIF_TIME_SERIES_FIXED_H

#include "eif_fixedpoint.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Simple Exponential Smoothing (SES) - Fixed Point
// =============================================================================

typedef struct {
  q15_t level;   ///< Current level estimate
  q15_t alpha;   ///< Smoothing factor (0..1 -> 0..32767)
  bool initialized;
} eif_ses_fixed_t;

/**
 * @brief Initialize SES
 */
void eif_ses_init_fixed(eif_ses_fixed_t *ses, q15_t alpha);

/**
 * @brief Update SES with new observation
 * Returns predicted next value
 */
q15_t eif_ses_update_fixed(eif_ses_fixed_t *ses, q15_t obs);

// =============================================================================
// Moving Average (SMA) - Fixed Point
// =============================================================================

typedef struct {
  q15_t *buffer;      ///< Circular buffer
  uint16_t size;      ///< Window size
  uint16_t head;      ///< Current write index
  int32_t sum;        ///< Running sum (Q31 to avoid overflow)
  uint16_t count;     ///< Current element count
} eif_sma_fixed_t;

/**
 * @brief Initialize SMA
 */
void eif_sma_init_fixed(eif_sma_fixed_t *sma, q15_t *buffer, uint16_t size);

/**
 * @brief Update SMA
 */
q15_t eif_sma_update_fixed(eif_sma_fixed_t *sma, q15_t val);

#ifdef __cplusplus
}
#endif

#endif // EIF_TIME_SERIES_FIXED_H
