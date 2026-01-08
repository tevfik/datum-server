/**
 * @file eif_cv_track.c
 * @brief Object Tracking
 */

#include "eif_cv_track.h"
#include "eif_cv_detect.h"
#include <string.h>
#include <math.h>

// ============================================================================
// Lucas-Kanade Optical Flow
// ============================================================================

eif_status_t eif_cv_optical_flow_lk(const eif_cv_image_t* prev,
                                     const eif_cv_image_t* curr,
                                     const eif_cv_point2f_t* prev_pts, int num_pts,
                                     eif_cv_point2f_t* next_pts,
                                     uint8_t* status,
                                     int win_size, int max_iter,
                                     eif_memory_pool_t* pool) {
    if (!prev || !curr || !prev_pts || !next_pts || !status) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    int radius = win_size / 2;
    
    for (int p = 0; p < num_pts; p++) {
        float32_t px = prev_pts[p].x;
        float32_t py = prev_pts[p].y;
        
        // Initialize next point
        next_pts[p].x = px;
        next_pts[p].y = py;
        status[p] = 0;
        
        // Check bounds
        if (px < radius || px >= prev->width - radius ||
            py < radius || py >= prev->height - radius) {
            continue;
        }
        
        // Compute spatial gradients and structure tensor
        float32_t Ixx = 0, Iyy = 0, Ixy = 0;
        float32_t* Ix_win = eif_memory_alloc(pool, win_size * win_size * sizeof(float32_t), 4);
        float32_t* Iy_win = eif_memory_alloc(pool, win_size * win_size * sizeof(float32_t), 4);
        
        if (!Ix_win || !Iy_win) continue;
        
        for (int wy = -radius; wy <= radius; wy++) {
            for (int wx = -radius; wx <= radius; wx++) {
                int x = (int)px + wx;
                int y = (int)py + wy;
                
                // Central differences
                float32_t gx = ((int)prev->data[y * prev->stride + x + 1] - 
                               prev->data[y * prev->stride + x - 1]) / 2.0f;
                float32_t gy = ((int)prev->data[(y + 1) * prev->stride + x] - 
                               prev->data[(y - 1) * prev->stride + x]) / 2.0f;
                
                int widx = (wy + radius) * win_size + (wx + radius);
                Ix_win[widx] = gx;
                Iy_win[widx] = gy;
                
                Ixx += gx * gx;
                Iyy += gy * gy;
                Ixy += gx * gy;
            }
        }
        
        // Check if point is trackable (sufficient texture)
        float32_t det = Ixx * Iyy - Ixy * Ixy;
        if (det < 1e-6f) continue;
        
        float32_t inv_det = 1.0f / det;
        
        // Iterative refinement
        float32_t dx = 0, dy = 0;
        
        for (int iter = 0; iter < max_iter; iter++) {
            float32_t Itx = 0, Ity = 0;
            
            for (int wy = -radius; wy <= radius; wy++) {
                for (int wx = -radius; wx <= radius; wx++) {
                    int px1 = (int)px + wx;
                    int py1 = (int)py + wy;
                    
                    // Bilinear interpolation for current frame
                    float32_t cx = px + dx + wx;
                    float32_t cy = py + dy + wy;
                    
                    if (cx < 0 || cx >= curr->width - 1 || cy < 0 || cy >= curr->height - 1) {
                        continue;
                    }
                    
                    int x0 = (int)cx, y0 = (int)cy;
                    float32_t fx = cx - x0, fy = cy - y0;
                    
                    float32_t c00 = curr->data[y0 * curr->stride + x0];
                    float32_t c01 = curr->data[y0 * curr->stride + x0 + 1];
                    float32_t c10 = curr->data[(y0 + 1) * curr->stride + x0];
                    float32_t c11 = curr->data[(y0 + 1) * curr->stride + x0 + 1];
                    
                    float32_t curr_val = c00 * (1-fx) * (1-fy) + c01 * fx * (1-fy) +
                                         c10 * (1-fx) * fy + c11 * fx * fy;
                    
                    float32_t prev_val = prev->data[py1 * prev->stride + px1];
                    float32_t It = curr_val - prev_val;
                    
                    int widx = (wy + radius) * win_size + (wx + radius);
                    Itx += Ix_win[widx] * It;
                    Ity += Iy_win[widx] * It;
                }
            }
            
            // Solve 2x2 system
            float32_t ddx = inv_det * (Iyy * Itx - Ixy * Ity);
            float32_t ddy = inv_det * (Ixx * Ity - Ixy * Itx);
            
            dx -= ddx;
            dy -= ddy;
            
            // Convergence check
            if (ddx * ddx + ddy * ddy < 0.01f) break;
        }
        
        next_pts[p].x = px + dx;
        next_pts[p].y = py + dy;
        
        // Check if point is still in bounds
        if (next_pts[p].x >= 0 && next_pts[p].x < curr->width &&
            next_pts[p].y >= 0 && next_pts[p].y < curr->height) {
            status[p] = 1;
        }
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Multi-Object Tracker
// ============================================================================

eif_status_t eif_cv_tracker_init(eif_cv_tracker_t* tracker, int max_tracks,
                                  int max_age, int min_hits, float32_t iou_thresh,
                                  eif_memory_pool_t* pool) {
    if (!tracker || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    tracker->tracks = eif_memory_alloc(pool, max_tracks * sizeof(eif_cv_track_t), 4);
    if (!tracker->tracks) return EIF_STATUS_OUT_OF_MEMORY;
    
    tracker->num_tracks = 0;
    tracker->max_tracks = max_tracks;
    tracker->next_id = 1;
    tracker->max_age = max_age;
    tracker->min_hits = min_hits;
    tracker->iou_threshold = iou_thresh;
    
    return EIF_STATUS_OK;
}

int eif_cv_tracker_update(eif_cv_tracker_t* tracker,
                           const eif_cv_rect_t* detections, int num_detections) {
    if (!tracker) return 0;
    
    // Predict existing tracks (simple constant velocity)
    for (int i = 0; i < tracker->num_tracks; i++) {
        tracker->tracks[i].bbox.x += (int)tracker->tracks[i].velocity.x;
        tracker->tracks[i].bbox.y += (int)tracker->tracks[i].velocity.y;
        tracker->tracks[i].center.x += tracker->tracks[i].velocity.x;
        tracker->tracks[i].center.y += tracker->tracks[i].velocity.y;
        tracker->tracks[i].age++;
    }
    
    // Match detections to tracks using IoU
    int* det_matched = (int*)__builtin_alloca(num_detections * sizeof(int));
    int* track_matched = (int*)__builtin_alloca(tracker->num_tracks * sizeof(int));
    memset(det_matched, 0, num_detections * sizeof(int));
    memset(track_matched, 0, tracker->num_tracks * sizeof(int));
    
    // Greedy matching
    for (int d = 0; d < num_detections; d++) {
        float32_t best_iou = tracker->iou_threshold;
        int best_track = -1;
        
        for (int t = 0; t < tracker->num_tracks; t++) {
            if (track_matched[t]) continue;
            
            float32_t iou = eif_cv_iou(&detections[d], &tracker->tracks[t].bbox);
            if (iou > best_iou) {
                best_iou = iou;
                best_track = t;
            }
        }
        
        if (best_track >= 0) {
            // Update matched track
            eif_cv_track_t* track = &tracker->tracks[best_track];
            
            // Update velocity
            track->velocity.x = 0.5f * track->velocity.x + 
                0.5f * (detections[d].x + detections[d].width/2.0f - track->center.x);
            track->velocity.y = 0.5f * track->velocity.y + 
                0.5f * (detections[d].y + detections[d].height/2.0f - track->center.y);
            
            track->bbox = detections[d];
            track->center.x = detections[d].x + detections[d].width / 2.0f;
            track->center.y = detections[d].y + detections[d].height / 2.0f;
            track->hits++;
            track->misses = 0;
            
            if (track->hits >= tracker->min_hits) {
                track->state = 1;  // Confirmed
            }
            
            det_matched[d] = 1;
            track_matched[best_track] = 1;
        }
    }
    
    // Handle unmatched tracks
    for (int t = 0; t < tracker->num_tracks; t++) {
        if (!track_matched[t]) {
            tracker->tracks[t].misses++;
            if (tracker->tracks[t].misses > tracker->max_age) {
                tracker->tracks[t].state = 2;  // Deleted
            }
        }
    }
    
    // Create new tracks for unmatched detections
    for (int d = 0; d < num_detections; d++) {
        if (!det_matched[d] && tracker->num_tracks < tracker->max_tracks) {
            eif_cv_track_t* track = &tracker->tracks[tracker->num_tracks];
            
            track->id = tracker->next_id++;
            track->bbox = detections[d];
            track->center.x = detections[d].x + detections[d].width / 2.0f;
            track->center.y = detections[d].y + detections[d].height / 2.0f;
            track->velocity.x = 0;
            track->velocity.y = 0;
            track->age = 0;
            track->hits = 1;
            track->misses = 0;
            track->state = 0;  // Tentative
            
            tracker->num_tracks++;
        }
    }
    
    // Remove deleted tracks
    int write_idx = 0;
    for (int t = 0; t < tracker->num_tracks; t++) {
        if (tracker->tracks[t].state != 2) {
            if (write_idx != t) {
                tracker->tracks[write_idx] = tracker->tracks[t];
            }
            write_idx++;
        }
    }
    tracker->num_tracks = write_idx;
    
    return tracker->num_tracks;
}

int eif_cv_tracker_get_tracks(const eif_cv_tracker_t* tracker,
                               eif_cv_track_t* tracks, int max_tracks) {
    if (!tracker || !tracks) return 0;
    
    int count = 0;
    for (int t = 0; t < tracker->num_tracks && count < max_tracks; t++) {
        if (tracker->tracks[t].state == 1) {  // Confirmed only
            tracks[count++] = tracker->tracks[t];
        }
    }
    
    return count;
}

// ============================================================================
// Background Subtraction
// ============================================================================

eif_status_t eif_cv_bg_init(eif_cv_bg_model_t* model, int width, int height,
                             float32_t alpha, eif_memory_pool_t* pool) {
    if (!model || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    model->width = width;
    model->height = height;
    model->alpha = alpha;
    
    int size = width * height;
    model->background = eif_memory_alloc(pool, size * sizeof(float32_t), 4);
    model->variance = eif_memory_alloc(pool, size * sizeof(float32_t), 4);
    
    if (!model->background || !model->variance) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    // Initialize with default values
    for (int i = 0; i < size; i++) {
        model->background[i] = 128.0f;
        model->variance[i] = 100.0f;
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_cv_bg_update(eif_cv_bg_model_t* model,
                               const eif_cv_image_t* frame,
                               eif_cv_image_t* fg_mask,
                               float32_t threshold) {
    if (!model || !frame || !fg_mask) return EIF_STATUS_INVALID_ARGUMENT;
    
    for (int y = 0; y < model->height; y++) {
        for (int x = 0; x < model->width; x++) {
            int idx = y * model->width + x;
            float32_t pixel = frame->data[y * frame->stride + x];
            
            // Compute difference from background
            float32_t diff = pixel - model->background[idx];
            float32_t std_dev = sqrtf(model->variance[idx]);
            
            // Foreground if difference > threshold * std_dev
            if (fabsf(diff) > threshold * std_dev) {
                fg_mask->data[y * fg_mask->stride + x] = 255;
            } else {
                fg_mask->data[y * fg_mask->stride + x] = 0;
                
                // Update background model (only for background pixels)
                model->background[idx] = (1 - model->alpha) * model->background[idx] + 
                                          model->alpha * pixel;
                model->variance[idx] = (1 - model->alpha) * model->variance[idx] + 
                                        model->alpha * diff * diff;
            }
        }
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Motion History
// ============================================================================

eif_status_t eif_cv_update_motion_history(const eif_cv_image_t* silhouette,
                                           float32_t* mhi, int width, int height,
                                           float32_t timestamp, float32_t duration) {
    if (!silhouette || !mhi) return EIF_STATUS_INVALID_ARGUMENT;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            
            if (silhouette->data[y * silhouette->stride + x] > 0) {
                // Motion detected: set to current timestamp
                mhi[idx] = timestamp;
            } else if (mhi[idx] > 0) {
                // Decay old motion
                if (timestamp - mhi[idx] > duration) {
                    mhi[idx] = 0;
                }
            }
        }
    }
    
    return EIF_STATUS_OK;
}
