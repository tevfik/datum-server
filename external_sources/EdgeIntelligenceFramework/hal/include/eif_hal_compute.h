/**
 * @file eif_hal_compute.h
 * @brief EIF HAL Compute Layer Interface
 *
 * Defines the interface for hardware-accelerated (or optimized generic)
 * compute kernels. This allows swapping the backend (e.g. CMSIS-NN, NEON)
 * without changing the upper layers.
 */

#ifndef EIF_HAL_COMPUTE_H
#define EIF_HAL_COMPUTE_H

#include "eif_status.h"
#include "eif_types.h"

// Forward declaration of layer types to avoid circular dependencies if
// possible, but usually we need definitions. For now specific params are passed
// explicitly or we include the necessary types. Since we want this decoupled,
// passing primitive types/structs is better. However, for convenience, we often
// mirror the layer params.

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Neural Network Kernels
// =============================================================================

/**
 * @brief Accelerated 1D Convolution
 *
 * @param input Input buffer [in_w * in_c]
 * @param filters Weights [filters * kernel_size * in_c]
 * @param biases Biases [filters] (optional)
 * @param output Output buffer [out_w * filters]
 * @param in_w Input width
 * @param in_c Input channels
 * @param out_w Output width
 * @param out_c Output channels (num filters)
 * @param k_w Kernel width (size)
 * @param stride Stride
 * @param pad Padding
 * @param act_type Activation type (0=None, 1=ReLU, 2=ReLU6)
 */
eif_status_t eif_hal_conv1d(const float32_t *input, const float32_t *filters,
                            const float32_t *biases, float32_t *output,
                            int in_w, int in_c, int out_w, int out_c, int k_w,
                            int stride, int pad, int act_type);

/**
 * @brief Accelerated 2D Convolution (NHWC)
 */
eif_status_t eif_hal_conv2d(const float32_t *input, const float32_t *filters,
                            const float32_t *biases, float32_t *output,
                            int in_h, int in_w, int in_c, int out_h, int out_w,
                            int out_c, int k_h, int k_w, int stride_h,
                            int stride_w, int pad_h, int pad_w, int act_type);

/**
 * @brief Accelerated Depthwise Convolution
 */
eif_status_t eif_hal_depthwise_conv2d(
    const float32_t *input, const float32_t *filters, const float32_t *biases,
    float32_t *output, int in_h, int in_w, int in_c, int out_h, int out_w,
    int out_c, int k_h, int k_w, int stride_h, int stride_w, int pad_h,
    int pad_w, int depth_multiplier, int act_type);

/**
 * @brief Accelerated Fully Connected (Dense)
 */
eif_status_t eif_hal_fully_connected(const float32_t *input,
                                     const float32_t *weights,
                                     const float32_t *biases, float32_t *output,
                                     int input_size, int units, int act_type);

/**
 * @brief Accelerated Max Pooling
 */
eif_status_t eif_hal_maxpool2d(const float32_t *input, float32_t *output,
                               int in_h, int in_w, int in_c, int out_h,
                               int out_w, int k_h, int k_w, int stride_h,
                               int stride_w, int pad_h, int pad_w);

// =============================================================================
// DSP Kernels
// =============================================================================

/**
 * @brief Accelerated Vector Dot Product
 */
eif_status_t eif_hal_dot_product(const float32_t *vec_a, const float32_t *vec_b,
                                 int size, float32_t *result);

/**
 * @brief Accelerated Vector Scale
 */
eif_status_t eif_hal_vector_scale(const float32_t *vec, float32_t scale,
                                  float32_t *output, int size);

/**
 * @brief Accelerated Vector Add
 */
eif_status_t eif_hal_vector_add(const float32_t *vec_a, const float32_t *vec_b,
                                float32_t *output, int size);

#ifdef __cplusplus
}
#endif

#endif // EIF_HAL_COMPUTE_H
