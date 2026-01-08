/**
 * @file eif_cv_image.h
 * @brief Image Representation and Basic Operations
 */

#ifndef EIF_CV_IMAGE_H
#define EIF_CV_IMAGE_H

#include "eif_types.h"
#include "eif_status.h"
#include "eif_memory.h"

// ============================================================================
// Image Format Definitions
// ============================================================================

typedef enum {
    EIF_CV_GRAY8,       // 8-bit grayscale
    EIF_CV_RGB888,      // 24-bit RGB (R,G,B packed)
    EIF_CV_RGBA8888,    // 32-bit RGBA
    EIF_CV_YUV420P,     // YUV 4:2:0 planar
    EIF_CV_BINARY       // 1-bit binary (packed bytes)
} eif_cv_format_t;

/**
 * @brief Image structure
 */
typedef struct {
    uint8_t* data;          // Pixel data
    int width;              // Image width
    int height;             // Image height
    int stride;             // Bytes per row (may include padding)
    int channels;           // Number of channels
    eif_cv_format_t format; // Pixel format
} eif_cv_image_t;

/**
 * @brief Rectangle structure
 */
typedef struct {
    int x, y;               // Top-left corner
    int width, height;      // Size
} eif_cv_rect_t;

/**
 * @brief Point structure
 */
typedef struct {
    int x, y;
} eif_cv_point_t;

/**
 * @brief Floating-point point
 */
typedef struct {
    float32_t x, y;
} eif_cv_point2f_t;

/**
 * @brief Color (BGR order for compatibility)
 */
typedef struct {
    uint8_t b, g, r, a;
} eif_cv_color_t;

// ============================================================================
// Image Creation and Management
// ============================================================================

/**
 * @brief Create a new image
 */
eif_status_t eif_cv_image_create(eif_cv_image_t* img, int width, int height,
                                  eif_cv_format_t format, eif_memory_pool_t* pool);

/**
 * @brief Wrap existing buffer as image (no copy)
 */
eif_status_t eif_cv_image_from_buffer(eif_cv_image_t* img, uint8_t* buffer,
                                       int width, int height, int stride,
                                       eif_cv_format_t format);

/**
 * @brief Clone an image (deep copy)
 */
eif_status_t eif_cv_image_clone(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                 eif_memory_pool_t* pool);

/**
 * @brief Get bytes per pixel for format
 */
int eif_cv_format_bpp(eif_cv_format_t format);

// ============================================================================
// Color Space Conversion
// ============================================================================

/**
 * @brief Convert RGB to grayscale
 */
eif_status_t eif_cv_rgb_to_gray(const eif_cv_image_t* src, eif_cv_image_t* dst);

/**
 * @brief Convert grayscale to RGB
 */
eif_status_t eif_cv_gray_to_rgb(const eif_cv_image_t* src, eif_cv_image_t* dst);

/**
 * @brief Convert RGB to YUV
 */
eif_status_t eif_cv_rgb_to_yuv(const eif_cv_image_t* src, eif_cv_image_t* dst);

/**
 * @brief Convert YUV to RGB
 */
eif_status_t eif_cv_yuv_to_rgb(const eif_cv_image_t* src, eif_cv_image_t* dst);

// ============================================================================
// Geometric Operations
// ============================================================================

/**
 * @brief Resize interpolation method
 */
typedef enum {
    EIF_CV_INTER_NEAREST,   // Nearest neighbor
    EIF_CV_INTER_BILINEAR,  // Bilinear interpolation
    EIF_CV_INTER_AREA       // Area averaging (for downscale)
} eif_cv_interpolation_t;

/**
 * @brief Resize image
 */
eif_status_t eif_cv_resize(const eif_cv_image_t* src, eif_cv_image_t* dst,
                            int new_width, int new_height,
                            eif_cv_interpolation_t interp,
                            eif_memory_pool_t* pool);

/**
 * @brief Crop a region of interest
 */
eif_status_t eif_cv_crop(const eif_cv_image_t* src, eif_cv_image_t* dst,
                          const eif_cv_rect_t* roi, eif_memory_pool_t* pool);

/**
 * @brief Flip image
 */
typedef enum {
    EIF_CV_FLIP_HORIZONTAL,
    EIF_CV_FLIP_VERTICAL,
    EIF_CV_FLIP_BOTH
} eif_cv_flip_mode_t;

eif_status_t eif_cv_flip(const eif_cv_image_t* src, eif_cv_image_t* dst,
                          eif_cv_flip_mode_t mode);

// ============================================================================
// Pixel Access Utilities
// ============================================================================

/**
 * @brief Get pixel value (grayscale)
 */
static inline uint8_t eif_cv_get_pixel(const eif_cv_image_t* img, int x, int y) {
    return img->data[y * img->stride + x];
}

/**
 * @brief Set pixel value (grayscale)
 */
static inline void eif_cv_set_pixel(eif_cv_image_t* img, int x, int y, uint8_t val) {
    img->data[y * img->stride + x] = val;
}

/**
 * @brief Get pixel pointer for RGB
 */
static inline uint8_t* eif_cv_get_pixel_rgb(const eif_cv_image_t* img, int x, int y) {
    return &img->data[y * img->stride + x * 3];
}

// ============================================================================
// Drawing Primitives
// ============================================================================

/**
 * @brief Draw rectangle
 */
eif_status_t eif_cv_draw_rect(eif_cv_image_t* img, const eif_cv_rect_t* rect,
                               eif_cv_color_t color, int thickness);

/**
 * @brief Draw circle
 */
eif_status_t eif_cv_draw_circle(eif_cv_image_t* img, int cx, int cy, int radius,
                                 eif_cv_color_t color, int thickness);

/**
 * @brief Draw line
 */
eif_status_t eif_cv_draw_line(eif_cv_image_t* img, int x1, int y1, int x2, int y2,
                               eif_cv_color_t color, int thickness);

/**
 * @brief Fill image with value
 */
eif_status_t eif_cv_fill(eif_cv_image_t* img, uint8_t value);

#endif // EIF_CV_IMAGE_H
