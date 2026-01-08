/**
 * @file eif_dsp_biquad_fixed.h
 * @brief Fixed-Point Biquad Filter
 *
 * Memory-efficient fixed-point biquad filter:
 * - Q15 format for coefficients and state
 * - Q31 accumulator for precision
 * - Direct Form II Transposed
 *
 * Perfect for: ARM Cortex-M0/M0+, low-power MCUs
 */

#ifndef EIF_DSP_BIQUAD_FIXED_H
#define EIF_DSP_BIQUAD_FIXED_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EIF_BIQUAD_FIXED_MAX_STAGES 4

// =============================================================================
// Q15 Biquad Section
// =============================================================================

/**
 * @brief Q15 Biquad filter structure
 *
 * Implements Direct Form II Transposed:
 *   y[n] = b0*x[n] + z1
 *   z1 = b1*x[n] - a1*y[n] + z2
 *   z2 = b2*x[n] - a2*y[n]
 */
typedef struct {
  int16_t b0, b1, b2; ///< Numerator coefficients (Q15)
  int16_t a1, a2;     ///< Denominator coefficients (Q15, negated)
  int32_t z1, z2;     ///< State variables (Q31 for precision)
} eif_biquad_q15_t;

/**
 * @brief Initialize Q15 biquad from float coefficients
 */
static inline void eif_biquad_q15_init_from_float(eif_biquad_q15_t *bq,
                                                  float b0, float b1, float b2,
                                                  float a1, float a2) {
  // Convert to Q15
  bq->b0 = (int16_t)(b0 * 32768.0f);
  bq->b1 = (int16_t)(b1 * 32768.0f);
  bq->b2 = (int16_t)(b2 * 32768.0f);
  bq->a1 = (int16_t)(a1 * 32768.0f);
  bq->a2 = (int16_t)(a2 * 32768.0f);
  bq->z1 = 0;
  bq->z2 = 0;
}

/**
 * @brief Reset biquad state
 */
static inline void eif_biquad_q15_reset(eif_biquad_q15_t *bq) {
  bq->z1 = 0;
  bq->z2 = 0;
}

/**
 * @brief Process single Q15 sample
 */
static inline int16_t eif_biquad_q15_process(eif_biquad_q15_t *bq,
                                             int16_t input) {
  // Compute output: y = b0*x + z1
  int32_t acc = (int32_t)bq->b0 * (int32_t)input;
  acc += bq->z1;

  // Shift and saturate output
  int32_t y32 = acc >> 15;
  if (y32 > 32767)
    y32 = 32767;
  if (y32 < -32768)
    y32 = -32768;
  int16_t output = (int16_t)y32;

  // Update state: z1 = b1*x - a1*y + z2
  bq->z1 = (int32_t)bq->b1 * (int32_t)input -
           (int32_t)bq->a1 * (int32_t)output + bq->z2;

  // z2 = b2*x - a2*y
  bq->z2 = (int32_t)bq->b2 * (int32_t)input - (int32_t)bq->a2 * (int32_t)output;

  return output;
}

/**
 * @brief Process block of Q15 samples
 */
static inline void eif_biquad_q15_process_block(eif_biquad_q15_t *bq,
                                                const int16_t *input,
                                                int16_t *output, int len) {
  for (int i = 0; i < len; i++) {
    output[i] = eif_biquad_q15_process(bq, input[i]);
  }
}

// =============================================================================
// Q15 Biquad Cascade
// =============================================================================

/**
 * @brief Q15 Biquad cascade for higher-order filters
 */
typedef struct {
  eif_biquad_q15_t stages[EIF_BIQUAD_FIXED_MAX_STAGES];
  int num_stages;
} eif_biquad_q15_cascade_t;

/**
 * @brief Initialize cascade
 */
static inline void
eif_biquad_q15_cascade_init(eif_biquad_q15_cascade_t *cascade, int num_stages) {
  cascade->num_stages = num_stages;
  for (int i = 0; i < num_stages; i++) {
    cascade->stages[i].z1 = 0;
    cascade->stages[i].z2 = 0;
  }
}

/**
 * @brief Process sample through cascade
 */
static inline int16_t
eif_biquad_q15_cascade_process(eif_biquad_q15_cascade_t *cascade,
                               int16_t input) {
  int16_t x = input;
  for (int i = 0; i < cascade->num_stages; i++) {
    x = eif_biquad_q15_process(&cascade->stages[i], x);
  }
  return x;
}

/**
 * @brief Reset cascade state
 */
static inline void
eif_biquad_q15_cascade_reset(eif_biquad_q15_cascade_t *cascade) {
  for (int i = 0; i < cascade->num_stages; i++) {
    eif_biquad_q15_reset(&cascade->stages[i]);
  }
}

// =============================================================================
// Filter Design (pre-computed for common cases)
// =============================================================================

/**
 * @brief Design 2nd order Butterworth lowpass (Q15)
 *
 * Pre-computed for common cutoff ratios (fc/fs)
 */
static inline void eif_biquad_q15_butter_lp_01(eif_biquad_q15_t *bq) {
  // fc/fs = 0.1 Butterworth lowpass
  bq->b0 = 2013; // 0.0614 * 32768
  bq->b1 = 4027; // 0.1229 * 32768
  bq->b2 = 2013;
  bq->a1 = -27876; // -0.8508 * 32768
  bq->a2 = 11862;  // 0.3621 * 32768
  bq->z1 = 0;
  bq->z2 = 0;
}

static inline void eif_biquad_q15_butter_lp_02(eif_biquad_q15_t *bq) {
  // fc/fs = 0.2 Butterworth lowpass
  bq->b0 = 5912;  // 0.1804
  bq->b1 = 11824; // 0.3608
  bq->b2 = 5912;
  bq->a1 = -10044; // -0.3066
  bq->a2 = 5624;   // 0.1717
  bq->z1 = 0;
  bq->z2 = 0;
}

/**
 * @brief Get memory usage in bytes
 */
static inline int eif_biquad_q15_memory(void) {
  return 5 * 2 + 2 * 4; // 5 Q15 coeffs + 2 Q31 states
}

static inline int eif_biquad_q15_cascade_memory(int stages) {
  return stages * eif_biquad_q15_memory();
}

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_BIQUAD_FIXED_H
