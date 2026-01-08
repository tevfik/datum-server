#include "eif_cv_haar.h"
#include "eif_hal_simd.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

void eif_cv_compute_integral(const eif_cv_image_t* src, eif_integral_image_t* dst) {
    if (!src || !dst) return;
    
    // Allocate if needed
    if (!dst->data) {
        dst->data = (uint32_t*)malloc(src->width * src->height * sizeof(uint32_t));
        dst->width = src->width;
        dst->height = src->height;
    }
    
    const uint8_t* p_src = src->data;
    uint32_t* p_dst = dst->data;
    int w = src->width;
    int h = src->height;
    int stride = src->stride; // Use stride!
    
    // Optimized Integral Image Calculation: 
    // Split into Horizontal Prefix Sum + Vertical Vector Add
    
    for (int y = 0; y < h; y++) {
        // 1. Horizontal Prefix Sum
        uint32_t row_sum = 0;
        const uint8_t* row_src = &p_src[y * stride];
        uint32_t* row_dst = &p_dst[y * w];
        
        // Simple unrolling for row sum could go here, but compiler usually does okay.
        // Keeping it simple for clarity or manually unroll if truly needed.
        for (int x = 0; x < w; x++) {
            row_sum += row_src[x];
            row_dst[x] = row_sum;
        }

        // 2. Vertical Accumulation (Vectorized)
        // current_row += prev_row
        if (y > 0) {
            uint32_t* prev_row_dst = &p_dst[(y - 1) * w];
            eif_simd_add_u32(row_dst, prev_row_dst, row_dst, w);
        }
    }
}

// Check one window at scale 1.0
static bool eval_window(const eif_integral_image_t* ii, 
                        const eif_haar_cascade_t* cascade, 
                        int off_x, int off_y) {
    
    for (int s = 0; s < cascade->stage_count; s++) {
        eif_haar_stage_t* stage = &cascade->stages[s];
        float stage_sum = 0.0f;
        
        for (int w = 0; w < stage->weak_count; w++) {
            eif_weak_classifier_t* weak = &stage->weaks[w];
            float feature_sum = 0.0f;
            
            for (int r = 0; r < weak->rect_count; r++) {
                eif_haar_rect_t* rect = &weak->rects[r];
                // Scale rect coords? Standard Haar uses relative coords to window (0,0)
                int rx = off_x + rect->x;
                int ry = off_y + rect->y;
                // Clip check usually needed
                uint32_t region_val = eif_cv_integral_sum(ii, rx, ry, rect->w, rect->h);
                feature_sum += (float)region_val * rect->weight;
            }
            
            if (feature_sum < weak->threshold) {
                stage_sum += weak->left_val;
            } else {
                stage_sum += weak->right_val;
            }
        }
        
        if (stage_sum < stage->stage_threshold) {
            return false; // Rejected by this stage
        }
    }
    return true; // Passed all stages
}

int eif_cv_haar_detect(const eif_integral_image_t* ii, 
                       const eif_haar_cascade_t* cascade,
                       eif_cv_rect_t* objects, 
                       int max_objects) {
    if (!ii || !cascade || !objects) return 0;
    
    int count = 0;
    int win_w = cascade->window_w;
    int win_h = cascade->window_h;
    
    int loop_h = ii->height - win_h;
    int loop_w = ii->width - win_w;
    
    // Slidng window (Step 2 for speed)
    for (int y = 0; y < loop_h; y += 2) {
        for (int x = 0; x < loop_w; x += 2) {
            if (eval_window(ii, cascade, x, y)) {
                if (count < max_objects) {
                    objects[count].x = x;
                    objects[count].y = y;
                    objects[count].width = win_w;
                    objects[count].height = win_h;
                    count++;
                } else {
                    return count;
                }
            }
        }
    }
    return count;
}

// Creates a dummy cascade that detects high contrast horizontal transitions (like eyes)
eif_status_t eif_cv_haar_load_fake_face(eif_haar_cascade_t* cascade) {
    cascade->window_w = 24;
    cascade->window_h = 24;
    cascade->stage_count = 1;
    cascade->stages = (eif_haar_stage_t*)malloc(sizeof(eif_haar_stage_t));
    
    // Single stage, single feature (Haar Type: 2-rect vertical or horizontal)
    // Let's make a simple "eye-like" feature: Dark horizontal band over light band
    eif_haar_stage_t* stg = &cascade->stages[0];
    stg->weak_count = 1;
    stg->stage_threshold = 0.5f; // Needs to pass
    stg->weaks = (eif_weak_classifier_t*)malloc(sizeof(eif_weak_classifier_t));
    
    // A horizontal edge: Black top, White bottom
    // Rect 1 (Top, Black): weight +1
    // Rect 2 (Bottom, White): weight -1
    // If Sum(Rect1) < Sum(Rect2), result is negative? 
    // Usually Haar is: threshold check.
    
    eif_weak_classifier_t* wk = &stg->weaks[0];
    wk->rect_count = 2;
    wk->threshold = -500.0f; // Arbitrary for demo
    wk->left_val = 1.0f;     // Pass
    wk->right_val = 0.0f;    // Fail
    
    // Top half (Darker ideally)
    wk->rects[0].x = 0; wk->rects[0].y = 0;
    wk->rects[0].w = 24; wk->rects[0].h = 12;
    wk->rects[0].weight = -1.0f; 
    
    // Bottom half (Lighter ideally)
    wk->rects[1].x = 0; wk->rects[1].y = 12;
    wk->rects[1].w = 24; wk->rects[1].h = 12;
    wk->rects[1].weight = 1.0f;
    
    // If Top is dark (Small sum) and Bottom is light (Large sum):
    // Sum = -1*Small + 1*Large = Large Positive.
    // Wait, threshold check usually: val < thresh -> left, else right.
    // If we want "Pass", we need val < thresh (if left is pass).
    // So we want Large Negative result?
    // Let's swap weights: Top=1 (Small), Bottom=-1 (Large). Result = Small - Large = Large Negative.
    
    wk->rects[0].weight = 1.0f;
    wk->rects[1].weight = -1.0f;
    
    return EIF_STATUS_OK;
}
