/**
 * @file eif_adaptive_threshold_fixed.h
 * @brief Adaptive Threshold (Fixed Point Q15)
 */

#ifndef EIF_ADAPTIVE_THRESHOLD_FIXED_H
#define EIF_ADAPTIVE_THRESHOLD_FIXED_H

#include "eif_fixedpoint.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  q15_t mean;
  q15_t variance; // Q15 representing variance
  q15_t alpha;    // Smoothing factor
  bool initialized;
} eif_adaptive_threshold_fixed_t;

/**
 * @brief Initialize Adaptive Threshold
 */
void eif_adaptive_thresh_init_fixed(eif_adaptive_threshold_fixed_t *at, q15_t alpha);

/**
 * @brief Update statistics
 */
void eif_adaptive_thresh_update_fixed(eif_adaptive_threshold_fixed_t *at, q15_t val);

/**
 * @brief Check if value is anomaly (e.g. > mean + n_std * std)
 * threshold_factor Q11.4 format (16 = 1.0, 32 = 2.0 etc) or just Q15
 */
bool eif_adaptive_thresh_check_fixed(const eif_adaptive_threshold_fixed_t *at, 
                                     q15_t val, 
                                     q15_t std_dev_factor);

#ifdef __cplusplus
}
#endif

#endif // EIF_ADAPTIVE_THRESHOLD_FIXED_H
