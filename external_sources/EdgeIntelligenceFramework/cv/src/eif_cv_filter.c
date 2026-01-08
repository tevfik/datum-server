/**
 * @file eif_cv_filter.c
 * @brief Image Filtering and Convolution
 */

#include "eif_cv_filter.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Generic Convolution
// ============================================================================

eif_status_t eif_cv_filter2d(const eif_cv_image_t* src, eif_cv_image_t* dst,
                              const float32_t* kernel, int ksize) {
    if (!src || !dst || !kernel || ksize < 1 || (ksize % 2) == 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    if (src->format != EIF_CV_GRAY8 || dst->format != EIF_CV_GRAY8) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    int radius = ksize / 2;
    
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            float32_t sum = 0.0f;
            
            for (int ky = -radius; ky <= radius; ky++) {
                for (int kx = -radius; kx <= radius; kx++) {
                    int sy = y + ky;
                    int sx = x + kx;
                    
                    // Clamp to border
                    if (sy < 0) sy = 0;
                    if (sy >= src->height) sy = src->height - 1;
                    if (sx < 0) sx = 0;
                    if (sx >= src->width) sx = src->width - 1;
                    
                    float32_t kval = kernel[(ky + radius) * ksize + (kx + radius)];
                    sum += src->data[sy * src->stride + sx] * kval;
                }
            }
            
            // Clamp to [0, 255]
            if (sum < 0) sum = 0;
            if (sum > 255) sum = 255;
            dst->data[y * dst->stride + x] = (uint8_t)(sum + 0.5f);
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_cv_sep_filter2d(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                  const float32_t* kernel_x, int ksize_x,
                                  const float32_t* kernel_y, int ksize_y,
                                  eif_memory_pool_t* pool) {
    if (!src || !dst || !kernel_x || !kernel_y || !pool) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    // Allocate temporary buffer
    eif_cv_image_t temp;
    eif_status_t status = eif_cv_image_create(&temp, src->width, src->height,
                                               src->format, pool);
    if (status != EIF_STATUS_OK) return status;
    
    int radius_x = ksize_x / 2;
    int radius_y = ksize_y / 2;
    
    // Horizontal pass
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            float32_t sum = 0.0f;
            for (int kx = -radius_x; kx <= radius_x; kx++) {
                int sx = x + kx;
                if (sx < 0) sx = 0;
                if (sx >= src->width) sx = src->width - 1;
                sum += src->data[y * src->stride + sx] * kernel_x[kx + radius_x];
            }
            if (sum < 0) sum = 0;
            if (sum > 255) sum = 255;
            temp.data[y * temp.stride + x] = (uint8_t)(sum + 0.5f);
        }
    }
    
    // Vertical pass
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            float32_t sum = 0.0f;
            for (int ky = -radius_y; ky <= radius_y; ky++) {
                int sy = y + ky;
                if (sy < 0) sy = 0;
                if (sy >= src->height) sy = src->height - 1;
                sum += temp.data[sy * temp.stride + x] * kernel_y[ky + radius_y];
            }
            if (sum < 0) sum = 0;
            if (sum > 255) sum = 255;
            dst->data[y * dst->stride + x] = (uint8_t)(sum + 0.5f);
        }
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Blur Filters
// ============================================================================

eif_status_t eif_cv_blur_box(const eif_cv_image_t* src, eif_cv_image_t* dst,
                              int ksize) {
    if (!src || !dst || ksize < 1 || (ksize % 2) == 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    // Maximum supported kernel size is 15x15 = 225 floats = 900 bytes stack
    #define MAX_BOX_KSIZE 15
    if (ksize > MAX_BOX_KSIZE) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    float32_t kernel[MAX_BOX_KSIZE * MAX_BOX_KSIZE];
    float32_t val = 1.0f / (ksize * ksize);
    
    int kernel_size = ksize * ksize;
    for (int i = 0; i < kernel_size; i++) {
        kernel[i] = val;
    }
    
    return eif_cv_filter2d(src, dst, kernel, ksize);
    #undef MAX_BOX_KSIZE
}

eif_status_t eif_cv_blur_gaussian(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                   int ksize, float32_t sigma,
                                   eif_memory_pool_t* pool) {
    if (!src || !dst || !pool || ksize < 1 || (ksize % 2) == 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    if (sigma <= 0) {
        sigma = 0.3f * ((ksize - 1) * 0.5f - 1) + 0.8f;
    }
    
    // Create 1D Gaussian kernel
    float32_t* kernel = eif_memory_alloc(pool, ksize * sizeof(float32_t), 4);
    if (!kernel) return EIF_STATUS_OUT_OF_MEMORY;
    
    int radius = ksize / 2;
    float32_t sum = 0.0f;
    
    for (int i = 0; i < ksize; i++) {
        float32_t x = (float32_t)(i - radius);
        kernel[i] = expf(-x * x / (2 * sigma * sigma));
        sum += kernel[i];
    }
    
    // Normalize
    for (int i = 0; i < ksize; i++) {
        kernel[i] /= sum;
    }
    
    // Apply separable filter
    return eif_cv_sep_filter2d(src, dst, kernel, ksize, kernel, ksize, pool);
}

eif_status_t eif_cv_blur_median(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                 int ksize) {
    if (!src || !dst || ksize < 1 || (ksize % 2) == 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    if (src->format != EIF_CV_GRAY8 || dst->format != EIF_CV_GRAY8) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    // Maximum supported kernel size is 15x15 = 225 bytes on stack
    #define MAX_MEDIAN_KSIZE 15
    if (ksize > MAX_MEDIAN_KSIZE) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    int radius = ksize / 2;
    int window_size = ksize * ksize;
    uint8_t window[MAX_MEDIAN_KSIZE * MAX_MEDIAN_KSIZE];
    
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            int idx = 0;
            
            for (int ky = -radius; ky <= radius; ky++) {
                for (int kx = -radius; kx <= radius; kx++) {
                    int sy = y + ky;
                    int sx = x + kx;
                    if (sy < 0) sy = 0;
                    if (sy >= src->height) sy = src->height - 1;
                    if (sx < 0) sx = 0;
                    if (sx >= src->width) sx = src->width - 1;
                    
                    window[idx++] = src->data[sy * src->stride + sx];
                }
            }
            
            // Simple insertion sort for small arrays
            for (int i = 1; i < window_size; i++) {
                uint8_t key = window[i];
                int j = i - 1;
                while (j >= 0 && window[j] > key) {
                    window[j + 1] = window[j];
                    j--;
                }
                window[j + 1] = key;
            }
            
            dst->data[y * dst->stride + x] = window[window_size / 2];
        }
    }
    
    return EIF_STATUS_OK;
    #undef MAX_MEDIAN_KSIZE
}

eif_status_t eif_cv_blur_bilateral(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                    int ksize, float32_t sigma_space,
                                    float32_t sigma_color) {
    if (!src || !dst || ksize < 1 || (ksize % 2) == 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    int radius = ksize / 2;
    float32_t space_coeff = -0.5f / (sigma_space * sigma_space);
    float32_t color_coeff = -0.5f / (sigma_color * sigma_color);
    
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            float32_t sum = 0.0f;
            float32_t weight_sum = 0.0f;
            uint8_t center = src->data[y * src->stride + x];
            
            for (int ky = -radius; ky <= radius; ky++) {
                for (int kx = -radius; kx <= radius; kx++) {
                    int sy = y + ky;
                    int sx = x + kx;
                    if (sy < 0) sy = 0;
                    if (sy >= src->height) sy = src->height - 1;
                    if (sx < 0) sx = 0;
                    if (sx >= src->width) sx = src->width - 1;
                    
                    uint8_t neighbor = src->data[sy * src->stride + sx];
                    
                    float32_t space_dist = kx * kx + ky * ky;
                    float32_t color_dist = (float32_t)(center - neighbor) * (center - neighbor);
                    
                    float32_t weight = expf(space_dist * space_coeff + color_dist * color_coeff);
                    sum += neighbor * weight;
                    weight_sum += weight;
                }
            }
            
            dst->data[y * dst->stride + x] = (uint8_t)(sum / weight_sum + 0.5f);
        }
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Edge Detection Filters
// ============================================================================

eif_status_t eif_cv_sobel(const eif_cv_image_t* src, eif_cv_image_t* dst,
                           int dx, int dy, int ksize) {
    if (!src || !dst) return EIF_STATUS_INVALID_ARGUMENT;
    if (dx == 0 && dy == 0) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Sobel 3x3 kernels
    static const float32_t sobel_x[9] = {
        -1, 0, 1,
        -2, 0, 2,
        -1, 0, 1
    };
    static const float32_t sobel_y[9] = {
        -1, -2, -1,
         0,  0,  0,
         1,  2,  1
    };
    
    const float32_t* kernel = dx ? sobel_x : sobel_y;
    
    // Apply convolution
    int radius = 1;
    
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            float32_t sum = 0.0f;
            
            for (int ky = -radius; ky <= radius; ky++) {
                for (int kx = -radius; kx <= radius; kx++) {
                    int sy = y + ky;
                    int sx = x + kx;
                    if (sy < 0) sy = 0;
                    if (sy >= src->height) sy = src->height - 1;
                    if (sx < 0) sx = 0;
                    if (sx >= src->width) sx = src->width - 1;
                    
                    float32_t kval = kernel[(ky + radius) * 3 + (kx + radius)];
                    sum += src->data[sy * src->stride + sx] * kval;
                }
            }
            
            // Scale and clamp
            sum = sum / 4.0f + 128.0f;  // Normalize to [0, 255]
            if (sum < 0) sum = 0;
            if (sum > 255) sum = 255;
            dst->data[y * dst->stride + x] = (uint8_t)sum;
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_cv_scharr(const eif_cv_image_t* src, eif_cv_image_t* dst,
                            int dx, int dy) {
    // Scharr 3x3 kernels (more accurate than Sobel)
    static const float32_t scharr_x[9] = {
        -3,  0,  3,
        -10, 0, 10,
        -3,  0,  3
    };
    static const float32_t scharr_y[9] = {
        -3, -10, -3,
         0,   0,  0,
         3,  10,  3
    };
    
    const float32_t* kernel = dx ? scharr_x : scharr_y;
    return eif_cv_filter2d(src, dst, kernel, 3);
}

eif_status_t eif_cv_laplacian(const eif_cv_image_t* src, eif_cv_image_t* dst,
                               int ksize) {
    static const float32_t laplacian[9] = {
        0,  1, 0,
        1, -4, 1,
        0,  1, 0
    };
    
    return eif_cv_filter2d(src, dst, laplacian, 3);
}

eif_status_t eif_cv_gradient_magnitude(const eif_cv_image_t* gx,
                                        const eif_cv_image_t* gy,
                                        eif_cv_image_t* magnitude) {
    if (!gx || !gy || !magnitude) return EIF_STATUS_INVALID_ARGUMENT;
    
    for (int y = 0; y < gx->height; y++) {
        for (int x = 0; x < gx->width; x++) {
            int dx = gx->data[y * gx->stride + x] - 128;
            int dy = gy->data[y * gy->stride + x] - 128;
            float32_t mag = sqrtf((float32_t)(dx * dx + dy * dy));
            if (mag > 255) mag = 255;
            magnitude->data[y * magnitude->stride + x] = (uint8_t)mag;
        }
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Thresholding
// ============================================================================

eif_status_t eif_cv_threshold(const eif_cv_image_t* src, eif_cv_image_t* dst,
                               uint8_t thresh, uint8_t max_val,
                               eif_cv_thresh_type_t type) {
    if (!src || !dst) return EIF_STATUS_INVALID_ARGUMENT;
    
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            uint8_t val = src->data[y * src->stride + x];
            uint8_t result;
            
            switch (type) {
                case EIF_CV_THRESH_BINARY:
                    result = (val > thresh) ? max_val : 0;
                    break;
                case EIF_CV_THRESH_BINARY_INV:
                    result = (val > thresh) ? 0 : max_val;
                    break;
                case EIF_CV_THRESH_TRUNC:
                    result = (val > thresh) ? thresh : val;
                    break;
                case EIF_CV_THRESH_TOZERO:
                    result = (val > thresh) ? val : 0;
                    break;
                case EIF_CV_THRESH_TOZERO_INV:
                    result = (val > thresh) ? 0 : val;
                    break;
                default:
                    result = val;
            }
            
            dst->data[y * dst->stride + x] = result;
        }
    }
    
    return EIF_STATUS_OK;
}

uint8_t eif_cv_threshold_otsu(const eif_cv_image_t* src, eif_cv_image_t* dst,
                               uint8_t max_val) {
    if (!src || !dst) return 0;
    
    // Compute histogram
    int histogram[256] = {0};
    int total = src->width * src->height;
    
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            histogram[src->data[y * src->stride + x]]++;
        }
    }
    
    // Otsu's method
    float32_t sum = 0;
    for (int i = 0; i < 256; i++) {
        sum += i * histogram[i];
    }
    
    float32_t sum_b = 0;
    int w_b = 0;
    float32_t max_variance = 0;
    uint8_t threshold = 0;
    
    for (int t = 0; t < 256; t++) {
        w_b += histogram[t];
        if (w_b == 0) continue;
        
        int w_f = total - w_b;
        if (w_f == 0) break;
        
        sum_b += t * histogram[t];
        float32_t m_b = sum_b / w_b;
        float32_t m_f = (sum - sum_b) / w_f;
        
        float32_t variance = (float32_t)w_b * w_f * (m_b - m_f) * (m_b - m_f);
        
        if (variance > max_variance) {
            max_variance = variance;
            threshold = t;
        }
    }
    
    // Apply threshold
    eif_cv_threshold(src, dst, threshold, max_val, EIF_CV_THRESH_BINARY);
    
    return threshold;
}

eif_status_t eif_cv_adaptive_threshold(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                        uint8_t max_val, int block_size,
                                        int C, eif_memory_pool_t* pool) {
    if (!src || !dst || !pool || block_size < 3 || (block_size % 2) == 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    // Compute local mean using box filter
    eif_cv_image_t mean;
    eif_cv_image_create(&mean, src->width, src->height, src->format, pool);
    eif_cv_blur_box(src, &mean, block_size);
    
    // Apply adaptive threshold
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            int val = src->data[y * src->stride + x];
            int thresh = mean.data[y * mean.stride + x] - C;
            dst->data[y * dst->stride + x] = (val > thresh) ? max_val : 0;
        }
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Sharpening
// ============================================================================

eif_status_t eif_cv_sharpen(const eif_cv_image_t* src, eif_cv_image_t* dst,
                             float32_t amount, eif_memory_pool_t* pool) {
    if (!src || !dst || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Compute blurred version
    eif_cv_image_t blurred;
    eif_cv_image_create(&blurred, src->width, src->height, src->format, pool);
    eif_cv_blur_gaussian(src, &blurred, 5, 1.0f, pool);
    
    // Unsharp mask: result = src + amount * (src - blurred)
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            float32_t val = src->data[y * src->stride + x];
            float32_t blur_val = blurred.data[y * blurred.stride + x];
            float32_t result = val + amount * (val - blur_val);
            
            if (result < 0) result = 0;
            if (result > 255) result = 255;
            dst->data[y * dst->stride + x] = (uint8_t)result;
        }
    }
    
    return EIF_STATUS_OK;
}
