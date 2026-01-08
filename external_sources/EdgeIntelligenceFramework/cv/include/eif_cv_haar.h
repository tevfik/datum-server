#ifndef EIF_CV_HAAR_H
#define EIF_CV_HAAR_H

#include "eif_cv_image.h"
#include "eif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Integral Image (Summed Area Table)
// =============================================================================

typedef struct {
    uint32_t* data; // 32-bit sums (assuming < 16MP image)
    int width;
    int height;
} eif_integral_image_t;

/**
 * @brief Compute Integral Image
 */
void eif_cv_compute_integral(const eif_cv_image_t* src, eif_integral_image_t* dst);

/**
 * @brief Get sum of rectangle from integral image
 * O(1) operation
 * @param ii Integral image
 * @param x Top-left X
 * @param y Top-left Y
 * @param w Width
 * @param h Height
 */
static inline uint32_t eif_cv_integral_sum(const eif_integral_image_t* ii, int x, int y, int w, int h) {
    // S(D) + S(A) - S(B) - S(C) logic
    // A=(x,y), B=(x+w,y), C=(x,y+h), D=(x+w,y+h)
    // Note: Integral image usually has +1 width/height or handles coords carefully.
    // Here we assume standard integer coords where I(x,y) contains sum above and left of (x,y) inclusive.
    // For fast calc, P4 + P1 - P2 - P3
    // Indices in flat array:
    // P4 = data[(y+h)*width + (x+w)]
    // ...
    // Using safe logic:
    
    int stride = ii->width;
    uint32_t p4 = ii->data[(y + h - 1) * stride + (x + w - 1)];
    uint32_t p1 = (x > 0 && y > 0) ? ii->data[(y - 1) * stride + (x - 1)] : 0;
    uint32_t p2 = (y > 0) ? ii->data[(y - 1) * stride + (x + w - 1)] : 0;
    uint32_t p3 = (x > 0) ? ii->data[(y + h - 1) * stride + (x - 1)] : 0;
    
    return p4 + p1 - p2 - p3;
}

// =============================================================================
// Haar Cascade Classifier Structures (Simplified)
// =============================================================================

typedef struct {
    int x, y, w, h;
    float weight;
} eif_haar_rect_t;

typedef struct {
    eif_haar_rect_t rects[3]; // Up to 3 rectangles per feature
    int rect_count;
    float threshold;
    float left_val;
    float right_val;
} eif_weak_classifier_t;

typedef struct {
    eif_weak_classifier_t* weaks;
    int weak_count;
    float stage_threshold;
} eif_haar_stage_t;

typedef struct {
    eif_haar_stage_t* stages;
    int stage_count;
    int window_w;
    int window_h;
} eif_haar_cascade_t;

/**
 * @brief Run Haar Cascade detection
 * 
 * @param ii Integral image of source
 * @param cascade Loaded cascade
 * @param objects Array to store detected rectangles
 * @param max_objects Size of objects array
 * @return Number of detections
 */
int eif_cv_haar_detect(const eif_integral_image_t* ii, 
                       const eif_haar_cascade_t* cascade,
                       eif_cv_rect_t* objects, 
                       int max_objects);

eif_status_t eif_cv_haar_load_fake_face(eif_haar_cascade_t* cascade);

#ifdef __cplusplus
}
#endif

#endif // EIF_CV_HAAR_H
