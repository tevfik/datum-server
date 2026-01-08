/**
 * @file eif_quantize.h
 * @brief Model Quantization for Edge Deployment
 *
 * Utilities for converting float models to low-precision formats:
 * - INT8 symmetric/asymmetric quantization
 * - Q15 fixed-point conversion
 * - Dynamic range analysis
 * - Calibration utilities
 *
 * Critical for TinyML deployment where memory and compute are limited.
 */

#ifndef EIF_QUANTIZE_H
#define EIF_QUANTIZE_H

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Quantization Types
// =============================================================================

typedef enum {
  EIF_QUANT_NONE = 0,
  EIF_QUANT_INT8_SYMMETRIC,
  EIF_QUANT_INT8_ASYMMETRIC,
  EIF_QUANT_Q15,
  EIF_QUANT_Q7
} eif_quant_type_t;

/**
 * @brief Quantization parameters for a tensor
 */
typedef struct {
  eif_quant_type_t type;
  float scale;        ///< Scale factor: float = (int - zero_point) * scale
  int32_t zero_point; ///< Zero point for asymmetric quantization
  float min_val;      ///< Min value encountered during calibration
  float max_val;      ///< Max value encountered during calibration
} eif_quant_params_t;

/**
 * @brief Quantization statistics for calibration
 */
typedef struct {
  float min_val;
  float max_val;
  float mean;
  float variance;
  int64_t count;
} eif_quant_stats_t;

// =============================================================================
// Range Analysis & Calibration
// =============================================================================

/**
 * @brief Initialize quantization statistics
 */
static inline void eif_quant_stats_init(eif_quant_stats_t *stats) {
  stats->min_val = FLT_MAX;
  stats->max_val = -FLT_MAX;
  stats->mean = 0.0f;
  stats->variance = 0.0f;
  stats->count = 0;
}

/**
 * @brief Update statistics with new tensor values (calibration)
 */
static inline void eif_quant_stats_update(eif_quant_stats_t *stats,
                                          const float *data, int size) {
  for (int i = 0; i < size; i++) {
    float val = data[i];

    if (val < stats->min_val)
      stats->min_val = val;
    if (val > stats->max_val)
      stats->max_val = val;

    // Welford's online algorithm for mean/variance
    stats->count++;
    float delta = val - stats->mean;
    stats->mean += delta / stats->count;
    float delta2 = val - stats->mean;
    stats->variance += delta * delta2;
  }
}

/**
 * @brief Get final variance from statistics
 */
static inline float eif_quant_stats_variance(eif_quant_stats_t *stats) {
  return stats->count > 1 ? stats->variance / (stats->count - 1) : 0.0f;
}

// =============================================================================
// INT8 Symmetric Quantization
// =============================================================================

/**
 * @brief Calculate INT8 symmetric quantization parameters
 *
 * For symmetric: zero_point = 0, scale = max(|min|, |max|) / 127
 */
static inline void eif_quant_calc_int8_symmetric(eif_quant_params_t *params,
                                                 float min_val, float max_val) {
  params->type = EIF_QUANT_INT8_SYMMETRIC;
  params->zero_point = 0;
  params->min_val = min_val;
  params->max_val = max_val;

  float abs_max = fmaxf(fabsf(min_val), fabsf(max_val));
  params->scale = abs_max / 127.0f;

  // Prevent zero scale
  if (params->scale < 1e-10f)
    params->scale = 1e-10f;
}

/**
 * @brief Quantize float array to INT8 symmetric
 */
static inline void eif_quant_to_int8_sym(const float *input, int8_t *output,
                                         int size,
                                         const eif_quant_params_t *params) {
  float inv_scale = 1.0f / params->scale;

  for (int i = 0; i < size; i++) {
    float scaled = input[i] * inv_scale;

    // Round and clamp
    int32_t val = (int32_t)roundf(scaled);
    if (val > 127)
      val = 127;
    if (val < -128)
      val = -128;

    output[i] = (int8_t)val;
  }
}

/**
 * @brief Dequantize INT8 symmetric to float
 */
static inline void eif_dequant_int8_sym(const int8_t *input, float *output,
                                        int size,
                                        const eif_quant_params_t *params) {
  for (int i = 0; i < size; i++) {
    output[i] = (float)input[i] * params->scale;
  }
}

// =============================================================================
// INT8 Asymmetric Quantization
// =============================================================================

/**
 * @brief Calculate INT8 asymmetric quantization parameters
 *
 * For asymmetric: full range mapping with zero_point
 */
static inline void eif_quant_calc_int8_asymmetric(eif_quant_params_t *params,
                                                  float min_val,
                                                  float max_val) {
  params->type = EIF_QUANT_INT8_ASYMMETRIC;
  params->min_val = min_val;
  params->max_val = max_val;

  // Scale covers full range
  params->scale = (max_val - min_val) / 255.0f;
  if (params->scale < 1e-10f)
    params->scale = 1e-10f;

  // Zero point maps min_val to -128
  params->zero_point = (int32_t)roundf(-128.0f - min_val / params->scale);

  // Clamp zero point
  if (params->zero_point < -128)
    params->zero_point = -128;
  if (params->zero_point > 127)
    params->zero_point = 127;
}

/**
 * @brief Quantize float array to INT8 asymmetric
 */
static inline void eif_quant_to_int8_asym(const float *input, int8_t *output,
                                          int size,
                                          const eif_quant_params_t *params) {
  float inv_scale = 1.0f / params->scale;

  for (int i = 0; i < size; i++) {
    int32_t val = (int32_t)roundf(input[i] * inv_scale) + params->zero_point;

    if (val > 127)
      val = 127;
    if (val < -128)
      val = -128;

    output[i] = (int8_t)val;
  }
}

/**
 * @brief Dequantize INT8 asymmetric to float
 */
static inline void eif_dequant_int8_asym(const int8_t *input, float *output,
                                         int size,
                                         const eif_quant_params_t *params) {
  for (int i = 0; i < size; i++) {
    output[i] = ((float)input[i] - params->zero_point) * params->scale;
  }
}

// =============================================================================
// Q15 Fixed-Point Quantization
// =============================================================================

/**
 * @brief Calculate Q15 quantization parameters
 */
static inline void eif_quant_calc_q15(eif_quant_params_t *params, float min_val,
                                      float max_val) {
  params->type = EIF_QUANT_Q15;
  params->zero_point = 0;
  params->min_val = min_val;
  params->max_val = max_val;

  float abs_max = fmaxf(fabsf(min_val), fabsf(max_val));
  params->scale = abs_max / 32767.0f;

  if (params->scale < 1e-10f)
    params->scale = 1e-10f;
}

/**
 * @brief Quantize float array to Q15
 */
static inline void eif_quant_to_q15(const float *input, int16_t *output,
                                    int size,
                                    const eif_quant_params_t *params) {
  float inv_scale = 1.0f / params->scale;

  for (int i = 0; i < size; i++) {
    int32_t val = (int32_t)roundf(input[i] * inv_scale);

    if (val > 32767)
      val = 32767;
    if (val < -32768)
      val = -32768;

    output[i] = (int16_t)val;
  }
}

/**
 * @brief Dequantize Q15 to float
 */
static inline void eif_dequant_q15(const int16_t *input, float *output,
                                   int size, const eif_quant_params_t *params) {
  for (int i = 0; i < size; i++) {
    output[i] = (float)input[i] * params->scale;
  }
}

// =============================================================================
// Quantization Error Analysis
// =============================================================================

/**
 * @brief Calculate mean squared quantization error
 */
static inline float eif_quant_mse(const float *original,
                                  const float *dequantized, int size) {
  float mse = 0.0f;
  for (int i = 0; i < size; i++) {
    float diff = original[i] - dequantized[i];
    mse += diff * diff;
  }
  return mse / size;
}

/**
 * @brief Calculate signal-to-quantization-noise ratio (SQNR) in dB
 */
static inline float eif_quant_sqnr(const float *original,
                                   const float *dequantized, int size) {
  float signal_power = 0.0f;
  float noise_power = 0.0f;

  for (int i = 0; i < size; i++) {
    signal_power += original[i] * original[i];
    float diff = original[i] - dequantized[i];
    noise_power += diff * diff;
  }

  if (noise_power < 1e-10f)
    return 100.0f; // Essentially perfect

  return 10.0f * log10f(signal_power / noise_power);
}

/**
 * @brief Get bits per element for quantization type
 */
static inline int eif_quant_bits(eif_quant_type_t type) {
  switch (type) {
  case EIF_QUANT_INT8_SYMMETRIC:
  case EIF_QUANT_INT8_ASYMMETRIC:
  case EIF_QUANT_Q7:
    return 8;
  case EIF_QUANT_Q15:
    return 16;
  default:
    return 32;
  }
}

/**
 * @brief Calculate memory savings ratio
 */
static inline float eif_quant_memory_ratio(eif_quant_type_t type) {
  return 32.0f / eif_quant_bits(type);
}

#ifdef __cplusplus
}
#endif

#endif // EIF_QUANTIZE_H
