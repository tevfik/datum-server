/**
 * @file eif_cv_morph.h
 * @brief Morphological Operations
 */

#ifndef EIF_CV_MORPH_H
#define EIF_CV_MORPH_H

#include "eif_cv_image.h"

// ============================================================================
// Structuring Elements
// ============================================================================

typedef enum {
    EIF_CV_MORPH_RECT,      // Rectangle
    EIF_CV_MORPH_CROSS,     // Cross shape
    EIF_CV_MORPH_ELLIPSE    // Ellipse
} eif_cv_morph_shape_t;

/**
 * @brief Create structuring element
 * 
 * @param elem Output element buffer (ksize * ksize bytes)
 * @param shape Shape type
 * @param ksize Kernel size
 */
eif_status_t eif_cv_morph_element(uint8_t* elem, eif_cv_morph_shape_t shape, int ksize);

// ============================================================================
// Basic Morphological Operations
// ============================================================================

/**
 * @brief Erosion
 * 
 * Shrinks bright regions, expands dark regions.
 */
eif_status_t eif_cv_erode(const eif_cv_image_t* src, eif_cv_image_t* dst,
                           int ksize, int iterations);

/**
 * @brief Dilation
 * 
 * Expands bright regions, shrinks dark regions.
 */
eif_status_t eif_cv_dilate(const eif_cv_image_t* src, eif_cv_image_t* dst,
                            int ksize, int iterations);

/**
 * @brief Opening (erosion followed by dilation)
 * 
 * Removes small bright spots (noise).
 */
eif_status_t eif_cv_morph_open(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                int ksize, eif_memory_pool_t* pool);

/**
 * @brief Closing (dilation followed by erosion)
 * 
 * Fills small dark holes.
 */
eif_status_t eif_cv_morph_close(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                 int ksize, eif_memory_pool_t* pool);

// ============================================================================
// Advanced Morphological Operations
// ============================================================================

/**
 * @brief Morphological gradient (dilation - erosion)
 * 
 * Outlines objects.
 */
eif_status_t eif_cv_morph_gradient(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                    int ksize, eif_memory_pool_t* pool);

/**
 * @brief Top hat transform (src - opening)
 * 
 * Extracts small bright details.
 */
eif_status_t eif_cv_morph_tophat(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                  int ksize, eif_memory_pool_t* pool);

/**
 * @brief Black hat transform (closing - src)
 * 
 * Extracts small dark details.
 */
eif_status_t eif_cv_morph_blackhat(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                    int ksize, eif_memory_pool_t* pool);

// ============================================================================
// Connected Components
// ============================================================================

/**
 * @brief Connected component labeling
 * 
 * @param binary Binary input image
 * @param labels Output label image (int32 per pixel)
 * @param connectivity 4 or 8 connectivity
 * @param pool Memory pool
 * @return Number of components (excluding background)
 */
int eif_cv_connected_components(const eif_cv_image_t* binary,
                                 int32_t* labels, int connectivity,
                                 eif_memory_pool_t* pool);

/**
 * @brief Component statistics
 */
typedef struct {
    int label;
    int area;               // Number of pixels
    eif_cv_rect_t bbox;     // Bounding box
    float32_t centroid_x;   // Center of mass X
    float32_t centroid_y;   // Center of mass Y
} eif_cv_component_stats_t;

/**
 * @brief Get component statistics
 */
eif_status_t eif_cv_component_stats(const int32_t* labels, int width, int height,
                                     int num_components,
                                     eif_cv_component_stats_t* stats);

// ============================================================================
// Contour Detection
// ============================================================================

/**
 * @brief Contour point array
 */
typedef struct {
    eif_cv_point_t* points;
    int num_points;
    int capacity;
} eif_cv_contour_t;

/**
 * @brief Find contours in binary image
 * 
 * @param binary Binary input image
 * @param contours Output contours array
 * @param max_contours Maximum contours to find
 * @param pool Memory pool
 * @return Number of contours found
 */
int eif_cv_find_contours(const eif_cv_image_t* binary,
                          eif_cv_contour_t* contours, int max_contours,
                          eif_memory_pool_t* pool);

/**
 * @brief Compute contour area
 */
float32_t eif_cv_contour_area(const eif_cv_contour_t* contour);

/**
 * @brief Compute contour perimeter
 */
float32_t eif_cv_contour_perimeter(const eif_cv_contour_t* contour);

/**
 * @brief Get contour bounding rectangle
 */
eif_cv_rect_t eif_cv_contour_bounding_rect(const eif_cv_contour_t* contour);

#endif // EIF_CV_MORPH_H
