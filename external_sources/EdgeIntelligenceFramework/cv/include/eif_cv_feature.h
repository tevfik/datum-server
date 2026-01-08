/**
 * @file eif_cv_feature.h
 * @brief Feature Detection and Description
 */

#ifndef EIF_CV_FEATURE_H
#define EIF_CV_FEATURE_H

#include "eif_cv_image.h"

// ============================================================================
// Keypoint and Descriptor Types
// ============================================================================

/**
 * @brief Keypoint (detected feature point)
 */
typedef struct {
    float32_t x, y;         // Sub-pixel position
    float32_t size;         // Keypoint diameter
    float32_t angle;        // Orientation in degrees
    float32_t response;     // Detector response strength
    int octave;             // Pyramid octave
    int class_id;           // Object class (for classification)
} eif_cv_keypoint_t;

/**
 * @brief Binary descriptor (256 bits)
 */
typedef struct {
    uint8_t data[32];
} eif_cv_descriptor_t;

/**
 * @brief Feature match
 */
typedef struct {
    int query_idx;          // Index in first set
    int train_idx;          // Index in second set
    float32_t distance;     // Match distance
} eif_cv_match_t;

// ============================================================================
// Edge Detection
// ============================================================================

/**
 * @brief Canny edge detection
 * 
 * @param src Input grayscale image
 * @param dst Output edge image (binary)
 * @param low_thresh Low threshold for hysteresis
 * @param high_thresh High threshold for hysteresis
 * @param pool Memory pool
 */
eif_status_t eif_cv_canny(const eif_cv_image_t* src, eif_cv_image_t* dst,
                           uint8_t low_thresh, uint8_t high_thresh,
                           eif_memory_pool_t* pool);

// ============================================================================
// Corner Detection
// ============================================================================

/**
 * @brief FAST corner detector
 * 
 * @param src Input grayscale image
 * @param threshold Intensity threshold (typically 10-50)
 * @param nonmax_suppression Apply non-maximum suppression
 * @param corners Output array of keypoints
 * @param max_corners Maximum corners to detect
 * @return Number of corners detected
 */
int eif_cv_detect_fast(const eif_cv_image_t* src, int threshold,
                        bool nonmax_suppression,
                        eif_cv_keypoint_t* corners, int max_corners);

/**
 * @brief Harris corner detector
 * 
 * @param src Input grayscale image
 * @param block_size Neighborhood size
 * @param ksize Sobel kernel size
 * @param k Harris detector free parameter (typically 0.04-0.06)
 * @param threshold Response threshold
 * @param corners Output array
 * @param max_corners Maximum corners
 * @param pool Memory pool
 * @return Number of corners
 */
int eif_cv_detect_harris(const eif_cv_image_t* src, int block_size, int ksize,
                          float32_t k, float32_t threshold,
                          eif_cv_keypoint_t* corners, int max_corners,
                          eif_memory_pool_t* pool);

/**
 * @brief Shi-Tomasi (Good Features to Track)
 */
int eif_cv_detect_shi_tomasi(const eif_cv_image_t* src, int max_corners,
                              float32_t quality_level, float32_t min_distance,
                              eif_cv_keypoint_t* corners,
                              eif_memory_pool_t* pool);

// ============================================================================
// Feature Description
// ============================================================================

/**
 * @brief Compute ORB descriptors
 * 
 * @param img Input grayscale image
 * @param keypoints Keypoints to describe
 * @param num_keypoints Number of keypoints
 * @param descriptors Output descriptors
 * @param pool Memory pool
 */
eif_status_t eif_cv_compute_orb(const eif_cv_image_t* img,
                                 eif_cv_keypoint_t* keypoints, int num_keypoints,
                                 eif_cv_descriptor_t* descriptors,
                                 eif_memory_pool_t* pool);

/**
 * @brief Compute BRIEF descriptors
 */
eif_status_t eif_cv_compute_brief(const eif_cv_image_t* img,
                                   const eif_cv_keypoint_t* keypoints, int num_keypoints,
                                   eif_cv_descriptor_t* descriptors);

// ============================================================================
// Feature Matching
// ============================================================================

/**
 * @brief Match binary descriptors using Hamming distance
 * 
 * @param desc1 First descriptor set
 * @param n1 Number of descriptors in first set
 * @param desc2 Second descriptor set
 * @param n2 Number of descriptors in second set
 * @param matches Output matches (size n1)
 * @param ratio_thresh Lowe's ratio test threshold (typically 0.7-0.8)
 * @return Number of good matches
 */
int eif_cv_match_bf(const eif_cv_descriptor_t* desc1, int n1,
                     const eif_cv_descriptor_t* desc2, int n2,
                     eif_cv_match_t* matches, float32_t ratio_thresh);

/**
 * @brief Compute Hamming distance between descriptors
 */
int eif_cv_hamming_distance(const eif_cv_descriptor_t* a, const eif_cv_descriptor_t* b);

// ============================================================================
// HOG (Histogram of Oriented Gradients)
// ============================================================================

/**
 * @brief HOG descriptor configuration
 */
typedef struct {
    int win_width;          // Detection window width
    int win_height;         // Detection window height
    int block_size;         // Block size in pixels
    int block_stride;       // Block stride in pixels
    int cell_size;          // Cell size in pixels
    int num_bins;           // Number of orientation bins (typically 9)
} eif_cv_hog_config_t;

/**
 * @brief Compute HOG descriptor for image region
 * 
 * @param img Input grayscale image
 * @param roi Region of interest
 * @param config HOG configuration
 * @param descriptor Output descriptor (caller allocates)
 * @param pool Memory pool
 * @return Status code
 */
eif_status_t eif_cv_compute_hog(const eif_cv_image_t* img,
                                 const eif_cv_rect_t* roi,
                                 const eif_cv_hog_config_t* config,
                                 float32_t* descriptor,
                                 eif_memory_pool_t* pool);

/**
 * @brief Get HOG descriptor size
 */
int eif_cv_hog_descriptor_size(const eif_cv_hog_config_t* config);

#endif // EIF_CV_FEATURE_H
