/**
 * @file eif_dsp_biquad.h
 * @brief Biquad Filter Cascade Architecture
 *
 * Provides professional-quality biquad filter implementation:
 * - Single biquad section (2nd order IIR)
 * - Cascadable multi-stage design
 * - Audio-specific filter types (parametric EQ, shelving)
 *
 * Standard topology for audio equalizers and pro audio.
 */

#ifndef EIF_DSP_BIQUAD_H
#define EIF_DSP_BIQUAD_H

#include <math.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Maximum cascade stages
#define EIF_BIQUAD_MAX_STAGES 8

/**
 * @brief Single biquad section (Direct Form II Transposed)
 *
 * Transfer function:
 *   H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
 */
typedef struct {
  float b0, b1, b2; ///< Numerator coefficients
  float a1, a2;     ///< Denominator coefficients (a0 = 1)
  float z1, z2;     ///< State variables
} eif_biquad_t;

/**
 * @brief Multi-stage biquad cascade
 */
typedef struct {
  eif_biquad_t stages[EIF_BIQUAD_MAX_STAGES];
  int num_stages;
  float gain; ///< Overall gain
} eif_biquad_cascade_t;

// =============================================================================
// Core Processing
// =============================================================================

/**
 * @brief Process single sample through biquad
 */
static inline float eif_biquad_process(eif_biquad_t *bq, float input) {
  float output = bq->b0 * input + bq->z1;
  bq->z1 = bq->b1 * input - bq->a1 * output + bq->z2;
  bq->z2 = bq->b2 * input - bq->a2 * output;
  return output;
}

/**
 * @brief Process through cascade
 */
static inline float eif_biquad_cascade_process(eif_biquad_cascade_t *cascade,
                                               float input) {
  float x = input * cascade->gain;
  for (int i = 0; i < cascade->num_stages; i++) {
    x = eif_biquad_process(&cascade->stages[i], x);
  }
  return x;
}

/**
 * @brief Reset biquad state
 */
static inline void eif_biquad_reset(eif_biquad_t *bq) {
  bq->z1 = bq->z2 = 0.0f;
}

/**
 * @brief Reset cascade state
 */
static inline void eif_biquad_cascade_reset(eif_biquad_cascade_t *cascade) {
  for (int i = 0; i < cascade->num_stages; i++) {
    eif_biquad_reset(&cascade->stages[i]);
  }
}

// =============================================================================
// Filter Design Functions
// =============================================================================

/**
 * @brief Initialize biquad as lowpass filter
 * @param bq Biquad state
 * @param fc Cutoff frequency (Hz)
 * @param fs Sample rate (Hz)
 * @param Q Quality factor (0.707 for Butterworth)
 */
static inline void eif_biquad_lowpass(eif_biquad_t *bq, float fc, float fs,
                                      float Q) {
  float w0 = 2.0f * M_PI * fc / fs;
  float cos_w0 = cosf(w0);
  float sin_w0 = sinf(w0);
  float alpha = sin_w0 / (2.0f * Q);

  float a0 = 1.0f + alpha;
  bq->b0 = ((1.0f - cos_w0) / 2.0f) / a0;
  bq->b1 = (1.0f - cos_w0) / a0;
  bq->b2 = bq->b0;
  bq->a1 = (-2.0f * cos_w0) / a0;
  bq->a2 = (1.0f - alpha) / a0;
  bq->z1 = bq->z2 = 0.0f;
}

/**
 * @brief Initialize biquad as highpass filter
 */
static inline void eif_biquad_highpass(eif_biquad_t *bq, float fc, float fs,
                                       float Q) {
  float w0 = 2.0f * M_PI * fc / fs;
  float cos_w0 = cosf(w0);
  float sin_w0 = sinf(w0);
  float alpha = sin_w0 / (2.0f * Q);

  float a0 = 1.0f + alpha;
  bq->b0 = ((1.0f + cos_w0) / 2.0f) / a0;
  bq->b1 = (-(1.0f + cos_w0)) / a0;
  bq->b2 = bq->b0;
  bq->a1 = (-2.0f * cos_w0) / a0;
  bq->a2 = (1.0f - alpha) / a0;
  bq->z1 = bq->z2 = 0.0f;
}

/**
 * @brief Initialize biquad as bandpass filter
 */
static inline void eif_biquad_bandpass(eif_biquad_t *bq, float fc, float fs,
                                       float Q) {
  float w0 = 2.0f * M_PI * fc / fs;
  float cos_w0 = cosf(w0);
  float sin_w0 = sinf(w0);
  float alpha = sin_w0 / (2.0f * Q);

  float a0 = 1.0f + alpha;
  bq->b0 = alpha / a0;
  bq->b1 = 0.0f;
  bq->b2 = -alpha / a0;
  bq->a1 = (-2.0f * cos_w0) / a0;
  bq->a2 = (1.0f - alpha) / a0;
  bq->z1 = bq->z2 = 0.0f;
}

/**
 * @brief Initialize biquad as notch (band-reject) filter
 */
static inline void eif_biquad_notch(eif_biquad_t *bq, float fc, float fs,
                                    float Q) {
  float w0 = 2.0f * M_PI * fc / fs;
  float cos_w0 = cosf(w0);
  float sin_w0 = sinf(w0);
  float alpha = sin_w0 / (2.0f * Q);

  float a0 = 1.0f + alpha;
  bq->b0 = 1.0f / a0;
  bq->b1 = (-2.0f * cos_w0) / a0;
  bq->b2 = 1.0f / a0;
  bq->a1 = (-2.0f * cos_w0) / a0;
  bq->a2 = (1.0f - alpha) / a0;
  bq->z1 = bq->z2 = 0.0f;
}

/**
 * @brief Initialize biquad as peaking EQ filter
 * @param gain_db Gain in dB (positive = boost, negative = cut)
 */
static inline void eif_biquad_peaking(eif_biquad_t *bq, float fc, float fs,
                                      float Q, float gain_db) {
  float A = sqrtf(powf(10.0f, gain_db / 20.0f));
  float w0 = 2.0f * M_PI * fc / fs;
  float cos_w0 = cosf(w0);
  float sin_w0 = sinf(w0);
  float alpha = sin_w0 / (2.0f * Q);

  float a0 = 1.0f + alpha / A;
  bq->b0 = (1.0f + alpha * A) / a0;
  bq->b1 = (-2.0f * cos_w0) / a0;
  bq->b2 = (1.0f - alpha * A) / a0;
  bq->a1 = (-2.0f * cos_w0) / a0;
  bq->a2 = (1.0f - alpha / A) / a0;
  bq->z1 = bq->z2 = 0.0f;
}

/**
 * @brief Initialize biquad as low shelf filter
 */
static inline void eif_biquad_lowshelf(eif_biquad_t *bq, float fc, float fs,
                                       float gain_db, float S) {
  float A = sqrtf(powf(10.0f, gain_db / 20.0f));
  float w0 = 2.0f * M_PI * fc / fs;
  float cos_w0 = cosf(w0);
  float sin_w0 = sinf(w0);
  float alpha =
      sin_w0 / 2.0f * sqrtf((A + 1.0f / A) * (1.0f / S - 1.0f) + 2.0f);
  float sqrtA = sqrtf(A);

  float a0 = (A + 1.0f) + (A - 1.0f) * cos_w0 + 2.0f * sqrtA * alpha;
  bq->b0 = (A * ((A + 1.0f) - (A - 1.0f) * cos_w0 + 2.0f * sqrtA * alpha)) / a0;
  bq->b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_w0)) / a0;
  bq->b2 = (A * ((A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * sqrtA * alpha)) / a0;
  bq->a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cos_w0)) / a0;
  bq->a2 = ((A + 1.0f) + (A - 1.0f) * cos_w0 - 2.0f * sqrtA * alpha) / a0;
  bq->z1 = bq->z2 = 0.0f;
}

/**
 * @brief Initialize biquad as high shelf filter
 */
static inline void eif_biquad_highshelf(eif_biquad_t *bq, float fc, float fs,
                                        float gain_db, float S) {
  float A = sqrtf(powf(10.0f, gain_db / 20.0f));
  float w0 = 2.0f * M_PI * fc / fs;
  float cos_w0 = cosf(w0);
  float sin_w0 = sinf(w0);
  float alpha =
      sin_w0 / 2.0f * sqrtf((A + 1.0f / A) * (1.0f / S - 1.0f) + 2.0f);
  float sqrtA = sqrtf(A);

  float a0 = (A + 1.0f) - (A - 1.0f) * cos_w0 + 2.0f * sqrtA * alpha;
  bq->b0 = (A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + 2.0f * sqrtA * alpha)) / a0;
  bq->b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0)) / a0;
  bq->b2 = (A * ((A + 1.0f) + (A - 1.0f) * cos_w0 - 2.0f * sqrtA * alpha)) / a0;
  bq->a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0)) / a0;
  bq->a2 = ((A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * sqrtA * alpha) / a0;
  bq->z1 = bq->z2 = 0.0f;
}

/**
 * @brief Initialize biquad as allpass filter
 */
static inline void eif_biquad_allpass(eif_biquad_t *bq, float fc, float fs,
                                      float Q) {
  float w0 = 2.0f * M_PI * fc / fs;
  float cos_w0 = cosf(w0);
  float sin_w0 = sinf(w0);
  float alpha = sin_w0 / (2.0f * Q);

  float a0 = 1.0f + alpha;
  bq->b0 = (1.0f - alpha) / a0;
  bq->b1 = (-2.0f * cos_w0) / a0;
  bq->b2 = (1.0f + alpha) / a0;
  bq->a1 = (-2.0f * cos_w0) / a0;
  bq->a2 = (1.0f - alpha) / a0;
  bq->z1 = bq->z2 = 0.0f;
}

// =============================================================================
// Cascade Helpers
// =============================================================================

/**
 * @brief Initialize single-stage cascade
 */
static inline void eif_biquad_cascade_init(eif_biquad_cascade_t *cascade,
                                           int num_stages) {
  cascade->num_stages =
      (num_stages > EIF_BIQUAD_MAX_STAGES) ? EIF_BIQUAD_MAX_STAGES : num_stages;
  cascade->gain = 1.0f;
  eif_biquad_cascade_reset(cascade);
}

/**
 * @brief Create 4th order Butterworth lowpass (2 biquad stages)
 */
static inline void eif_biquad_butter4_lowpass(eif_biquad_cascade_t *cascade,
                                              float fc, float fs) {
  cascade->num_stages = 2;
  cascade->gain = 1.0f;

  // Butterworth Q values for 4th order
  eif_biquad_lowpass(&cascade->stages[0], fc, fs, 0.5412f); // Q1
  eif_biquad_lowpass(&cascade->stages[1], fc, fs, 1.3065f); // Q2
}

/**
 * @brief Create 6th order Butterworth lowpass (3 biquad stages)
 */
static inline void eif_biquad_butter6_lowpass(eif_biquad_cascade_t *cascade,
                                              float fc, float fs) {
  cascade->num_stages = 3;
  cascade->gain = 1.0f;

  eif_biquad_lowpass(&cascade->stages[0], fc, fs, 0.5176f);
  eif_biquad_lowpass(&cascade->stages[1], fc, fs, 0.7071f);
  eif_biquad_lowpass(&cascade->stages[2], fc, fs, 1.9319f);
}

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_BIQUAD_H
