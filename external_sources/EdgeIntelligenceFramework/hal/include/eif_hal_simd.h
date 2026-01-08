/**
 * @file eif_hal_simd.h
 * @brief EIF Hardware SIMD Abstraction Layer
 *
 * Platform-specific SIMD implementations:
 * - ESP32-S3: ESP-NN library
 * - ARM Cortex-M55: Helium (MVE)
 * - ARM Cortex-A: NEON
 * - Generic: Portable C fallback
 *
 * Usage:
 *   // Same API works on all platforms
 *   float result = eif_simd_dot_f32(a, b, n);
 *   eif_simd_conv2d_f32(...);
 */

#ifndef EIF_HAL_SIMD_H
#define EIF_HAL_SIMD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Platform Detection
// =============================================================================

// ESP32-S3 with ESP-NN
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32S3)
#define EIF_SIMD_ESP_NN 1
#if __has_include("esp_nn.h")
#include "esp_nn.h"
#define EIF_ESP_NN_AVAILABLE 1
#else
#define EIF_ESP_NN_AVAILABLE 0
#endif

// ARM Cortex-M55 Helium (M-Profile Vector Extension)
#elif defined(__ARM_FEATURE_MVE) && __ARM_FEATURE_MVE
#define EIF_SIMD_HELIUM 1
#include <arm_mve.h>

// ARM Cortex-A NEON
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#define EIF_SIMD_NEON 1
#include <arm_neon.h>

// Generic fallback
#else
#define EIF_SIMD_GENERIC 1
#endif

// =============================================================================
// Type Definitions
// =============================================================================

typedef float float32_t;
typedef int8_t q7_t;
typedef int16_t q15_t;

// =============================================================================
// Platform Info
// =============================================================================

/**
 * @brief Get SIMD platform name
 */
static inline const char *eif_simd_get_platform(void) {
#if defined(EIF_SIMD_ESP_NN)
#if EIF_ESP_NN_AVAILABLE
  return "ESP32-S3 ESP-NN";
#else
  return "ESP32-S3 (ESP-NN not found, using generic)";
#endif
#elif defined(EIF_SIMD_HELIUM)
  return "ARM Helium (MVE)";
#elif defined(EIF_SIMD_NEON)
  return "ARM NEON";
#else
  return "Generic C";
#endif
}

/**
 * @brief Check if hardware acceleration is available
 */
static inline bool eif_simd_is_accelerated(void) {
#if defined(EIF_SIMD_ESP_NN) && EIF_ESP_NN_AVAILABLE
  return true;
#elif defined(EIF_SIMD_HELIUM) || defined(EIF_SIMD_NEON)
  return true;
#else
  return false;
#endif
}

// =============================================================================
// Dot Product (Float32)
// =============================================================================

/**
 * @brief Vectorized dot product for float32
 * @param a First vector [n]
 * @param b Second vector [n]
 * @param n Vector length
 * @return Dot product sum
 */
static inline float32_t eif_simd_dot_f32(const float32_t *a, const float32_t *b,
                                         int n) {
#if defined(EIF_SIMD_ESP_NN) && EIF_ESP_NN_AVAILABLE
  // ESP-NN optimized (uses ESP32-S3 vector instructions)
  float32_t result = 0.0f;
  esp_nn_dot_prod_f32(a, b, n, &result);
  return result;

#elif defined(EIF_SIMD_NEON)
  // ARM NEON vectorized
  float32x4_t sum_vec = vdupq_n_f32(0.0f);
  int i = 0;

  // Process 4 elements at a time
  for (; i <= n - 4; i += 4) {
    float32x4_t va = vld1q_f32(a + i);
    float32x4_t vb = vld1q_f32(b + i);
    sum_vec = vmlaq_f32(sum_vec, va, vb);
  }

  // Horizontal add (sum all 4 lanes)
  float32_t result = vaddvq_f32(sum_vec);

  // Handle remainder
  for (; i < n; i++) {
    result += a[i] * b[i];
  }
  return result;

#elif defined(EIF_SIMD_HELIUM)
  // ARM Helium (Cortex-M55)
  float32_t result = 0.0f;
  int i = 0;

  for (; i <= n - 4; i += 4) {
    mve_pred16_t p = vctp32q(n - i);
    float32x4_t va = vldrwq_z_f32(a + i, p);
    float32x4_t vb = vldrwq_z_f32(b + i, p);
    result += vaddvq_p_f32(vmulq_f32(va, vb), p);
  }
  return result;

#else
  // Generic C fallback
  float32_t result = 0.0f;
  for (int i = 0; i < n; i++) {
    result += a[i] * b[i];
  }
  return result;
#endif
}

/**
 * @brief Vectorized Squared Euclidean Distance
 * Sum of (a[i] - b[i])^2
 * @return Sum of squared differences
 */
static inline float32_t eif_simd_dist_sq_f32(const float32_t *a, const float32_t *b, int n) {
#if defined(EIF_SIMD_NEON)
  float32x4_t sum_vec = vdupq_n_f32(0.0f);
  int i = 0;
  for (; i <= n - 4; i += 4) {
    float32x4_t va = vld1q_f32(a + i);
    float32x4_t vb = vld1q_f32(b + i);
    float32x4_t diff = vsubq_f32(va, vb);
    sum_vec = vmlaq_f32(sum_vec, diff, diff);
  }
  float32_t result = vaddvq_f32(sum_vec);
  for (; i < n; i++) {
    float32_t diff = a[i] - b[i];
    result += diff * diff;
  }
  return result;

#elif defined(EIF_SIMD_HELIUM)
  float32_t result = 0.0f;
  int i = 0;
  for (; i <= n - 4; i += 4) {
    mve_pred16_t p = vctp32q(n - i);
    float32x4_t va = vldrwq_z_f32(a + i, p);
    float32x4_t vb = vldrwq_z_f32(b + i, p);
    float32x4_t diff = vsubq_f32(va, vb);
    result += vaddvq_p_f32(vmulq_f32(diff, diff), p);
  }
  return result;

#else
  // Generic unrolled loop
  float32_t result = 0.0f;
  int i = 0;
  // Unroll 4x
  for (; i <= n - 4; i += 4) {
    float32_t d0 = a[i] - b[i];
    float32_t d1 = a[i+1] - b[i+1];
    float32_t d2 = a[i+2] - b[i+2];
    float32_t d3 = a[i+3] - b[i+3];
    result += d0*d0 + d1*d1 + d2*d2 + d3*d3;
  }
  for (; i < n; i++) {
    float32_t diff = a[i] - b[i];
    result += diff * diff;
  }
  return result;
#endif
}

/**
 * @brief Vectorized Vector Add
 * out[i] = a[i] + b[i]
 */
static inline void eif_simd_add_f32(const float32_t *a, const float32_t *b, float32_t *out, int n) {
#if defined(EIF_SIMD_NEON)
  int i = 0;
  for (; i <= n - 4; i += 4) {
    float32x4_t va = vld1q_f32(a + i);
    float32x4_t vb = vld1q_f32(b + i);
    vst1q_f32(out + i, vaddq_f32(va, vb));
  }
  for (; i < n; i++) out[i] = a[i] + b[i];
#elif defined(EIF_SIMD_HELIUM)
  int i = 0;
  for (; i <= n - 4; i += 4) {
      mve_pred16_t p = vctp32q(n - i);
      float32x4_t va = vldrwq_z_f32(a + i, p);
      float32x4_t vb = vldrwq_z_f32(b + i, p);
      vstrwq_p_f32(out + i, vaddq_f32(va, vb), p);
  }
#else
  for (int i = 0; i < n; i++) out[i] = a[i] + b[i];
#endif
}

/**
 * @brief Vectorized Vector Add (UInt32)
 * out[i] = a[i] + b[i]
 */
static inline void eif_simd_add_u32(const uint32_t *a, const uint32_t *b, uint32_t *out, int n) {
#if defined(EIF_SIMD_NEON)
  int i = 0;
  for (; i <= n - 4; i += 4) {
    uint32x4_t va = vld1q_u32(a + i);
    uint32x4_t vb = vld1q_u32(b + i);
    vst1q_u32(out + i, vaddq_u32(va, vb));
  }
  for (; i < n; i++) out[i] = a[i] + b[i];
#else
  for (int i = 0; i < n; i++) out[i] = a[i] + b[i];
#endif
}


// =============================================================================

/**
 * @brief Vectorized dot product for int8 (Q7)
 * @return Accumulated result (int32)
 */
static inline int32_t eif_simd_dot_q7(const q7_t *a, const q7_t *b, int n) {
#if defined(EIF_SIMD_ESP_NN) && EIF_ESP_NN_AVAILABLE
  int32_t result = 0;
  // ESP-NN uses different signature - may need adjustment
  for (int i = 0; i < n; i++) {
    result += (int32_t)a[i] * (int32_t)b[i];
  }
  return result;

#elif defined(EIF_SIMD_NEON)
  int32x4_t sum = vdupq_n_s32(0);
  int i = 0;

  // Process 8 elements at a time
  for (; i <= n - 8; i += 8) {
    int8x8_t va = vld1_s8(a + i);
    int8x8_t vb = vld1_s8(b + i);
    int16x8_t prod = vmull_s8(va, vb);
    sum = vpadalq_s16(sum, prod);
  }

  int32_t result = vaddvq_s32(sum);

  // Remainder
  for (; i < n; i++) {
    result += (int32_t)a[i] * (int32_t)b[i];
  }
  return result;

#else
  // Generic fallback
  int32_t result = 0;
  for (int i = 0; i < n; i++) {
    result += (int32_t)a[i] * (int32_t)b[i];
  }
  return result;
#endif
}

// =============================================================================
// Vector Operations
// =============================================================================

/**
 * @brief Vectorized ReLU activation
 */
static inline void eif_simd_relu_f32(float32_t *data, int n) {
#if defined(EIF_SIMD_NEON)
  float32x4_t zero = vdupq_n_f32(0.0f);
  int i = 0;

  for (; i <= n - 4; i += 4) {
    float32x4_t v = vld1q_f32(data + i);
    v = vmaxq_f32(v, zero);
    vst1q_f32(data + i, v);
  }

  for (; i < n; i++) {
    if (data[i] < 0.0f)
      data[i] = 0.0f;
  }
#else
  for (int i = 0; i < n; i++) {
    if (data[i] < 0.0f)
      data[i] = 0.0f;
  }
#endif
}

/**
 * @brief Vectorized vector scale: b = a * scale
 */
static inline void eif_simd_scale_f32(const float32_t *a, float32_t scale,
                                      float32_t *b, int n) {
#if defined(EIF_SIMD_NEON)
  float32x4_t s = vdupq_n_f32(scale);
  int i = 0;
  for (; i <= n - 4; i += 4) {
    float32x4_t va = vld1q_f32(a + i);
    vst1q_f32(b + i, vmulq_f32(va, s));
  }
  for (; i < n; i++) {
    b[i] = a[i] * scale;
  }
#else
  for (int i = 0; i < n; i++) {
    b[i] = a[i] * scale;
  }
#endif
}

// =============================================================================
// Convolution Helpers
// =============================================================================

/**
 * @brief Compute single conv2d output pixel using SIMD dot product
 * @param input_patch Flattened input patch [k*k*c]
 * @param filter Flattened filter [k*k*c]
 * @param bias Bias value (or 0)
 * @param patch_size k*k*c
 * @return Single output value
 */
static inline float32_t eif_simd_conv2d_pixel_f32(const float32_t *input_patch,
                                                  const float32_t *filter,
                                                  float32_t bias,
                                                  int patch_size) {
  return eif_simd_dot_f32(input_patch, filter, patch_size) + bias;
}

// =============================================================================
// Matrix Operations
// =============================================================================

/**
 * @brief Vectorized matrix-vector multiply: y = A * x
 * @param A Matrix [m x n] row-major
 * @param x Vector [n]
 * @param y Output vector [m]
 */
static inline void eif_simd_matvec_f32(const float32_t *A, const float32_t *x,
                                       float32_t *y, int m, int n) {
  for (int i = 0; i < m; i++) {
    y[i] = eif_simd_dot_f32(A + i * n, x, n);
  }
}

// =============================================================================
// Fixed-Point Helpers (Q15)
// =============================================================================

/**
 * @brief Vectorized Squared Euclidean Distance (Q15)
 * Sum of (a[i] - b[i])^2.
 * Returns q31_t to prevent overflow during accumulation.
 */
static inline int32_t eif_simd_dist_sq_q15(const q15_t *a, const q15_t *b, int n) {
#if defined(EIF_SIMD_NEON)
  int32x4_t sum_vec = vdupq_n_s32(0);
  int i = 0;
  for (; i <= n - 8; i += 8) {
    int16x8_t va = vld1q_s16(a + i);
    int16x8_t vb = vld1q_s16(b + i);
    int16x8_t diff = vsubq_s16(va, vb);
    // vmull_s16 -> results in int32x4_t (low and high parts)
    int32x4_t sq_lo = vmull_s16(vget_low_s16(diff), vget_low_s16(diff));
    int32x4_t sq_hi = vmull_s16(vget_high_s16(diff), vget_high_s16(diff));
    sum_vec = vaddq_s32(sum_vec, sq_lo);
    sum_vec = vaddq_s32(sum_vec, sq_hi);
  }
  int32_t result = vaddvq_s32(sum_vec);
  for (; i < n; i++) {
    int32_t diff = (int32_t)a[i] - b[i];
    result += diff * diff;
  }
  return result;
#else
  int32_t result = 0;
  int i = 0;
  for (; i <= n - 4; i += 4) {
      int32_t d0 = (int32_t)a[i] - b[i];
      int32_t d1 = (int32_t)a[i+1] - b[i+1];
      int32_t d2 = (int32_t)a[i+2] - b[i+2];
      int32_t d3 = (int32_t)a[i+3] - b[i+3];
      result += d0*d0 + d1*d1 + d2*d2 + d3*d3;
  }
  for (; i < n; i++) {
    int32_t diff = (int32_t)a[i] - b[i];
    result += diff * diff;
  }
  return result;
#endif
}

#ifdef __cplusplus
}
#endif

#endif // EIF_HAL_SIMD_H
