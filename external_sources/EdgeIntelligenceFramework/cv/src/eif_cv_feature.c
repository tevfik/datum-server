/**
 * @file eif_cv_feature.c
 * @brief Feature Detection and Description
 */

#include "eif_cv_feature.h"
#include "eif_cv_filter.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// FAST Corner Detector
// ============================================================================

// FAST-9 circle offsets (16 pixels around center)
static const int fast_circle_x[16] = { 3, 3, 2, 1, 0,-1,-2,-3,-3,-3,-2,-1, 0, 1, 2, 3};
static const int fast_circle_y[16] = { 0,-1,-2,-3,-3,-3,-2,-1, 0, 1, 2, 3, 3, 3, 2, 1};

static bool is_corner_fast9(const eif_cv_image_t* img, int x, int y, int threshold) {
    uint8_t center = eif_cv_get_pixel(img, x, y);
    int high = center + threshold;
    int low = center - threshold;
    
    // Quick test: check pixels at positions 1, 5, 9, 13 (90 degrees apart)
    int count_bright = 0;
    int count_dark = 0;
    
    for (int i = 0; i < 16; i += 4) {
        int px = x + fast_circle_x[i];
        int py = y + fast_circle_y[i];
        
        if (px < 0 || px >= img->width || py < 0 || py >= img->height) continue;
        
        uint8_t val = eif_cv_get_pixel(img, px, py);
        if (val > high) count_bright++;
        else if (val < low) count_dark++;
    }
    
    // Need at least 3 of 4 to be potential corner
    if (count_bright < 3 && count_dark < 3) return false;
    
    // Full test: check for 9 contiguous pixels
    count_bright = 0;
    count_dark = 0;
    
    int values[16];
    for (int i = 0; i < 16; i++) {
        int px = x + fast_circle_x[i];
        int py = y + fast_circle_y[i];
        
        if (px < 0 || px >= img->width || py < 0 || py >= img->height) {
            values[i] = center;
        } else {
            values[i] = eif_cv_get_pixel(img, px, py);
        }
    }
    
    // Check for 9 contiguous bright pixels
    int max_bright = 0, max_dark = 0;
    int cur_bright = 0, cur_dark = 0;
    
    for (int i = 0; i < 32; i++) {
        int val = values[i % 16];
        
        if (val > high) {
            cur_bright++;
            cur_dark = 0;
        } else if (val < low) {
            cur_dark++;
            cur_bright = 0;
        } else {
            cur_bright = 0;
            cur_dark = 0;
        }
        
        if (cur_bright > max_bright) max_bright = cur_bright;
        if (cur_dark > max_dark) max_dark = cur_dark;
    }
    
    return max_bright >= 9 || max_dark >= 9;
}

static float32_t corner_response_fast(const eif_cv_image_t* img, int x, int y, int threshold) {
    uint8_t center = eif_cv_get_pixel(img, x, y);
    float32_t response = 0;
    
    for (int i = 0; i < 16; i++) {
        int px = x + fast_circle_x[i];
        int py = y + fast_circle_y[i];
        
        if (px >= 0 && px < img->width && py >= 0 && py < img->height) {
            int diff = (int)eif_cv_get_pixel(img, px, py) - center;
            if (abs(diff) > threshold) {
                response += diff * diff;
            }
        }
    }
    
    return response;
}

int eif_cv_detect_fast(const eif_cv_image_t* src, int threshold,
                        bool nonmax_suppression,
                        eif_cv_keypoint_t* corners, int max_corners) {
    if (!src || !corners || max_corners <= 0 || src->format != EIF_CV_GRAY8) {
        return 0;
    }
    
    int count = 0;
    
    // Detect corners
    for (int y = 3; y < src->height - 3; y++) {
        for (int x = 3; x < src->width - 3; x++) {
            if (is_corner_fast9(src, x, y, threshold)) {
                if (count < max_corners) {
                    corners[count].x = (float32_t)x;
                    corners[count].y = (float32_t)y;
                    corners[count].response = corner_response_fast(src, x, y, threshold);
                    corners[count].size = 7.0f;
                    corners[count].angle = 0.0f;
                    corners[count].octave = 0;
                    corners[count].class_id = -1;
                    count++;
                }
            }
        }
    }
    
    // Non-maximum suppression
    if (nonmax_suppression && count > 1) {
        // Simple NMS: mark weaker neighbors
        for (int i = 0; i < count; i++) {
            if (corners[i].response < 0) continue;
            
            for (int j = i + 1; j < count; j++) {
                if (corners[j].response < 0) continue;
                
                float32_t dx = corners[i].x - corners[j].x;
                float32_t dy = corners[i].y - corners[j].y;
                float32_t dist = dx * dx + dy * dy;
                
                if (dist < 9) {  // Within 3 pixels
                    if (corners[i].response > corners[j].response) {
                        corners[j].response = -1;
                    } else {
                        corners[i].response = -1;
                        break;
                    }
                }
            }
        }
        
        // Compact array
        int new_count = 0;
        for (int i = 0; i < count; i++) {
            if (corners[i].response >= 0) {
                if (new_count != i) {
                    corners[new_count] = corners[i];
                }
                new_count++;
            }
        }
        count = new_count;
    }
    
    return count;
}

// ============================================================================
// Harris Corner Detector
// ============================================================================

int eif_cv_detect_harris(const eif_cv_image_t* src, int block_size, int ksize,
                          float32_t k, float32_t threshold,
                          eif_cv_keypoint_t* corners, int max_corners,
                          eif_memory_pool_t* pool) {
    if (!src || !corners || !pool) return 0;
    
    int w = src->width, h = src->height;
    
    // Allocate gradient images
    eif_cv_image_t Ix, Iy;
    eif_cv_image_create(&Ix, w, h, EIF_CV_GRAY8, pool);
    eif_cv_image_create(&Iy, w, h, EIF_CV_GRAY8, pool);
    
    // Compute gradients
    eif_cv_sobel(src, &Ix, 1, 0, ksize);
    eif_cv_sobel(src, &Iy, 0, 1, ksize);
    
    // Allocate response buffer
    float32_t* response = eif_memory_alloc(pool, w * h * sizeof(float32_t), 4);
    if (!response) return 0;
    
    int radius = block_size / 2;
    
    // Compute Harris response
    for (int y = radius; y < h - radius; y++) {
        for (int x = radius; x < w - radius; x++) {
            float32_t Ixx = 0, Iyy = 0, Ixy = 0;
            
            for (int ky = -radius; ky <= radius; ky++) {
                for (int kx = -radius; kx <= radius; kx++) {
                    float32_t gx = (float32_t)(Ix.data[(y + ky) * Ix.stride + (x + kx)] - 128);
                    float32_t gy = (float32_t)(Iy.data[(y + ky) * Iy.stride + (x + kx)] - 128);
                    
                    Ixx += gx * gx;
                    Iyy += gy * gy;
                    Ixy += gx * gy;
                }
            }
            
            // Harris corner response: det(M) - k * trace(M)^2
            float32_t det = Ixx * Iyy - Ixy * Ixy;
            float32_t trace = Ixx + Iyy;
            response[y * w + x] = det - k * trace * trace;
        }
    }
    
    // Find local maxima
    int count = 0;
    for (int y = radius + 1; y < h - radius - 1 && count < max_corners; y++) {
        for (int x = radius + 1; x < w - radius - 1 && count < max_corners; x++) {
            float32_t val = response[y * w + x];
            
            if (val > threshold) {
                // Check if local maximum
                bool is_max = true;
                for (int ky = -1; ky <= 1 && is_max; ky++) {
                    for (int kx = -1; kx <= 1 && is_max; kx++) {
                        if (kx == 0 && ky == 0) continue;
                        if (response[(y + ky) * w + (x + kx)] >= val) {
                            is_max = false;
                        }
                    }
                }
                
                if (is_max) {
                    corners[count].x = (float32_t)x;
                    corners[count].y = (float32_t)y;
                    corners[count].response = val;
                    corners[count].size = (float32_t)block_size;
                    corners[count].angle = 0;
                    corners[count].octave = 0;
                    corners[count].class_id = -1;
                    count++;
                }
            }
        }
    }
    
    return count;
}

// ============================================================================
// Canny Edge Detection
// ============================================================================

eif_status_t eif_cv_canny(const eif_cv_image_t* src, eif_cv_image_t* dst,
                           uint8_t low_thresh, uint8_t high_thresh,
                           eif_memory_pool_t* pool) {
    if (!src || !dst || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    int w = src->width, h = src->height;
    
    // Compute gradients
    eif_cv_image_t gx, gy, blurred;
    eif_cv_image_create(&gx, w, h, EIF_CV_GRAY8, pool);
    eif_cv_image_create(&gy, w, h, EIF_CV_GRAY8, pool);
    eif_cv_image_create(&blurred, w, h, EIF_CV_GRAY8, pool);
    
    // Gaussian blur first
    eif_cv_blur_gaussian(src, &blurred, 5, 1.4f, pool);
    
    // Sobel gradients
    eif_cv_sobel(&blurred, &gx, 1, 0, 3);
    eif_cv_sobel(&blurred, &gy, 0, 1, 3);
    
    // Compute magnitude and angle
    float32_t* magnitude = eif_memory_alloc(pool, w * h * sizeof(float32_t), 4);
    uint8_t* direction = eif_memory_alloc(pool, w * h, 4);
    
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float32_t dx = (float32_t)(gx.data[y * gx.stride + x] - 128);
            float32_t dy = (float32_t)(gy.data[y * gy.stride + x] - 128);
            magnitude[y * w + x] = sqrtf(dx * dx + dy * dy);
            
            // Quantize direction to 0, 45, 90, 135 degrees
            float32_t angle = atan2f(dy, dx) * 180.0f / M_PI;
            if (angle < 0) angle += 180;
            
            if (angle < 22.5f || angle >= 157.5f) direction[y * w + x] = 0;       // Horizontal
            else if (angle < 67.5f) direction[y * w + x] = 1;                      // 45 degrees
            else if (angle < 112.5f) direction[y * w + x] = 2;                     // Vertical
            else direction[y * w + x] = 3;                                         // 135 degrees
        }
    }
    
    // Non-maximum suppression
    float32_t* nms = eif_memory_alloc(pool, w * h * sizeof(float32_t), 4);
    memset(nms, 0, w * h * sizeof(float32_t));
    
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            float32_t mag = magnitude[y * w + x];
            float32_t n1, n2;
            
            switch (direction[y * w + x]) {
                case 0:  // Horizontal edge
                    n1 = magnitude[y * w + x - 1];
                    n2 = magnitude[y * w + x + 1];
                    break;
                case 1:  // 45 degrees
                    n1 = magnitude[(y - 1) * w + x + 1];
                    n2 = magnitude[(y + 1) * w + x - 1];
                    break;
                case 2:  // Vertical edge
                    n1 = magnitude[(y - 1) * w + x];
                    n2 = magnitude[(y + 1) * w + x];
                    break;
                default: // 135 degrees
                    n1 = magnitude[(y - 1) * w + x - 1];
                    n2 = magnitude[(y + 1) * w + x + 1];
                    break;
            }
            
            if (mag >= n1 && mag >= n2) {
                nms[y * w + x] = mag;
            }
        }
    }
    
    // Double threshold and edge tracking by hysteresis
    memset(dst->data, 0, dst->stride * dst->height);
    
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            float32_t mag = nms[y * w + x];
            
            if (mag >= high_thresh) {
                dst->data[y * dst->stride + x] = 255;  // Strong edge
            } else if (mag >= low_thresh) {
                // Check if connected to strong edge
                bool connected = false;
                for (int ky = -1; ky <= 1 && !connected; ky++) {
                    for (int kx = -1; kx <= 1 && !connected; kx++) {
                        if (nms[(y + ky) * w + (x + kx)] >= high_thresh) {
                            connected = true;
                        }
                    }
                }
                if (connected) {
                    dst->data[y * dst->stride + x] = 255;
                }
            }
        }
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Descriptor Matching
// ============================================================================

int eif_cv_hamming_distance(const eif_cv_descriptor_t* a, const eif_cv_descriptor_t* b) {
    int distance = 0;
    for (int i = 0; i < 32; i++) {
        uint8_t xor_val = a->data[i] ^ b->data[i];
        // Count bits
        while (xor_val) {
            distance += xor_val & 1;
            xor_val >>= 1;
        }
    }
    return distance;
}

int eif_cv_match_bf(const eif_cv_descriptor_t* desc1, int n1,
                     const eif_cv_descriptor_t* desc2, int n2,
                     eif_cv_match_t* matches, float32_t ratio_thresh) {
    if (!desc1 || !desc2 || !matches || n1 <= 0 || n2 <= 0) return 0;
    
    int good_matches = 0;
    
    for (int i = 0; i < n1; i++) {
        int best_dist = 256;
        int second_dist = 256;
        int best_idx = -1;
        
        for (int j = 0; j < n2; j++) {
            int dist = eif_cv_hamming_distance(&desc1[i], &desc2[j]);
            
            if (dist < best_dist) {
                second_dist = best_dist;
                best_dist = dist;
                best_idx = j;
            } else if (dist < second_dist) {
                second_dist = dist;
            }
        }
        
        // Lowe's ratio test
        if (best_idx >= 0 && (float32_t)best_dist < ratio_thresh * second_dist) {
            matches[good_matches].query_idx = i;
            matches[good_matches].train_idx = best_idx;
            matches[good_matches].distance = (float32_t)best_dist;
            good_matches++;
        }
    }
    
    return good_matches;
}

// ============================================================================
// HOG Descriptor
// ============================================================================

int eif_cv_hog_descriptor_size(const eif_cv_hog_config_t* config) {
    if (!config) return 0;
    
    int blocks_x = (config->win_width - config->block_size) / config->block_stride + 1;
    int blocks_y = (config->win_height - config->block_size) / config->block_stride + 1;
    int cells_per_block = (config->block_size / config->cell_size) * 
                          (config->block_size / config->cell_size);
    
    return blocks_x * blocks_y * cells_per_block * config->num_bins;
}

eif_status_t eif_cv_compute_hog(const eif_cv_image_t* img,
                                 const eif_cv_rect_t* roi,
                                 const eif_cv_hog_config_t* config,
                                 float32_t* descriptor,
                                 eif_memory_pool_t* pool) {
    if (!img || !config || !descriptor || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    int roi_x = roi ? roi->x : 0;
    int roi_y = roi ? roi->y : 0;
    int roi_w = roi ? roi->width : img->width;
    int roi_h = roi ? roi->height : img->height;
    
    // Compute gradients
    int desc_size = eif_cv_hog_descriptor_size(config);
    memset(descriptor, 0, desc_size * sizeof(float32_t));
    
    int cells_x = roi_w / config->cell_size;
    int cells_y = roi_h / config->cell_size;
    
    // Allocate cell histograms
    float32_t* cell_hists = eif_memory_alloc(pool, 
        cells_x * cells_y * config->num_bins * sizeof(float32_t), 4);
    if (!cell_hists) return EIF_STATUS_OUT_OF_MEMORY;
    memset(cell_hists, 0, cells_x * cells_y * config->num_bins * sizeof(float32_t));
    
    // Compute cell histograms
    for (int cy = 0; cy < cells_y; cy++) {
        for (int cx = 0; cx < cells_x; cx++) {
            float32_t* hist = &cell_hists[(cy * cells_x + cx) * config->num_bins];
            
            for (int py = 0; py < config->cell_size; py++) {
                for (int px = 0; px < config->cell_size; px++) {
                    int x = roi_x + cx * config->cell_size + px;
                    int y = roi_y + cy * config->cell_size + py;
                    
                    if (x <= 0 || x >= img->width - 1 || y <= 0 || y >= img->height - 1) continue;
                    
                    // Compute gradient
                    int gx = (int)img->data[y * img->stride + x + 1] - img->data[y * img->stride + x - 1];
                    int gy = (int)img->data[(y + 1) * img->stride + x] - img->data[(y - 1) * img->stride + x];
                    
                    float32_t magnitude = sqrtf((float32_t)(gx * gx + gy * gy));
                    float32_t angle = atan2f((float32_t)gy, (float32_t)gx) * 180.0f / M_PI;
                    if (angle < 0) angle += 180;
                    
                    // Bin interpolation
                    float32_t bin_width = 180.0f / config->num_bins;
                    int bin = (int)(angle / bin_width) % config->num_bins;
                    hist[bin] += magnitude;
                }
            }
        }
    }
    
    // Block normalization
    int cells_per_block = config->block_size / config->cell_size;
    int blocks_x = (cells_x - cells_per_block) / (config->block_stride / config->cell_size) + 1;
    int blocks_y = (cells_y - cells_per_block) / (config->block_stride / config->cell_size) + 1;
    int block_stride_cells = config->block_stride / config->cell_size;
    
    int desc_idx = 0;
    for (int by = 0; by < blocks_y; by++) {
        for (int bx = 0; bx < blocks_x; bx++) {
            // Collect cells in block
            float32_t block_norm = 0;
            
            for (int cy = 0; cy < cells_per_block; cy++) {
                for (int cx = 0; cx < cells_per_block; cx++) {
                    int cell_idx = (by * block_stride_cells + cy) * cells_x + 
                                   (bx * block_stride_cells + cx);
                    
                    for (int b = 0; b < config->num_bins; b++) {
                        float32_t val = cell_hists[cell_idx * config->num_bins + b];
                        block_norm += val * val;
                    }
                }
            }
            
            block_norm = sqrtf(block_norm + 1e-6f);
            
            // L2 normalize and copy to descriptor
            for (int cy = 0; cy < cells_per_block; cy++) {
                for (int cx = 0; cx < cells_per_block; cx++) {
                    int cell_idx = (by * block_stride_cells + cy) * cells_x + 
                                   (bx * block_stride_cells + cx);
                    
                    for (int b = 0; b < config->num_bins; b++) {
                        descriptor[desc_idx++] = cell_hists[cell_idx * config->num_bins + b] / block_norm;
                    }
                }
            }
        }
    }
    
    return EIF_STATUS_OK;
}
