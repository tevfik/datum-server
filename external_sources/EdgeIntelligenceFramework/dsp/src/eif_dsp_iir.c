/**
 * @file eif_dsp_iir.c
 * @brief Infinite Impulse Response (IIR) Filters Implementation
 */

#include "eif_dsp_iir.h"
#include <string.h>

void eif_dsp_iir_init(eif_dsp_iir_t *filter, float32_t b0, float32_t b1,
                      float32_t b2, float32_t a1, float32_t a2) {
  if (!filter)
    return;

  filter->b0 = b0;
  filter->b1 = b1;
  filter->b2 = b2;
  filter->a1 = a1;
  filter->a2 = a2;

  eif_dsp_iir_reset(filter);
}

void eif_dsp_iir_reset(eif_dsp_iir_t *filter) {
  if (!filter)
    return;
  filter->state[0] = 0.0f;
  filter->state[1] = 0.0f;
}

// Direct Form II Transposed
// y[n] = b0*x[n] + s1[n-1]
// s1[n] = s2[n-1] + b1*x[n] - a1*y[n]
// s2[n] = b2*x[n] - a2*y[n]
float32_t eif_dsp_iir_update(eif_dsp_iir_t *filter, float32_t input) {
  if (!filter)
    return 0.0f;

  // y[n] = b0*x[n] + s1[n-1]
  float32_t output = filter->b0 * input + filter->state[0];

  // s1[n] = s2[n-1] + b1*x[n] - a1*y[n]
  filter->state[0] =
      filter->state[1] + filter->b1 * input - filter->a1 * output;

  // s2[n] = b2*x[n] - a2*y[n]
  filter->state[1] = filter->b2 * input - filter->a2 * output;

  return output;
}

void eif_dsp_iir_process(eif_dsp_iir_t *filter, const float32_t *input,
                         float32_t *output, int size) {
  if (!filter || !input || !output)
    return;

  for (int i = 0; i < size; i++) {
    output[i] = eif_dsp_iir_update(filter, input[i]);
  }
}
