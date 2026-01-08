/**
 * @file eif_hal_compute_generic.c
 * @brief Generic Pure C Implementation of HAL Compute Interface
 */

#include "eif_hal_compute.h"
#include <math.h>
#include <string.h>

// Helper macros
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLAMP_RELU6(x) (MIN(MAX(x, 0.0f), 6.0f))

static inline float32_t apply_activation(float32_t val, int act_type) {
  if (act_type == 1)
    return MAX(val, 0.0f); // ReLU
  if (act_type == 2)
    return CLAMP_RELU6(val); // ReLU6
  return val;                // None
}

eif_status_t eif_hal_conv1d(const float32_t *input, const float32_t *filters,
                            const float32_t *biases, float32_t *output,
                            int in_w, int in_c, int out_w, int out_c, int k_w,
                            int stride, int pad, int act_type) {

  // Generic Conv1D loop
  // Weights layout: [out_c, k_w, in_c]

  for (int w = 0; w < out_w; w++) {
    for (int f = 0; f < out_c; f++) {
      float32_t sum = 0.0f;
      if (biases)
        sum = biases[f];

      int in_start_w = w * stride - pad;

      // Kernel loop
      for (int k = 0; k < k_w; k++) {
        int in_idx_w = in_start_w + k;
        if (in_idx_w >= 0 && in_idx_w < in_w) {
          // Vector dot product over channels
          const float32_t *in_ptr = input + in_idx_w * in_c;
          const float32_t *w_ptr = filters + (f * k_w + k) * in_c;

          for (int c = 0; c < in_c; c++) {
            sum += in_ptr[c] * w_ptr[c];
          }
        }
      }
      output[w * out_c + f] = apply_activation(sum, act_type);
    }
  }
  return EIF_STATUS_OK;
}

eif_status_t eif_hal_conv2d(const float32_t *input, const float32_t *filters,
                            const float32_t *biases, float32_t *output,
                            int in_h, int in_w, int in_c, int out_h, int out_w,
                            int out_c, int k_h, int k_w, int stride_h,
                            int stride_w, int pad_h, int pad_w, int act_type) {

  // Simple 6-loop implementation (Batch=1)
  // Weights: [out_c, k_h, k_w, in_c]

  for (int y = 0; y < out_h; y++) {
    for (int x = 0; x < out_w; x++) {
      for (int f = 0; f < out_c; f++) {
        float32_t sum = 0.0f;
        if (biases)
          sum = biases[f];

        int in_start_y = y * stride_h - pad_h;
        int in_start_x = x * stride_w - pad_w;

        for (int ky = 0; ky < k_h; ky++) {
          int in_y = in_start_y + ky;
          if (in_y >= 0 && in_y < in_h) {
            for (int kx = 0; kx < k_w; kx++) {
              int in_x = in_start_x + kx;
              if (in_x >= 0 && in_x < in_w) {
                // Inner loop over channels
                const float32_t *in_ptr = input + (in_y * in_w + in_x) * in_c;
                const float32_t *w_ptr =
                    filters + ((f * k_h + ky) * k_w + kx) * in_c;

                for (int c = 0; c < in_c; c++) {
                  sum += in_ptr[c] * w_ptr[c];
                }
              }
            }
          }
        }
        output[(y * out_w + x) * out_c + f] = apply_activation(sum, act_type);
      }
    }
  }
  return EIF_STATUS_OK;
}

eif_status_t eif_hal_depthwise_conv2d(
    const float32_t *input, const float32_t *filters, const float32_t *biases,
    float32_t *output, int in_h, int in_w, int in_c, int out_h, int out_w,
    int out_c, int k_h, int k_w, int stride_h, int stride_w, int pad_h,
    int pad_w, int depth_multiplier, int act_type) {
  // Depthwise: each input channel is convolved with its own filter (and
  // potentially multiplied) Output channels = in_c * depth_multiplier

  for (int y = 0; y < out_h; y++) {
    for (int x = 0; x < out_w; x++) {
      for (int c = 0; c < in_c; c++) {
        for (int m = 0; m < depth_multiplier; m++) {
          int out_idx_c = c * depth_multiplier + m;
          float32_t sum = 0.0f;
          if (biases)
            sum = biases[out_idx_c];

          int in_start_y = y * stride_h - pad_h;
          int in_start_x = x * stride_w - pad_w;

          for (int ky = 0; ky < k_h; ky++) {
            int in_y = in_start_y + ky;
            if (in_y >= 0 && in_y < in_h) {
              for (int kx = 0; kx < k_w; kx++) {
                int in_x = in_start_x + kx;
                if (in_x >= 0 && in_x < in_w) {
                  float32_t val = input[(in_y * in_w + in_x) * in_c + c];
                  // Weights: [1, k_h, k_w, out_c] or [out_c, k_h, k_w, 1]?
                  // Usually Depthwise weights are [1, k_h, k_w, out_c] in
                  // TFLite Here we assume: [k_h, k_w, out_c] since input depth
                  // is implicit Index: (ky * k_w + kx) * out_c + out_idx_c
                  float32_t w = filters[((ky * k_w + kx) * out_c) + out_idx_c];
                  sum += val * w;
                }
              }
            }
          }
          output[(y * out_w + x) * out_c + out_idx_c] =
              apply_activation(sum, act_type);
        }
      }
    }
  }
  return EIF_STATUS_OK;
}

eif_status_t eif_hal_fully_connected(const float32_t *input,
                                     const float32_t *weights,
                                     const float32_t *biases, float32_t *output,
                                     int input_size, int units, int act_type) {
  for (int u = 0; u < units; u++) {
    float32_t sum = 0.0f;
    if (biases)
      sum = biases[u];

    // Weights: [units, input_size]
    const float32_t *w_ptr = weights + u * input_size;

    for (int i = 0; i < input_size; i++) {
      sum += input[i] * w_ptr[i];
    }
    output[u] = apply_activation(sum, act_type);
  }
  return EIF_STATUS_OK;
}

eif_status_t eif_hal_maxpool2d(const float32_t *input, float32_t *output,
                               int in_h, int in_w, int in_c, int out_h,
                               int out_w, int k_h, int k_w, int stride_h,
                               int stride_w, int pad_h, int pad_w) {
  for (int y = 0; y < out_h; y++) {
    for (int x = 0; x < out_w; x++) {
      for (int c = 0; c < in_c; c++) {
        float32_t max_val = -3.4028235E38f; // FLT_MIN

        int in_start_y = y * stride_h - pad_h;
        int in_start_x = x * stride_w - pad_w;

        for (int ky = 0; ky < k_h; ky++) {
          int in_y = in_start_y + ky;
          if (in_y >= 0 && in_y < in_h) {
            for (int kx = 0; kx < k_w; kx++) {
              int in_x = in_start_x + kx;
              if (in_x >= 0 && in_x < in_w) {
                float32_t val = input[(in_y * in_w + in_x) * in_c + c];
                if (val > max_val)
                  max_val = val;
              }
            }
          }
        }
        output[(y * out_w + x) * in_c + c] = max_val;
      }
    }
  }
  return EIF_STATUS_OK;
}

eif_status_t eif_hal_dot_product(const float32_t *vec_a, const float32_t *vec_b,
                                 int size, float32_t *result) {
  float32_t sum = 0.0f;
  for (int i = 0; i < size; i++) {
    sum += vec_a[i] * vec_b[i];
  }
  *result = sum;
  return EIF_STATUS_OK;
}

eif_status_t eif_hal_vector_scale(const float32_t *vec, float32_t scale,
                                  float32_t *output, int size) {
  for (int i = 0; i < size; i++)
    output[i] = vec[i] * scale;
  return EIF_STATUS_OK;
}

eif_status_t eif_hal_vector_add(const float32_t *vec_a, const float32_t *vec_b,
                                float32_t *output, int size) {
  for (int i = 0; i < size; i++)
    output[i] = vec_a[i] + vec_b[i];
  return EIF_STATUS_OK;
}
