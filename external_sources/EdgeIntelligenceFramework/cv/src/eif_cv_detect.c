/**
 * @file eif_cv_detect.c
 * @brief Object Detection
 */

#include "eif_cv_detect.h"
#include <string.h>
#include <math.h>

// ============================================================================
// Template Matching
// ============================================================================

eif_status_t eif_cv_match_template(const eif_cv_image_t* src,
                                    const eif_cv_image_t* templ,
                                    float32_t* result,
                                    eif_cv_tm_method_t method,
                                    eif_memory_pool_t* pool) {
    if (!src || !templ || !result) return EIF_STATUS_INVALID_ARGUMENT;
    if (templ->width > src->width || templ->height > src->height) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    int result_w = src->width - templ->width + 1;
    int result_h = src->height - templ->height + 1;
    
    // Template statistics
    float32_t templ_sum = 0, templ_sq_sum = 0;
    for (int ty = 0; ty < templ->height; ty++) {
        for (int tx = 0; tx < templ->width; tx++) {
            float32_t val = templ->data[ty * templ->stride + tx];
            templ_sum += val;
            templ_sq_sum += val * val;
        }
    }
    int templ_size = templ->width * templ->height;
    float32_t templ_mean = templ_sum / templ_size;
    
    // Scan source image
    for (int y = 0; y < result_h; y++) {
        for (int x = 0; x < result_w; x++) {
            float32_t sum = 0, sq_sum = 0, cross_sum = 0;
            
            for (int ty = 0; ty < templ->height; ty++) {
                for (int tx = 0; tx < templ->width; tx++) {
                    float32_t src_val = src->data[(y + ty) * src->stride + (x + tx)];
                    float32_t tpl_val = templ->data[ty * templ->stride + tx];
                    
                    sum += src_val;
                    sq_sum += src_val * src_val;
                    cross_sum += src_val * tpl_val;
                }
            }
            
            float32_t src_mean = sum / templ_size;
            float32_t match;
            
            switch (method) {
                case EIF_CV_TM_SQDIFF: {
                    match = sq_sum - 2 * cross_sum + templ_sq_sum;
                    break;
                }
                case EIF_CV_TM_SQDIFF_NORMED: {
                    float32_t denom = sqrtf(sq_sum * templ_sq_sum);
                    match = denom > 0 ? (sq_sum - 2 * cross_sum + templ_sq_sum) / denom : 1;
                    break;
                }
                case EIF_CV_TM_CCORR:
                    match = cross_sum;
                    break;
                    
                case EIF_CV_TM_CCORR_NORMED: {
                    float32_t denom = sqrtf(sq_sum * templ_sq_sum);
                    match = denom > 0 ? cross_sum / denom : 0;
                    break;
                }
                case EIF_CV_TM_CCOEFF: {
                    float32_t ccorr = 0;
                    for (int ty = 0; ty < templ->height; ty++) {
                        for (int tx = 0; tx < templ->width; tx++) {
                            float32_t src_val = src->data[(y + ty) * src->stride + (x + tx)];
                            float32_t tpl_val = templ->data[ty * templ->stride + tx];
                            ccorr += (src_val - src_mean) * (tpl_val - templ_mean);
                        }
                    }
                    match = ccorr;
                    break;
                }
                case EIF_CV_TM_CCOEFF_NORMED:
                default: {
                    float32_t ccorr = 0, src_var = 0, tpl_var = 0;
                    for (int ty = 0; ty < templ->height; ty++) {
                        for (int tx = 0; tx < templ->width; tx++) {
                            float32_t src_val = src->data[(y + ty) * src->stride + (x + tx)];
                            float32_t tpl_val = templ->data[ty * templ->stride + tx];
                            float32_t src_diff = src_val - src_mean;
                            float32_t tpl_diff = tpl_val - templ_mean;
                            ccorr += src_diff * tpl_diff;
                            src_var += src_diff * src_diff;
                            tpl_var += tpl_diff * tpl_diff;
                        }
                    }
                    float32_t denom = sqrtf(src_var * tpl_var);
                    match = denom > 0 ? ccorr / denom : 0;
                    break;
                }
            }
            
            result[y * result_w + x] = match;
        }
    }
    
    return EIF_STATUS_OK;
}

eif_cv_tm_result_t eif_cv_template_minmax(const float32_t* result,
                                           int result_width, int result_height,
                                           eif_cv_tm_method_t method) {
    eif_cv_tm_result_t res = {0, 0, 0};
    if (!result) return res;
    
    // For SQDIFF methods, we want minimum; otherwise maximum
    bool find_min = (method == EIF_CV_TM_SQDIFF || method == EIF_CV_TM_SQDIFF_NORMED);
    
    float32_t best = find_min ? 1e30f : -1e30f;
    
    for (int y = 0; y < result_height; y++) {
        for (int x = 0; x < result_width; x++) {
            float32_t val = result[y * result_width + x];
            
            if ((find_min && val < best) || (!find_min && val > best)) {
                best = val;
                res.x = x;
                res.y = y;
                res.score = val;
            }
        }
    }
    
    return res;
}

// ============================================================================
// Integral Image
// ============================================================================

eif_status_t eif_cv_integral(const eif_cv_image_t* src,
                              int32_t* sum, int64_t* sqsum) {
    if (!src || !sum) return EIF_STATUS_INVALID_ARGUMENT;
    
    int w = src->width;
    int h = src->height;
    int sw = w + 1;  // Integral image is (w+1) x (h+1)
    
    // Initialize first row/column to zero
    for (int x = 0; x <= w; x++) sum[x] = 0;
    for (int y = 0; y <= h; y++) sum[y * sw] = 0;
    
    if (sqsum) {
        for (int x = 0; x <= w; x++) sqsum[x] = 0;
        for (int y = 0; y <= h; y++) sqsum[y * sw] = 0;
    }
    
    // Compute integral image
    for (int y = 0; y < h; y++) {
        int32_t row_sum = 0;
        int64_t row_sqsum = 0;
        
        for (int x = 0; x < w; x++) {
            uint8_t val = src->data[y * src->stride + x];
            row_sum += val;
            sum[(y + 1) * sw + (x + 1)] = row_sum + sum[y * sw + (x + 1)];
            
            if (sqsum) {
                row_sqsum += (int64_t)val * val;
                sqsum[(y + 1) * sw + (x + 1)] = row_sqsum + sqsum[y * sw + (x + 1)];
            }
        }
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// IoU and NMS
// ============================================================================

float32_t eif_cv_iou(const eif_cv_rect_t* a, const eif_cv_rect_t* b) {
    if (!a || !b) return 0;
    
    int x1 = a->x > b->x ? a->x : b->x;
    int y1 = a->y > b->y ? a->y : b->y;
    int x2 = (a->x + a->width) < (b->x + b->width) ? (a->x + a->width) : (b->x + b->width);
    int y2 = (a->y + a->height) < (b->y + b->height) ? (a->y + a->height) : (b->y + b->height);
    
    if (x2 <= x1 || y2 <= y1) return 0;
    
    int intersection = (x2 - x1) * (y2 - y1);
    int union_area = a->width * a->height + b->width * b->height - intersection;
    
    return union_area > 0 ? (float32_t)intersection / union_area : 0;
}

int eif_cv_nms(eif_cv_detection_t* detections, int num_detections,
                float32_t iou_threshold) {
    if (!detections || num_detections <= 0) return 0;
    
    // Sort by confidence (simple bubble sort for small arrays)
    for (int i = 0; i < num_detections - 1; i++) {
        for (int j = i + 1; j < num_detections; j++) {
            if (detections[j].confidence > detections[i].confidence) {
                eif_cv_detection_t tmp = detections[i];
                detections[i] = detections[j];
                detections[j] = tmp;
            }
        }
    }
    
    // Mark suppressed detections
    for (int i = 0; i < num_detections; i++) {
        if (detections[i].confidence < 0) continue;  // Already suppressed
        
        for (int j = i + 1; j < num_detections; j++) {
            if (detections[j].confidence < 0) continue;
            
            float32_t iou = eif_cv_iou(&detections[i].rect, &detections[j].rect);
            if (iou > iou_threshold) {
                detections[j].confidence = -1;  // Suppress
            }
        }
    }
    
    // Compact array
    int count = 0;
    for (int i = 0; i < num_detections; i++) {
        if (detections[i].confidence >= 0) {
            if (count != i) {
                detections[count] = detections[i];
            }
            count++;
        }
    }
    
    return count;
}

// ============================================================================
// HOG Detector
// ============================================================================

eif_status_t eif_cv_hog_detector_init(eif_cv_hog_detector_t* detector,
                                       const eif_cv_hog_config_t* config,
                                       const float32_t* weights, float32_t bias) {
    if (!detector || !config || !weights) return EIF_STATUS_INVALID_ARGUMENT;
    
    detector->hog_config = *config;
    detector->svm_weights = (float32_t*)weights;
    detector->svm_bias = bias;
    detector->descriptor_size = eif_cv_hog_descriptor_size(config);
    
    return EIF_STATUS_OK;
}

int eif_cv_hog_detect(const eif_cv_image_t* img,
                       const eif_cv_hog_detector_t* detector,
                       float32_t hit_threshold, int win_stride,
                       float32_t scale_factor,
                       eif_cv_detection_t* detections, int max_detections,
                       eif_memory_pool_t* pool) {
    if (!img || !detector || !detections || !pool) return 0;
    
    int count = 0;
    float32_t* descriptor = eif_memory_alloc(pool, 
        detector->descriptor_size * sizeof(float32_t), 4);
    if (!descriptor) return 0;
    
    // Multi-scale detection
    for (float scale = 1.0f; scale > 0.1f; scale /= scale_factor) {
        int scaled_w = (int)(img->width * scale);
        int scaled_h = (int)(img->height * scale);
        
        if (scaled_w < detector->hog_config.win_width ||
            scaled_h < detector->hog_config.win_height) {
            break;
        }
        
        // Create scaled image
        eif_cv_image_t scaled;
        eif_cv_image_create(&scaled, scaled_w, scaled_h, img->format, pool);
        eif_cv_resize(img, &scaled, scaled_w, scaled_h, EIF_CV_INTER_BILINEAR, pool);
        
        // Slide window
        for (int y = 0; y <= scaled_h - detector->hog_config.win_height; y += win_stride) {
            for (int x = 0; x <= scaled_w - detector->hog_config.win_width; x += win_stride) {
                eif_cv_rect_t roi = {x, y, detector->hog_config.win_width, 
                                          detector->hog_config.win_height};
                
                // Compute HOG
                eif_cv_compute_hog(&scaled, &roi, &detector->hog_config, descriptor, pool);
                
                // SVM classification
                float32_t score = detector->svm_bias;
                for (int i = 0; i < detector->descriptor_size; i++) {
                    score += descriptor[i] * detector->svm_weights[i];
                }
                
                if (score > hit_threshold && count < max_detections) {
                    detections[count].rect.x = (int)(x / scale);
                    detections[count].rect.y = (int)(y / scale);
                    detections[count].rect.width = (int)(detector->hog_config.win_width / scale);
                    detections[count].rect.height = (int)(detector->hog_config.win_height / scale);
                    detections[count].confidence = score;
                    detections[count].neighbors = 0;
                    count++;
                }
            }
        }
    }
    
    // Apply NMS
    if (count > 1) {
        count = eif_cv_nms(detections, count, 0.4f);
    }
    
    return count;
}
