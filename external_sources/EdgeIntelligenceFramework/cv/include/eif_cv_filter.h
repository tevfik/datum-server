/**
 * @file eif_cv_filter.h
 * @brief Image Filtering and Convolution
 */

#ifndef EIF_CV_FILTER_H
#define EIF_CV_FILTER_H

#include "eif_cv_image.h"

// ============================================================================
// Generic Convolution
// ============================================================================

/**
 * @brief Apply 2D convolution with custom kernel
 * 
 * @param src Source image (grayscale)
 * @param dst Destination image (same size as src)
 * @param kernel Convolution kernel (row-major)
 * @param ksize Kernel size (must be odd: 3, 5, 7...)
 * @return Status code
 */
eif_status_t eif_cv_filter2d(const eif_cv_image_t* src, eif_cv_image_t* dst,
                              const float32_t* kernel, int ksize);

/**
 * @brief Separable 2D filter (faster for separable kernels)
 */
eif_status_t eif_cv_sep_filter2d(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                  const float32_t* kernel_x, int ksize_x,
                                  const float32_t* kernel_y, int ksize_y,
                                  eif_memory_pool_t* pool);

// ============================================================================
// Blur Filters
// ============================================================================

/**
 * @brief Box blur (averaging filter)
 */
eif_status_t eif_cv_blur_box(const eif_cv_image_t* src, eif_cv_image_t* dst,
                              int ksize);

/**
 * @brief Gaussian blur
 * 
 * @param sigma Standard deviation (0 = auto from ksize)
 */
eif_status_t eif_cv_blur_gaussian(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                   int ksize, float32_t sigma,
                                   eif_memory_pool_t* pool);

/**
 * @brief Median filter (good for salt & pepper noise)
 */
eif_status_t eif_cv_blur_median(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                 int ksize);

/**
 * @brief Bilateral filter (edge-preserving smooth)
 * 
 * @param sigma_space Spatial sigma
 * @param sigma_color Color/intensity sigma
 */
eif_status_t eif_cv_blur_bilateral(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                    int ksize, float32_t sigma_space,
                                    float32_t sigma_color);

// ============================================================================
// Edge Detection Filters
// ============================================================================

/**
 * @brief Sobel derivative
 * 
 * @param dx X derivative order (0 or 1)
 * @param dy Y derivative order (0 or 1)
 */
eif_status_t eif_cv_sobel(const eif_cv_image_t* src, eif_cv_image_t* dst,
                           int dx, int dy, int ksize);

/**
 * @brief Scharr derivative (more accurate than Sobel)
 */
eif_status_t eif_cv_scharr(const eif_cv_image_t* src, eif_cv_image_t* dst,
                            int dx, int dy);

/**
 * @brief Laplacian (second derivative)
 */
eif_status_t eif_cv_laplacian(const eif_cv_image_t* src, eif_cv_image_t* dst,
                               int ksize);

/**
 * @brief Compute gradient magnitude from Sobel X and Y
 */
eif_status_t eif_cv_gradient_magnitude(const eif_cv_image_t* gx,
                                        const eif_cv_image_t* gy,
                                        eif_cv_image_t* magnitude);

// ============================================================================
// Thresholding
// ============================================================================

typedef enum {
    EIF_CV_THRESH_BINARY,        // dst = (src > thresh) ? max_val : 0
    EIF_CV_THRESH_BINARY_INV,    // dst = (src > thresh) ? 0 : max_val
    EIF_CV_THRESH_TRUNC,         // dst = (src > thresh) ? thresh : src
    EIF_CV_THRESH_TOZERO,        // dst = (src > thresh) ? src : 0
    EIF_CV_THRESH_TOZERO_INV     // dst = (src > thresh) ? 0 : src
} eif_cv_thresh_type_t;

/**
 * @brief Apply threshold
 */
eif_status_t eif_cv_threshold(const eif_cv_image_t* src, eif_cv_image_t* dst,
                               uint8_t thresh, uint8_t max_val,
                               eif_cv_thresh_type_t type);

/**
 * @brief Otsu's automatic threshold selection
 * 
 * @return Computed threshold value
 */
uint8_t eif_cv_threshold_otsu(const eif_cv_image_t* src, eif_cv_image_t* dst,
                               uint8_t max_val);

/**
 * @brief Adaptive threshold
 * 
 * Computes threshold locally based on neighborhood.
 */
eif_status_t eif_cv_adaptive_threshold(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                        uint8_t max_val, int block_size,
                                        int C, eif_memory_pool_t* pool);

// ============================================================================
// Sharpening
// ============================================================================

/**
 * @brief Unsharp mask (sharpen image)
 * 
 * @param amount Sharpening strength (typically 0.5-2.0)
 */
eif_status_t eif_cv_sharpen(const eif_cv_image_t* src, eif_cv_image_t* dst,
                             float32_t amount, eif_memory_pool_t* pool);

#endif // EIF_CV_FILTER_H
