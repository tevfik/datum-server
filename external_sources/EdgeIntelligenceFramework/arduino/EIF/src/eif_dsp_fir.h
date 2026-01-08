/**
 * @file eif_dsp_fir.h
 * @brief Finite Impulse Response (FIR) Filters
 *
 * Provides lightweight FIR filter implementation:
 * - Arbitrary order FIR filtering
 * - Windowed-sinc filter design
 * - Block processing support
 *
 * All implementations are optimized for embedded systems.
 */

#ifndef EIF_DSP_FIR_H
#define EIF_DSP_FIR_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Maximum FIR order (memory constraint for embedded)
#define EIF_FIR_MAX_ORDER 64

/**
 * @brief FIR filter state
 */
typedef struct {
  float coeffs[EIF_FIR_MAX_ORDER]; ///< Filter coefficients
  float buffer[EIF_FIR_MAX_ORDER]; ///< Delay line buffer
  int order;                       ///< Filter order (num coeffs)
  int buf_idx;                     ///< Current buffer index
} eif_fir_t;

/**
 * @brief Initialize FIR filter with coefficients
 * @param fir Filter state
 * @param coeffs Coefficient array
 * @param order Number of coefficients
 * @return true on success
 */
static inline bool eif_fir_init(eif_fir_t *fir, const float *coeffs,
                                int order) {
  if (order <= 0 || order > EIF_FIR_MAX_ORDER) {
    return false;
  }

  fir->order = order;
  fir->buf_idx = 0;

  // Copy coefficients
  for (int i = 0; i < order; i++) {
    fir->coeffs[i] = coeffs[i];
    fir->buffer[i] = 0.0f;
  }

  return true;
}

/**
 * @brief Process single sample through FIR filter
 * @param fir Filter state
 * @param input Input sample
 * @return Filtered output
 */
static inline float eif_fir_process(eif_fir_t *fir, float input) {
  // Store input in circular buffer
  fir->buffer[fir->buf_idx] = input;

  // Convolution
  float output = 0.0f;
  int idx = fir->buf_idx;

  for (int i = 0; i < fir->order; i++) {
    output += fir->coeffs[i] * fir->buffer[idx];
    idx--;
    if (idx < 0)
      idx = fir->order - 1;
  }

  // Advance buffer index
  fir->buf_idx++;
  if (fir->buf_idx >= fir->order)
    fir->buf_idx = 0;

  return output;
}

/**
 * @brief Process block of samples
 * @param fir Filter state
 * @param input Input buffer
 * @param output Output buffer
 * @param len Number of samples
 */
static inline void eif_fir_process_block(eif_fir_t *fir, const float *input,
                                         float *output, int len) {
  for (int i = 0; i < len; i++) {
    output[i] = eif_fir_process(fir, input[i]);
  }
}

/**
 * @brief Reset FIR filter state
 */
static inline void eif_fir_reset(eif_fir_t *fir) {
  for (int i = 0; i < fir->order; i++) {
    fir->buffer[i] = 0.0f;
  }
  fir->buf_idx = 0;
}

/**
 * @brief Apply window function to coefficients
 */
typedef enum {
  EIF_WINDOW_RECTANGULAR,
  EIF_WINDOW_HAMMING,
  EIF_WINDOW_HANNING,
  EIF_WINDOW_BLACKMAN
} eif_window_t;

/**
 * @brief Get window value
 */
static inline float eif_window_value(eif_window_t type, int n, int N) {
  float x = (float)n / (float)(N - 1);

  switch (type) {
  case EIF_WINDOW_HAMMING:
    return 0.54f - 0.46f * cosf(2.0f * M_PI * x);
  case EIF_WINDOW_HANNING:
    return 0.5f * (1.0f - cosf(2.0f * M_PI * x));
  case EIF_WINDOW_BLACKMAN:
    return 0.42f - 0.5f * cosf(2.0f * M_PI * x) + 0.08f * cosf(4.0f * M_PI * x);
  case EIF_WINDOW_RECTANGULAR:
  default:
    return 1.0f;
  }
}

/**
 * @brief Design lowpass FIR filter using windowed sinc
 * @param fir Filter state to initialize
 * @param cutoff Normalized cutoff frequency (0-0.5)
 * @param order Filter order (must be odd for symmetric)
 * @param window Window type
 */
static inline bool eif_fir_design_lowpass(eif_fir_t *fir, float cutoff,
                                          int order, eif_window_t window) {
  if (order <= 0 || order > EIF_FIR_MAX_ORDER) {
    return false;
  }

  float coeffs[EIF_FIR_MAX_ORDER];
  int M = order - 1;
  float fc = cutoff;

  for (int n = 0; n <= M; n++) {
    float x = n - M / 2.0f;
    float h;

    if (fabsf(x) < 1e-6f) {
      h = 2.0f * fc;
    } else {
      h = sinf(2.0f * M_PI * fc * x) / (M_PI * x);
    }

    coeffs[n] = h * eif_window_value(window, n, order);
  }

  // Normalize for unity gain at DC
  float sum = 0.0f;
  for (int i = 0; i < order; i++)
    sum += coeffs[i];
  for (int i = 0; i < order; i++)
    coeffs[i] /= sum;

  return eif_fir_init(fir, coeffs, order);
}

/**
 * @brief Design highpass FIR filter
 */
static inline bool eif_fir_design_highpass(eif_fir_t *fir, float cutoff,
                                           int order, eif_window_t window) {
  if (!eif_fir_design_lowpass(fir, cutoff, order, window)) {
    return false;
  }

  // Spectral inversion
  for (int i = 0; i < order; i++) {
    fir->coeffs[i] = -fir->coeffs[i];
  }
  fir->coeffs[order / 2] += 1.0f;

  return true;
}

/**
 * @brief Design bandpass FIR filter
 */
static inline bool eif_fir_design_bandpass(eif_fir_t *fir, float low_cutoff,
                                           float high_cutoff, int order,
                                           eif_window_t window) {
  eif_fir_t lpf, hpf;

  if (!eif_fir_design_lowpass(&lpf, high_cutoff, order, window))
    return false;
  if (!eif_fir_design_highpass(&hpf, low_cutoff, order, window))
    return false;

  // Combine: BPF = LPF - (LPF - HPF)
  float coeffs[EIF_FIR_MAX_ORDER];
  for (int i = 0; i < order; i++) {
    coeffs[i] = lpf.coeffs[i] * hpf.coeffs[i]; // Simplified combination
  }

  return eif_fir_init(fir, coeffs, order);
}

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_FIR_H
