/**
 * @file eif_cv_track.h
 * @brief Object Tracking
 */

#ifndef EIF_CV_TRACK_H
#define EIF_CV_TRACK_H

#include "eif_cv_image.h"

// ============================================================================
// Optical Flow
// ============================================================================

/**
 * @brief Lucas-Kanade optical flow (sparse)
 * 
 * Tracks points from previous frame to current frame.
 * 
 * @param prev Previous frame (grayscale)
 * @param curr Current frame (grayscale)
 * @param prev_pts Points in previous frame
 * @param num_pts Number of points
 * @param next_pts Output points in current frame
 * @param status Output status for each point (1=found, 0=lost)
 * @param win_size Window size for search (odd, typically 15-21)
 * @param max_iter Maximum iterations
 * @param pool Memory pool
 */
eif_status_t eif_cv_optical_flow_lk(const eif_cv_image_t* prev,
                                     const eif_cv_image_t* curr,
                                     const eif_cv_point2f_t* prev_pts, int num_pts,
                                     eif_cv_point2f_t* next_pts,
                                     uint8_t* status,
                                     int win_size, int max_iter,
                                     eif_memory_pool_t* pool);

// ============================================================================
// Simple Trackers
// ============================================================================

/**
 * @brief Tracked object state
 */
typedef struct {
    int id;                     // Unique track ID
    eif_cv_rect_t bbox;         // Current bounding box
    eif_cv_point2f_t center;    // Center position
    eif_cv_point2f_t velocity;  // Velocity estimate
    int age;                    // Frames since creation
    int hits;                   // Consecutive detections
    int misses;                 // Consecutive missed detections
    int state;                  // 0=tentative, 1=confirmed, 2=deleted
} eif_cv_track_t;

/**
 * @brief Multi-object tracker
 */
typedef struct {
    eif_cv_track_t* tracks;     // Active tracks
    int num_tracks;             // Number of active tracks
    int max_tracks;             // Maximum tracks
    int next_id;                // Next track ID
    int max_age;                // Max frames without detection
    int min_hits;               // Min hits to confirm
    float32_t iou_threshold;    // IoU threshold for matching
} eif_cv_tracker_t;

/**
 * @brief Initialize tracker
 */
eif_status_t eif_cv_tracker_init(eif_cv_tracker_t* tracker, int max_tracks,
                                  int max_age, int min_hits, float32_t iou_thresh,
                                  eif_memory_pool_t* pool);

/**
 * @brief Update tracker with new detections
 * 
 * @param tracker Tracker state
 * @param detections New detections
 * @param num_detections Number of detections
 * @return Number of active tracks
 */
int eif_cv_tracker_update(eif_cv_tracker_t* tracker,
                           const eif_cv_rect_t* detections, int num_detections);

/**
 * @brief Get confirmed tracks
 * 
 * @param tracker Tracker state
 * @param tracks Output confirmed tracks
 * @param max_tracks Maximum tracks to return
 * @return Number of confirmed tracks
 */
int eif_cv_tracker_get_tracks(const eif_cv_tracker_t* tracker,
                               eif_cv_track_t* tracks, int max_tracks);

// ============================================================================
// Background Subtraction
// ============================================================================

/**
 * @brief Running average background model
 */
typedef struct {
    float32_t* background;      // Background model
    float32_t* variance;        // Variance estimate
    int width, height;
    float32_t alpha;            // Learning rate
} eif_cv_bg_model_t;

/**
 * @brief Initialize background model
 */
eif_status_t eif_cv_bg_init(eif_cv_bg_model_t* model, int width, int height,
                             float32_t alpha, eif_memory_pool_t* pool);

/**
 * @brief Update background and get foreground mask
 * 
 * @param model Background model
 * @param frame Current frame (grayscale)
 * @param fg_mask Output foreground mask (binary)
 * @param threshold Detection threshold (typically 2-3 sigma)
 */
eif_status_t eif_cv_bg_update(eif_cv_bg_model_t* model,
                               const eif_cv_image_t* frame,
                               eif_cv_image_t* fg_mask,
                               float32_t threshold);

// ============================================================================
// Motion History
// ============================================================================

/**
 * @brief Update motion history image
 * 
 * @param silhouette Binary motion mask
 * @param mhi Motion history image (float32)
 * @param timestamp Current timestamp
 * @param duration History duration
 */
eif_status_t eif_cv_update_motion_history(const eif_cv_image_t* silhouette,
                                           float32_t* mhi, int width, int height,
                                           float32_t timestamp, float32_t duration);

#endif // EIF_CV_TRACK_H
