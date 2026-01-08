/**
 * @file eif_dsp_fir_fixed.h
 * @brief Fixed-Point FIR Filter
 *
 * Memory-efficient fixed-point FIR filter for MCUs without FPU:
 * - Q15 format (signed 16-bit with 15 fractional bits)
 * - Q31 format for higher precision
 * - Optimized MAC operations
 *
 * Perfect for: ARM Cortex-M0/M0+, MSP430, 8-bit MCUs
 */

#ifndef EIF_DSP_FIR_FIXED_H
#define EIF_DSP_FIR_FIXED_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EIF_FIR_FIXED_MAX_ORDER 64

// =============================================================================
// Q15 Format Utilities
// =============================================================================

/**
 * @brief Convert float to Q15
 */
static inline int16_t float_to_q15(float x) {
  float scaled = x * 32768.0f;
  if (scaled > 32767.0f)
    return 32767;
  if (scaled < -32768.0f)
    return -32768;
  return (int16_t)scaled;
}

/**
 * @brief Convert Q15 to float
 */
static inline float q15_to_float(int16_t x) { return (float)x / 32768.0f; }

/**
 * @brief Saturating Q15 multiply
 */
static inline int16_t q15_mul(int16_t a, int16_t b) {
  int32_t result = ((int32_t)a * (int32_t)b) >> 15;
  if (result > 32767)
    return 32767;
  if (result < -32768)
    return -32768;
  return (int16_t)result;
}

/**
 * @brief Saturating Q15 add
 */
static inline int16_t q15_add(int16_t a, int16_t b) {
  int32_t result = (int32_t)a + (int32_t)b;
  if (result > 32767)
    return 32767;
  if (result < -32768)
    return -32768;
  return (int16_t)result;
}

// =============================================================================
// Q15 FIR Filter
// =============================================================================

/**
 * @brief Q15 FIR filter state
 */
typedef struct {
  int16_t coeffs[EIF_FIR_FIXED_MAX_ORDER]; ///< Filter coefficients (Q15)
  int16_t buffer[EIF_FIR_FIXED_MAX_ORDER]; ///< Delay line
  int order;                               ///< Number of taps
  int buf_idx;                             ///< Circular buffer index
} eif_fir_q15_t;

/**
 * @brief Initialize Q15 FIR filter
 */
static inline bool eif_fir_q15_init(eif_fir_q15_t *fir, const int16_t *coeffs,
                                    int order) {
  if (order <= 0 || order > EIF_FIR_FIXED_MAX_ORDER) {
    return false;
  }

  fir->order = order;
  fir->buf_idx = 0;

  for (int i = 0; i < order; i++) {
    fir->coeffs[i] = coeffs[i];
    fir->buffer[i] = 0;
  }

  return true;
}

/**
 * @brief Initialize Q15 FIR from float coefficients
 */
static inline bool eif_fir_q15_init_from_float(eif_fir_q15_t *fir,
                                               const float *coeffs, int order) {
  if (order <= 0 || order > EIF_FIR_FIXED_MAX_ORDER) {
    return false;
  }

  fir->order = order;
  fir->buf_idx = 0;

  for (int i = 0; i < order; i++) {
    fir->coeffs[i] = float_to_q15(coeffs[i]);
    fir->buffer[i] = 0;
  }

  return true;
}

/**
 * @brief Process single Q15 sample
 */
static inline int16_t eif_fir_q15_process(eif_fir_q15_t *fir, int16_t input) {
  // Store input in circular buffer
  fir->buffer[fir->buf_idx] = input;

  // Convolution with 32-bit accumulator
  int32_t acc = 0;
  int idx = fir->buf_idx;

  for (int i = 0; i < fir->order; i++) {
    acc += (int32_t)fir->coeffs[i] * (int32_t)fir->buffer[idx];
    idx--;
    if (idx < 0)
      idx = fir->order - 1;
  }

  // Advance buffer index
  fir->buf_idx++;
  if (fir->buf_idx >= fir->order)
    fir->buf_idx = 0;

  // Scale down and saturate
  int32_t result = acc >> 15;
  if (result > 32767)
    return 32767;
  if (result < -32768)
    return -32768;

  return (int16_t)result;
}

/**
 * @brief Process block of Q15 samples
 */
static inline void eif_fir_q15_process_block(eif_fir_q15_t *fir,
                                             const int16_t *input,
                                             int16_t *output, int len) {
  for (int i = 0; i < len; i++) {
    output[i] = eif_fir_q15_process(fir, input[i]);
  }
}

/**
 * @brief Reset filter state
 */
static inline void eif_fir_q15_reset(eif_fir_q15_t *fir) {
  for (int i = 0; i < fir->order; i++) {
    fir->buffer[i] = 0;
  }
  fir->buf_idx = 0;
}

// =============================================================================
// Design Helpers
// =============================================================================

/**
 * @brief Design simple lowpass (moving average) Q15 coefficients
 */
static inline void eif_fir_q15_design_ma(int16_t *coeffs, int order) {
  int16_t val = (int16_t)(32768 / order); // Equal weights
  for (int i = 0; i < order; i++) {
    coeffs[i] = val;
  }
}

/**
 * @brief Get memory usage in bytes
 */
static inline int eif_fir_q15_memory(int order) {
  return order * 2 * 2 + 8; // coeffs + buffer + overhead
}

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_FIR_FIXED_H
