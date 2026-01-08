/**
 * @file main.c
 * @brief Audio EQ Demo (IIR Bi-Quad Filters)
 *
 * Simulates a multi-band equalizer by processing a synthetic audio signal
 * through Low-Pass and High-Pass IIR filters.
 */

#include "eif_dsp.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define SAMPLE_RATE 48000
#define NUM_SAMPLES 512

void calculate_lpf_coeffs(float f0, float Q, float32_t *b0, float32_t *b1,
                          float32_t *b2, float32_t *a1, float32_t *a2) {
  float w0 = 2 * 3.14159f * f0 / SAMPLE_RATE;
  float alpha = sinf(w0) / (2 * Q);
  float cos_w0 = cosf(w0);

  float a0 = 1 + alpha;
  *b0 = ((1 - cos_w0) / 2) / a0;
  *b1 = ((1 - cos_w0)) / a0;
  *b2 = ((1 - cos_w0) / 2) / a0;
  *a1 = (-2 * cos_w0) /
        a0; // Note: In our implementation, we subtract a1*y[n-1], so sign might
            // depend on formula eif_dsp_iir uses: y[n] = b0*x[n] ... -
            // a1*y[n-1] ... Standard difference: y[n] = (b0*x[n] ... -
            // a1*y[n-1])/a0 So if we normalize by a0, the coeffs passed to
            // update should be strictly a1/a0. Our implementation: state[0] =
            // ... - a1 * output. So yes, passed 'a1' corresponds to normalized
            // a1.
  *a2 = (1 - alpha) / a0;
}

void calculate_hpf_coeffs(float f0, float Q, float32_t *b0, float32_t *b1,
                          float32_t *b2, float32_t *a1, float32_t *a2) {
  float w0 = 2 * 3.14159f * f0 / SAMPLE_RATE;
  float alpha = sinf(w0) / (2 * Q);
  float cos_w0 = cosf(w0);

  float a0 = 1 + alpha;
  *b0 = ((1 + cos_w0) / 2) / a0;
  *b1 = (-(1 + cos_w0)) / a0;
  *b2 = ((1 + cos_w0) / 2) / a0;
  *a1 = (-2 * cos_w0) / a0;
  *a2 = (1 - alpha) / a0;
}

int main() {
  float32_t input[NUM_SAMPLES];
  float32_t out_lp[NUM_SAMPLES];
  float32_t out_hp[NUM_SAMPLES];

  // Generate signal: 100Hz (Bass) + 10kHz (Treble)
  for (int i = 0; i < NUM_SAMPLES; i++) {
    float t = (float)i / SAMPLE_RATE;
    input[i] = sinf(2 * 3.14159f * 100 * t) * 0.5f +
               sinf(2 * 3.14159f * 10000 * t) * 0.3f;
  }

  // Initialize Filters
  eif_dsp_iir_t lpf, hpf;
  float32_t b0, b1, b2, a1, a2;

  // LPF @ 500Hz
  calculate_lpf_coeffs(500.0f, 0.707f, &b0, &b1, &b2, &a1, &a2);
  eif_dsp_iir_init(&lpf, b0, b1, b2, a1, a2);

  // HPF @ 5000Hz
  calculate_hpf_coeffs(5000.0f, 0.707f, &b0, &b1, &b2, &a1, &a2);
  eif_dsp_iir_init(&hpf, b0, b1, b2, a1, a2);

  // Process
  eif_dsp_iir_process(&lpf, input, out_lp, NUM_SAMPLES);
  eif_dsp_iir_reset(
      &lpf); // Just to test reset, though not needed for next filter

  eif_dsp_iir_process(&hpf, input, out_hp, NUM_SAMPLES);

  // Output JSON
  printf("{\n");
  printf("  \"waveform\": {\n");

  printf("    \"input\": [");
  for (int i = 0; i < NUM_SAMPLES; i++)
    printf("%.4f%s", input[i], (i < NUM_SAMPLES - 1) ? ", " : "");
  printf("],\n");

  printf("    \"low_pass_500hz\": [");
  for (int i = 0; i < NUM_SAMPLES; i++)
    printf("%.4f%s", out_lp[i], (i < NUM_SAMPLES - 1) ? ", " : "");
  printf("],\n");

  printf("    \"high_pass_5khz\": [");
  for (int i = 0; i < NUM_SAMPLES; i++)
    printf("%.4f%s", out_hp[i], (i < NUM_SAMPLES - 1) ? ", " : "");
  printf("]\n");

  printf("  },\n");
  printf("  \"info\": \"Audio EQ Demo: 100Hz+10kHz signal filtered by 500Hz "
         "LPF and 5kHz HPF\"\n");
  printf("}\n");

  return 0;
}
