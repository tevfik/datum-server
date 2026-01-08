/**
 * @file eif_cv_detect.h
 * @brief Object Detection
 */

#ifndef EIF_CV_DETECT_H
#define EIF_CV_DETECT_H

#include "eif_cv_image.h"
#include "eif_cv_feature.h"

// ============================================================================
// Template Matching
// ============================================================================

typedef enum {
    EIF_CV_TM_SQDIFF,           // Sum of squared differences
    EIF_CV_TM_SQDIFF_NORMED,    // Normalized SQDIFF
    EIF_CV_TM_CCORR,            // Cross-correlation
    EIF_CV_TM_CCORR_NORMED,     // Normalized cross-correlation
    EIF_CV_TM_CCOEFF,           // Correlation coefficient
    EIF_CV_TM_CCOEFF_NORMED     // Normalized correlation coefficient
} eif_cv_tm_method_t;

/**
 * @brief Template matching result
 */
typedef struct {
    int x, y;               // Best match position
    float32_t score;        // Match score
} eif_cv_tm_result_t;

/**
 * @brief Match template
 * 
 * @param src Source image
 * @param templ Template to find
 * @param result Match result map (size: (src.w - templ.w + 1) x (src.h - templ.h + 1))
 * @param method Matching method
 * @param pool Memory pool
 */
eif_status_t eif_cv_match_template(const eif_cv_image_t* src,
                                    const eif_cv_image_t* templ,
                                    float32_t* result,
                                    eif_cv_tm_method_t method,
                                    eif_memory_pool_t* pool);

/**
 * @brief Find best template match location
 */
eif_cv_tm_result_t eif_cv_template_minmax(const float32_t* result,
                                           int result_width, int result_height,
                                           eif_cv_tm_method_t method);

// ============================================================================
// Cascade Classifier (Haar/LBP)
// ============================================================================

/**
 * @brief Cascade stage
 */
typedef struct {
    float32_t* weak_classifiers;    // Array of weak classifier params
    int num_weak;                   // Number of weak classifiers
    float32_t threshold;            // Stage threshold
} eif_cv_cascade_stage_t;

/**
 * @brief Cascade classifier
 */
typedef struct {
    eif_cv_cascade_stage_t* stages; // Cascade stages
    int num_stages;
    int win_width;                  // Detection window width
    int win_height;                 // Detection window height
    int feature_type;               // 0=Haar, 1=LBP
} eif_cv_cascade_t;

/**
 * @brief Detection result
 */
typedef struct {
    eif_cv_rect_t rect;         // Detection bounding box
    float32_t confidence;       // Detection confidence
    int neighbors;              // Number of grouped detections
} eif_cv_detection_t;

/**
 * @brief Load cascade from data buffer
 * 
 * @param cascade Output cascade
 * @param data Binary cascade data
 * @param data_size Size of data
 * @param pool Memory pool
 */
eif_status_t eif_cv_cascade_load(eif_cv_cascade_t* cascade,
                                  const uint8_t* data, int data_size,
                                  eif_memory_pool_t* pool);

/**
 * @brief Multi-scale cascade detection
 * 
 * @param img Input image (grayscale)
 * @param cascade Trained cascade
 * @param scale_factor Scale factor between levels (typically 1.1-1.3)
 * @param min_neighbors Minimum neighbors for grouping
 * @param min_size Minimum detection size
 * @param detections Output detections
 * @param max_detections Maximum detections
 * @param pool Memory pool
 * @return Number of detections
 */
int eif_cv_cascade_detect(const eif_cv_image_t* img,
                           const eif_cv_cascade_t* cascade,
                           float32_t scale_factor, int min_neighbors,
                           const eif_cv_rect_t* min_size,
                           eif_cv_detection_t* detections, int max_detections,
                           eif_memory_pool_t* pool);

// ============================================================================
// HOG Detector
// ============================================================================

/**
 * @brief HOG + Linear SVM detector
 */
typedef struct {
    eif_cv_hog_config_t hog_config;
    float32_t* svm_weights;     // SVM weight vector
    float32_t svm_bias;         // SVM bias
    int descriptor_size;        // HOG descriptor size
} eif_cv_hog_detector_t;

/**
 * @brief Initialize HOG detector with pre-trained weights
 */
eif_status_t eif_cv_hog_detector_init(eif_cv_hog_detector_t* detector,
                                       const eif_cv_hog_config_t* config,
                                       const float32_t* weights, float32_t bias);

/**
 * @brief Load HOG detector from binary data
 */
eif_status_t eif_cv_hog_detector_load(eif_cv_hog_detector_t* detector,
                                       const uint8_t* data, int data_size,
                                       eif_memory_pool_t* pool);

/**
 * @brief HOG multi-scale detection
 * 
 * @param img Input grayscale image
 * @param detector HOG detector
 * @param hit_threshold SVM threshold (typically 0)
 * @param win_stride Window stride
 * @param scale_factor Scale factor
 * @param detections Output detections
 * @param max_detections Maximum detections
 * @param pool Memory pool
 * @return Number of detections
 */
int eif_cv_hog_detect(const eif_cv_image_t* img,
                       const eif_cv_hog_detector_t* detector,
                       float32_t hit_threshold, int win_stride,
                       float32_t scale_factor,
                       eif_cv_detection_t* detections, int max_detections,
                       eif_memory_pool_t* pool);

// ============================================================================
// Integral Image
// ============================================================================

/**
 * @brief Compute integral image (summed area table)
 * 
 * Enables O(1) rectangle sum computation.
 * 
 * @param src Input image
 * @param sum Output integral image (int32, size (w+1)*(h+1))
 * @param sqsum Output squared integral (optional, for variance)
 */
eif_status_t eif_cv_integral(const eif_cv_image_t* src,
                              int32_t* sum, int64_t* sqsum);

/**
 * @brief Compute rectangle sum from integral image
 */
static inline int32_t eif_cv_integral_rect_sum(const int32_t* integral,
                                                int img_width,
                                                int x, int y, int w, int h) {
    int stride = img_width + 1;
    return integral[(y + h) * stride + (x + w)]
         - integral[y * stride + (x + w)]
         - integral[(y + h) * stride + x]
         + integral[y * stride + x];
}

// ============================================================================
// Non-Maximum Suppression
// ============================================================================

/**
 * @brief Non-maximum suppression for detection boxes
 * 
 * @param detections Input/output detections (sorted by confidence)
 * @param num_detections Number of input detections
 * @param iou_threshold IoU threshold for suppression
 * @return Number of remaining detections
 */
int eif_cv_nms(eif_cv_detection_t* detections, int num_detections,
                float32_t iou_threshold);

/**
 * @brief Compute IoU (Intersection over Union) of two rectangles
 */
float32_t eif_cv_iou(const eif_cv_rect_t* a, const eif_cv_rect_t* b);

#endif // EIF_CV_DETECT_H
