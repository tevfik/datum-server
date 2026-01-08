/**
 * @file eif_dsp_iir.h
 * @brief Infinite Impulse Response (IIR) Filters
 */

#ifndef EIF_DSP_IIR_H
#define EIF_DSP_IIR_H

#include "eif_status.h"
#include "eif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// IIR Filter Structures
// =============================================================================

/**
 * @brief Bi-Quad IIR Filter State
 *
 * Direct Form I or II implementation.
 * Generic form:
 * y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
 */
typedef struct {
  float32_t b0, b1, b2; // Feedforward coeffs
  float32_t a1, a2;     // Feedback coeffs

  // State (Previous inputs/outputs)
  float32_t state[2]; // x[n-1], x[n-2] or internal states depending on form
                      // For Direct Form II: w[n-1], w[n-2]
} eif_dsp_iir_t;

/**
 * @brief IIR Filter initialization
 */
void eif_dsp_iir_init(eif_dsp_iir_t *filter, float32_t b0, float32_t b1,
                      float32_t b2, float32_t a1, float32_t a2);

/**
 * @brief Reset filter state
 */
void eif_dsp_iir_reset(eif_dsp_iir_t *filter);

/**
 * @brief Process single sample (Direct Form II Transposed is numerically
 * better)
 */
float32_t eif_dsp_iir_update(eif_dsp_iir_t *filter, float32_t input);

/**
 * @brief Process block of samples
 */
void eif_dsp_iir_process(eif_dsp_iir_t *filter, const float32_t *input,
                         float32_t *output, int size);

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_IIR_H
