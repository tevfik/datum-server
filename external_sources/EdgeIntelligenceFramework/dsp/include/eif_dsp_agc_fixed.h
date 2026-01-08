/**
 * @file eif_dsp_agc_fixed.h
 * @brief Fixed-Point AGC
 */

#ifndef EIF_DSP_AGC_FIXED_H
#define EIF_DSP_AGC_FIXED_H

#include "eif_fixedpoint.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  q15_t target_level;
  q15_t max_gain; // Max gain (Q12.3: 1.0 = 8, 8.0 = 64 etc? No, let's stick to
                  // Q15 and shift) If max gain > 1, we need headroom. Let's say
                  // gain is maintained in Q12.4 format (up to ~2048x? No Q12.4
                  // is 12 integer bits). Q4.11 format: 4 integer bits
                  // (max 15.0), 11 fractional.

  q15_t current_gain; // Q4.11 format
  q15_t attack;       // Q15 coefficient
  q15_t decay;        // Q15 coefficient
  q15_t noise_gate;   // Q15 threshold
} eif_agc_q15_t;

void eif_agc_q15_init(eif_agc_q15_t *ctx, q15_t target, q15_t max_gain);

/**
 * @brief Process buffer with AGC
 * @param input Input Q15
 * @param output Output Q15
 * @param length Sample count
 */
void eif_agc_q15_process(eif_agc_q15_t *ctx, const q15_t *input, q15_t *output,
                         size_t length);

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_AGC_FIXED_H
