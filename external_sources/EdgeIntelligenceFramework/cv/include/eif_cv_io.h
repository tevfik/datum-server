/**
 * @file eif_cv_io.h
 * @brief Image I/O for Visualization
 * 
 * Provides PPM/PGM image output for cross-platform visualization.
 * Zero external dependencies - works on embedded systems.
 */

#ifndef EIF_CV_IO_H
#define EIF_CV_IO_H

#include "eif_cv_image.h"
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// File-Based Output (Linux/Windows)
// ============================================================================

/**
 * @brief Write grayscale image to PGM file
 * 
 * @param img Grayscale image (EIF_CV_GRAY8)
 * @param filename Output filename (e.g., "output.pgm")
 * @return Status code
 */
static inline eif_status_t eif_cv_write_pgm(const eif_cv_image_t* img, 
                                             const char* filename) {
    if (!img || !filename || img->format != EIF_CV_GRAY8) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    FILE* fp = fopen(filename, "wb");
    if (!fp) return EIF_STATUS_INVALID_ARGUMENT;
    
    // PGM header
    fprintf(fp, "P5\n%d %d\n255\n", img->width, img->height);
    
    // Pixel data (row by row to handle stride)
    for (int y = 0; y < img->height; y++) {
        fwrite(&img->data[y * img->stride], 1, img->width, fp);
    }
    
    fclose(fp);
    return EIF_STATUS_OK;
}

/**
 * @brief Write RGB image to PPM file
 * 
 * @param img RGB image (EIF_CV_RGB888)
 * @param filename Output filename (e.g., "output.ppm")
 * @return Status code
 */
static inline eif_status_t eif_cv_write_ppm(const eif_cv_image_t* img,
                                             const char* filename) {
    if (!img || !filename) return EIF_STATUS_INVALID_ARGUMENT;
    
    FILE* fp = fopen(filename, "wb");
    if (!fp) return EIF_STATUS_INVALID_ARGUMENT;
    
    // PPM header
    fprintf(fp, "P6\n%d %d\n255\n", img->width, img->height);
    
    if (img->format == EIF_CV_RGB888) {
        for (int y = 0; y < img->height; y++) {
            fwrite(&img->data[y * img->stride], 1, img->width * 3, fp);
        }
    } else if (img->format == EIF_CV_GRAY8) {
        // Convert grayscale to RGB on-the-fly
        for (int y = 0; y < img->height; y++) {
            for (int x = 0; x < img->width; x++) {
                uint8_t gray = img->data[y * img->stride + x];
                uint8_t rgb[3] = {gray, gray, gray};
                fwrite(rgb, 1, 3, fp);
            }
        }
    }
    
    fclose(fp);
    return EIF_STATUS_OK;
}

/**
 * @brief Write image with overlaid keypoints
 */
static inline eif_status_t eif_cv_write_with_keypoints(const eif_cv_image_t* img,
                                                        const eif_cv_keypoint_t* kpts,
                                                        int num_kpts,
                                                        const char* filename) {
    if (!img || !filename) return EIF_STATUS_INVALID_ARGUMENT;
    
    FILE* fp = fopen(filename, "wb");
    if (!fp) return EIF_STATUS_INVALID_ARGUMENT;
    
    fprintf(fp, "P6\n%d %d\n255\n", img->width, img->height);
    
    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            uint8_t r, g, b;
            
            // Check if near a keypoint
            int is_keypoint = 0;
            for (int k = 0; k < num_kpts && !is_keypoint; k++) {
                int kx = (int)kpts[k].x;
                int ky = (int)kpts[k].y;
                if (abs(x - kx) <= 2 && abs(y - ky) <= 2) {
                    is_keypoint = 1;
                }
            }
            
            if (is_keypoint) {
                r = 255; g = 0; b = 0;  // Red for keypoints
            } else if (img->format == EIF_CV_GRAY8) {
                uint8_t gray = img->data[y * img->stride + x];
                r = g = b = gray;
            } else {
                r = img->data[y * img->stride + x * 3];
                g = img->data[y * img->stride + x * 3 + 1];
                b = img->data[y * img->stride + x * 3 + 2];
            }
            
            uint8_t rgb[3] = {r, g, b};
            fwrite(rgb, 1, 3, fp);
        }
    }
    
    fclose(fp);
    return EIF_STATUS_OK;
}

// ============================================================================
// Buffer-Based Output (Embedded/UART)
// ============================================================================

/**
 * @brief Get required buffer size for PGM output
 */
static inline size_t eif_cv_pgm_buffer_size(const eif_cv_image_t* img) {
    // Header (max ~30 bytes) + pixel data
    return 32 + img->width * img->height;
}

/**
 * @brief Write PGM to memory buffer
 * 
 * @param img Input image
 * @param buffer Output buffer
 * @param buf_size Buffer size
 * @param out_size Actual bytes written
 * @return Status code
 */
static inline eif_status_t eif_cv_write_pgm_buffer(const eif_cv_image_t* img,
                                                    uint8_t* buffer, size_t buf_size,
                                                    size_t* out_size) {
    if (!img || !buffer || img->format != EIF_CV_GRAY8) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    size_t needed = eif_cv_pgm_buffer_size(img);
    if (buf_size < needed) return EIF_STATUS_OUT_OF_MEMORY;
    
    // Write header - use snprintf with bounds checking
    int header_len = snprintf((char*)buffer, buf_size, "P5\n%d %d\n255\n", img->width, img->height);
    if (header_len < 0 || header_len >= (int)buf_size) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    // Write pixel data
    uint8_t* ptr = buffer + header_len;
    for (int y = 0; y < img->height; y++) {
        memcpy(ptr, &img->data[y * img->stride], img->width);
        ptr += img->width;
    }
    
    *out_size = ptr - buffer;
    return EIF_STATUS_OK;
}

// ============================================================================
// UART Streaming Protocol (Future: add heatshrink compression)
// ============================================================================

#define EIF_CV_FRAME_START  0xAA
#define EIF_CV_FRAME_END    0x55

/**
 * @brief Frame header for UART streaming
 */
typedef struct {
    uint8_t start_marker;   // 0xAA
    uint16_t width;
    uint16_t height;
    uint8_t format;         // 0=Gray, 1=RGB
    uint32_t data_size;
    uint8_t checksum;
} __attribute__((packed)) eif_cv_uart_header_t;

/**
 * @brief Calculate simple checksum
 */
static inline uint8_t eif_cv_checksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}

#endif // EIF_CV_IO_H
