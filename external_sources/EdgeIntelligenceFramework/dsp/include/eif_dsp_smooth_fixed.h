/**
 * @file eif_dsp_smooth_fixed.h
 * @brief Fixed-Point Smoothing Filters
 */

#ifndef EIF_DSP_SMOOTH_FIXED_H
#define EIF_DSP_SMOOTH_FIXED_H

#include "eif_fixedpoint.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Median Filter ---
typedef struct {
  q15_t buffer[7]; // Max size 7 for efficiency
  int size;
  int index;
  int count;
} eif_median_q15_t;

void eif_median_q15_init(eif_median_q15_t *mf, int size);
void eif_median_q15_reset(eif_median_q15_t *mf);
q15_t eif_median_q15_update(eif_median_q15_t *mf, q15_t input);

// --- Moving Average ---
typedef struct {
  q15_t buffer[16]; // Max size 16
  int size;
  int index;
  int count;
  q31_t sum; // Accumulator in Q31
} eif_ma_q15_t;

void eif_ma_q15_init(eif_ma_q15_t *ma, int size);
void eif_ma_q15_reset(eif_ma_q15_t *ma);
q15_t eif_ma_q15_update(eif_ma_q15_t *ma, q15_t input);

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_SMOOTH_FIXED_H
