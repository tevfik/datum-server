/**
 * @file eif_bf_slam.h
 * @brief SLAM Algorithms
 * 
 * EKF-SLAM and UKF-SLAM for 2D localization and mapping.
 */

#ifndef EIF_BF_SLAM_H
#define EIF_BF_SLAM_H

#include "eif_types.h"
#include "eif_status.h"
#include "eif_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// EKF-SLAM (2D)
// ============================================================================

typedef struct {
    int num_landmarks;
    float32_t* state;            ///< [x, y, theta, lx_1, ly_1, ..., lx_N, ly_N]
    float32_t* P;                ///< (3+2N) x (3+2N) covariance
} eif_ekf_slam_t;

eif_status_t eif_ekf_slam_init(eif_ekf_slam_t* slam, int num_landmarks, eif_memory_pool_t* pool);
eif_status_t eif_ekf_slam_predict(eif_ekf_slam_t* slam, float32_t v, float32_t w, float32_t dt, 
                                   float32_t std_v, float32_t std_w);
eif_status_t eif_ekf_slam_update(eif_ekf_slam_t* slam, int landmark_id, float32_t range, float32_t bearing, 
                                  float32_t std_range, float32_t std_bearing, eif_memory_pool_t* pool);

// ============================================================================
// UKF-SLAM (2D)
// ============================================================================

typedef struct {
    int num_landmarks;
    int state_dim;               ///< 3 + 2*N
    
    float32_t* state;
    float32_t* P;
    
    float32_t alpha;
    float32_t kappa;
    float32_t beta;
    float32_t lambda;
    
    float32_t* sigma_points;     ///< (2*dim + 1) x dim
    float32_t* wm;               ///< Weights for mean
    float32_t* wc;               ///< Weights for covariance
} eif_ukf_slam_t;

eif_status_t eif_ukf_slam_init(eif_ukf_slam_t* slam, int num_landmarks, eif_memory_pool_t* pool);
eif_status_t eif_ukf_slam_predict(eif_ukf_slam_t* slam, float32_t v, float32_t w, float32_t dt, 
                                   float32_t std_v, float32_t std_w, eif_memory_pool_t* pool);
eif_status_t eif_ukf_slam_update(eif_ukf_slam_t* slam, int landmark_id, float32_t range, float32_t bearing, 
                                  float32_t std_range, float32_t std_bearing, eif_memory_pool_t* pool);

#ifdef __cplusplus
}
#endif

#endif // EIF_BF_SLAM_H
