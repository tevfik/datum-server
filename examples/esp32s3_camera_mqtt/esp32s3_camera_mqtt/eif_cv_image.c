/**
 * @file eif_cv_image.c
 * @brief Image Representation and Basic Operations
 */

#include "eif_cv_image.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

// ============================================================================
// Image Format Utilities
// ============================================================================

int eif_cv_format_bpp(eif_cv_format_t format) {
    switch (format) {
        case EIF_CV_GRAY8:    return 1;
        case EIF_CV_RGB888:   return 3;
        case EIF_CV_RGBA8888: return 4;
        case EIF_CV_YUV420P:  return 1;  // Per plane
        case EIF_CV_BINARY:   return 1;  // Packed bits
        default: return 1;
    }
}

static int format_channels(eif_cv_format_t format) {
    switch (format) {
        case EIF_CV_GRAY8:    return 1;
        case EIF_CV_RGB888:   return 3;
        case EIF_CV_RGBA8888: return 4;
        case EIF_CV_YUV420P:  return 3;
        case EIF_CV_BINARY:   return 1;
        default: return 1;
    }
}

// ============================================================================
// Image Creation
// ============================================================================

eif_status_t eif_cv_image_create(eif_cv_image_t* img, int width, int height,
                                  eif_cv_format_t format, eif_memory_pool_t* pool) {
    if (!img || !pool || width <= 0 || height <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    int bpp = eif_cv_format_bpp(format);
    int stride = width * bpp;
    // Align stride to 4 bytes
    stride = (stride + 3) & ~3;
    
    int data_size = stride * height;
    if (format == EIF_CV_YUV420P) {
        data_size = width * height * 3 / 2;  // Y + U/4 + V/4
    }
    
    img->data = eif_memory_alloc(pool, data_size, 4);
    if (!img->data) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    img->width = width;
    img->height = height;
    img->stride = stride;
    img->channels = format_channels(format);
    img->format = format;
    
    memset(img->data, 0, data_size);
    
    return EIF_STATUS_OK;
}

eif_status_t eif_cv_image_from_buffer(eif_cv_image_t* img, uint8_t* buffer,
                                       int width, int height, int stride,
                                       eif_cv_format_t format) {
    if (!img || !buffer || width <= 0 || height <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    img->data = buffer;
    img->width = width;
    img->height = height;
    img->stride = stride > 0 ? stride : width * eif_cv_format_bpp(format);
    img->channels = format_channels(format);
    img->format = format;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_cv_image_clone(const eif_cv_image_t* src, eif_cv_image_t* dst,
                                 eif_memory_pool_t* pool) {
    if (!src || !dst || !pool) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    eif_status_t status = eif_cv_image_create(dst, src->width, src->height,
                                               src->format, pool);
    if (status != EIF_STATUS_OK) return status;
    
    // Copy row by row (handles different strides)
    int row_bytes = src->width * eif_cv_format_bpp(src->format);
    for (int y = 0; y < src->height; y++) {
        memcpy(&dst->data[y * dst->stride], &src->data[y * src->stride], row_bytes);
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Color Space Conversion
// ============================================================================

eif_status_t eif_cv_rgb_to_gray(const eif_cv_image_t* src, eif_cv_image_t* dst) {
    if (!src || !dst || src->format != EIF_CV_RGB888 || dst->format != EIF_CV_GRAY8) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    if (src->width != dst->width || src->height != dst->height) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    for (int y = 0; y < src->height; y++) {
        const uint8_t* src_row = &src->data[y * src->stride];
        uint8_t* dst_row = &dst->data[y * dst->stride];
        
        for (int x = 0; x < src->width; x++) {
            // ITU-R BT.601: Y = 0.299*R + 0.587*G + 0.114*B
            int r = src_row[x * 3 + 0];
            int g = src_row[x * 3 + 1];
            int b = src_row[x * 3 + 2];
            dst_row[x] = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_cv_gray_to_rgb(const eif_cv_image_t* src, eif_cv_image_t* dst) {
    if (!src || !dst || src->format != EIF_CV_GRAY8 || dst->format != EIF_CV_RGB888) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    for (int y = 0; y < src->height; y++) {
        const uint8_t* src_row = &src->data[y * src->stride];
        uint8_t* dst_row = &dst->data[y * dst->stride];
        
        for (int x = 0; x < src->width; x++) {
            uint8_t gray = src_row[x];
            dst_row[x * 3 + 0] = gray;
            dst_row[x * 3 + 1] = gray;
            dst_row[x * 3 + 2] = gray;
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_cv_rgb_to_yuv(const eif_cv_image_t* src, eif_cv_image_t* dst) {
    // Simplified version: convert to Y only for grayscale
    return eif_cv_rgb_to_gray(src, dst);
}

eif_status_t eif_cv_yuv_to_rgb(const eif_cv_image_t* src, eif_cv_image_t* dst) {
    return eif_cv_gray_to_rgb(src, dst);
}

// ============================================================================
// Geometric Operations
// ============================================================================

eif_status_t eif_cv_resize(const eif_cv_image_t* src, eif_cv_image_t* dst,
                            int new_width, int new_height,
                            eif_cv_interpolation_t interp,
                            eif_memory_pool_t* pool) {
    if (!src || !dst || new_width <= 0 || new_height <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    if (dst->data == NULL) {
        eif_status_t status = eif_cv_image_create(dst, new_width, new_height,
                                                   src->format, pool);
        if (status != EIF_STATUS_OK) return status;
    }
    
    float scale_x = (float)src->width / new_width;
    float scale_y = (float)src->height / new_height;
    int bpp = eif_cv_format_bpp(src->format);
    
    for (int y = 0; y < new_height; y++) {
        for (int x = 0; x < new_width; x++) {
            float src_x = x * scale_x;
            float src_y = y * scale_y;
            
            if (interp == EIF_CV_INTER_NEAREST) {
                int sx = (int)(src_x + 0.5f);
                int sy = (int)(src_y + 0.5f);
                if (sx >= src->width) sx = src->width - 1;
                if (sy >= src->height) sy = src->height - 1;
                
                for (int c = 0; c < bpp; c++) {
                    dst->data[y * dst->stride + x * bpp + c] =
                        src->data[sy * src->stride + sx * bpp + c];
                }
            } else {
                // Bilinear interpolation
                int x0 = (int)src_x;
                int y0 = (int)src_y;
                int x1 = x0 + 1;
                int y1 = y0 + 1;
                
                if (x1 >= src->width) x1 = src->width - 1;
                if (y1 >= src->height) y1 = src->height - 1;
                
                float fx = src_x - x0;
                float fy = src_y - y0;
                
                for (int c = 0; c < bpp; c++) {
                    float v00 = src->data[y0 * src->stride + x0 * bpp + c];
                    float v01 = src->data[y0 * src->stride + x1 * bpp + c];
                    float v10 = src->data[y1 * src->stride + x0 * bpp + c];
                    float v11 = src->data[y1 * src->stride + x1 * bpp + c];
                    
                    float v0 = v00 * (1 - fx) + v01 * fx;
                    float v1 = v10 * (1 - fx) + v11 * fx;
                    float v = v0 * (1 - fy) + v1 * fy;
                    
                    dst->data[y * dst->stride + x * bpp + c] = (uint8_t)(v + 0.5f);
                }
            }
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_cv_crop(const eif_cv_image_t* src, eif_cv_image_t* dst,
                          const eif_cv_rect_t* roi, eif_memory_pool_t* pool) {
    if (!src || !dst || !roi || !pool) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    if (roi->x < 0 || roi->y < 0 || roi->width <= 0 || roi->height <= 0 ||
        roi->x + roi->width > src->width || roi->y + roi->height > src->height) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    eif_status_t status = eif_cv_image_create(dst, roi->width, roi->height,
                                               src->format, pool);
    if (status != EIF_STATUS_OK) return status;
    
    int bpp = eif_cv_format_bpp(src->format);
    int row_bytes = roi->width * bpp;
    
    for (int y = 0; y < roi->height; y++) {
        memcpy(&dst->data[y * dst->stride],
               &src->data[(roi->y + y) * src->stride + roi->x * bpp],
               row_bytes);
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_cv_flip(const eif_cv_image_t* src, eif_cv_image_t* dst,
                          eif_cv_flip_mode_t mode) {
    if (!src || !dst) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    int bpp = eif_cv_format_bpp(src->format);
    
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            int src_x = x, src_y = y;
            
            if (mode == EIF_CV_FLIP_HORIZONTAL || mode == EIF_CV_FLIP_BOTH) {
                src_x = src->width - 1 - x;
            }
            if (mode == EIF_CV_FLIP_VERTICAL || mode == EIF_CV_FLIP_BOTH) {
                src_y = src->height - 1 - y;
            }
            
            for (int c = 0; c < bpp; c++) {
                dst->data[y * dst->stride + x * bpp + c] =
                    src->data[src_y * src->stride + src_x * bpp + c];
            }
        }
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Drawing Primitives
// ============================================================================

eif_status_t eif_cv_fill(eif_cv_image_t* img, uint8_t value) {
    if (!img) return EIF_STATUS_INVALID_ARGUMENT;
    
    for (int y = 0; y < img->height; y++) {
        memset(&img->data[y * img->stride], value, img->width * eif_cv_format_bpp(img->format));
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_cv_draw_rect(eif_cv_image_t* img, const eif_cv_rect_t* rect,
                               eif_cv_color_t color, int thickness) {
    if (!img || !rect) return EIF_STATUS_INVALID_ARGUMENT;
    
    int x1 = rect->x;
    int y1 = rect->y;
    int x2 = rect->x + rect->width - 1;
    int y2 = rect->y + rect->height - 1;
    
    // Clamp to image bounds
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= img->width) x2 = img->width - 1;
    if (y2 >= img->height) y2 = img->height - 1;
    
    uint8_t gray = (color.r * 77 + color.g * 150 + color.b * 29) >> 8;
    
    if (img->format == EIF_CV_GRAY8) {
        // Draw horizontal lines
        for (int t = 0; t < thickness; t++) {
            if (y1 + t < img->height) {
                for (int x = x1; x <= x2; x++) {
                    img->data[(y1 + t) * img->stride + x] = gray;
                }
            }
            if (y2 - t >= 0) {
                for (int x = x1; x <= x2; x++) {
                    img->data[(y2 - t) * img->stride + x] = gray;
                }
            }
        }
        // Draw vertical lines
        for (int t = 0; t < thickness; t++) {
            for (int y = y1; y <= y2; y++) {
                if (x1 + t < img->width) {
                    img->data[y * img->stride + x1 + t] = gray;
                }
                if (x2 - t >= 0) {
                    img->data[y * img->stride + x2 - t] = gray;
                }
            }
        }
    } else if (img->format == EIF_CV_RGB888) {
        // Similar but for RGB
        for (int t = 0; t < thickness; t++) {
            if (y1 + t < img->height) {
                for (int x = x1; x <= x2; x++) {
                    uint8_t* p = &img->data[(y1 + t) * img->stride + x * 3];
                    p[0] = color.r; p[1] = color.g; p[2] = color.b;
                }
            }
            if (y2 - t >= 0) {
                for (int x = x1; x <= x2; x++) {
                    uint8_t* p = &img->data[(y2 - t) * img->stride + x * 3];
                    p[0] = color.r; p[1] = color.g; p[2] = color.b;
                }
            }
        }
        for (int t = 0; t < thickness; t++) {
            for (int y = y1; y <= y2; y++) {
                if (x1 + t < img->width) {
                    uint8_t* p = &img->data[y * img->stride + (x1 + t) * 3];
                    p[0] = color.r; p[1] = color.g; p[2] = color.b;
                }
                if (x2 - t >= 0) {
                    uint8_t* p = &img->data[y * img->stride + (x2 - t) * 3];
                    p[0] = color.r; p[1] = color.g; p[2] = color.b;
                }
            }
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_cv_draw_circle(eif_cv_image_t* img, int cx, int cy, int radius,
                                 eif_cv_color_t color, int thickness) {
    if (!img || radius <= 0) return EIF_STATUS_INVALID_ARGUMENT;
    
    uint8_t gray = (color.r * 77 + color.g * 150 + color.b * 29) >> 8;
    
    // Midpoint circle algorithm
    int x = 0, y = radius;
    int d = 1 - radius;
    
    while (x <= y) {
        // Draw 8 symmetric points
        int points[8][2] = {
            {cx + x, cy + y}, {cx - x, cy + y},
            {cx + x, cy - y}, {cx - x, cy - y},
            {cx + y, cy + x}, {cx - y, cy + x},
            {cx + y, cy - x}, {cx - y, cy - x}
        };
        
        for (int i = 0; i < 8; i++) {
            int px = points[i][0];
            int py = points[i][1];
            if (px >= 0 && px < img->width && py >= 0 && py < img->height) {
                if (img->format == EIF_CV_GRAY8) {
                    img->data[py * img->stride + px] = gray;
                } else if (img->format == EIF_CV_RGB888) {
                    uint8_t* p = &img->data[py * img->stride + px * 3];
                    p[0] = color.r; p[1] = color.g; p[2] = color.b;
                }
            }
        }
        
        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_cv_draw_line(eif_cv_image_t* img, int x1, int y1, int x2, int y2,
                               eif_cv_color_t color, int thickness) {
    if (!img) return EIF_STATUS_INVALID_ARGUMENT;
    
    uint8_t gray = (color.r * 77 + color.g * 150 + color.b * 29) >> 8;
    
    // Bresenham's line algorithm
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        if (x1 >= 0 && x1 < img->width && y1 >= 0 && y1 < img->height) {
            if (img->format == EIF_CV_GRAY8) {
                img->data[y1 * img->stride + x1] = gray;
            } else if (img->format == EIF_CV_RGB888) {
                uint8_t* p = &img->data[y1 * img->stride + x1 * 3];
                p[0] = color.r; p[1] = color.g; p[2] = color.b;
            }
        }
        
        if (x1 == x2 && y1 == y2) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
    
    return EIF_STATUS_OK;
}
