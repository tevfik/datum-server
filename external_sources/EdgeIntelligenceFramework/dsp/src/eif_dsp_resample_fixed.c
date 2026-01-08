/**
 * @file eif_dsp_resample_fixed.c
 * @brief Fixed-Point Resampling Implementation
 */

#include "eif_dsp_resample_fixed.h"

eif_status_t eif_resample_linear_q15(const q15_t *input, size_t in_len,
                                     q15_t *output, size_t out_len) {
  if (!input || !output || out_len == 0 || in_len == 0)
    return EIF_STATUS_INVALID_ARGUMENT;

  if (out_len == 1) {
    output[0] = input[0];
    return EIF_STATUS_OK;
  }

  // Step size in Q16 fixed point to handle fractional steps
  // We want step = (in_len - 1) / (out_len - 1)
  // Use Q16.16 format for 'pos' to get enough precision

  uint32_t step_q16 = ((in_len - 1) << 16) / (out_len - 1);

  for (size_t i = 0; i < out_len; i++) {
    uint32_t exact_pos_q16 = i * step_q16;
    size_t idx = exact_pos_q16 >> 16;

    // Fraction is low 16 bits. Convert to Q15 (Shift >> 1)
    q15_t frac = (q15_t)((exact_pos_q16 & 0xFFFF) >> 1);

    if (idx >= in_len - 1) {
      output[i] = input[in_len - 1];
    } else {
      // Linear Interp: y0 + frac * (y1 - y0)
      q15_t y0 = input[idx];
      q15_t y1 = input[idx + 1];

      // frac is Q15 [0..1). (y1-y0) is Q15.
      // frac * delta -> Q30. >> 15 -> Q15.
      q15_t delta = y1 - y0;
      q31_t product = ((q31_t)frac * delta) >> 15;

      output[i] = y0 + (q15_t)product;
    }
  }

  return EIF_STATUS_OK;
}
