/**
 * @file eif_bf_kalman.h
 * @brief Kalman Filter Family
 * 
 * Standard Kalman Filter (KF), Extended Kalman Filter (EKF),
 * and Unscented Kalman Filter (UKF).
 */

#ifndef EIF_BF_KALMAN_H
#define EIF_BF_KALMAN_H

#include "eif_types.h"
#include "eif_status.h"
#include "eif_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Standard Kalman Filter
// ============================================================================

typedef struct {
    int n;                       ///< State dimension
    int m;                       ///< Control dimension
    int p;                       ///< Measurement dimension
    
    float32_t* x;                ///< State Estimate (n x 1)
    float32_t* P;                ///< Error Covariance (n x n)
    float32_t* Q;                ///< Process Noise Covariance (n x n)
    float32_t* R;                ///< Measurement Noise Covariance (p x p)
    float32_t* F;                ///< State Transition Matrix (n x n)
    float32_t* B;                ///< Control Input Matrix (n x m)
    float32_t* H;                ///< Observation Matrix (p x n)
    float32_t* K;                ///< Kalman Gain (n x p)
    
    float32_t* scratch;
    size_t scratch_size;
} eif_kalman_filter_t;

eif_status_t eif_kalman_init(eif_kalman_filter_t* kf, eif_memory_pool_t* pool);
eif_status_t eif_kalman_predict(eif_kalman_filter_t* kf, const float32_t* control_input);
eif_status_t eif_kalman_update(eif_kalman_filter_t* kf, const float32_t* measurement);

// ============================================================================
// Extended Kalman Filter (EKF)
// ============================================================================

typedef void (*eif_ekf_f_func)(const float32_t* x, const float32_t* u, float32_t* out_x);
typedef void (*eif_ekf_h_func)(const float32_t* x, float32_t* out_z);
typedef void (*eif_ekf_jac_f_func)(const float32_t* x, const float32_t* u, float32_t* out_F);
typedef void (*eif_ekf_jac_h_func)(const float32_t* x, float32_t* out_H);

typedef struct {
    int n;                       ///< State dimension
    int p;                       ///< Measurement dimension
    
    float32_t* x;                ///< State (n)
    float32_t* P;                ///< Covariance (n x n)
    float32_t* Q;                ///< Process Noise (n x n)
    float32_t* R;                ///< Measurement Noise (p x p)
    float32_t* F;                ///< Jacobian (n x n)
    float32_t* H;                ///< Jacobian (p x n)
    float32_t* K;                ///< Gain (n x p)
    float32_t* y;                ///< Innovation (p)
    float32_t* S;                ///< Innovation Covariance (p x p)
    
    eif_ekf_f_func f_func;
    eif_ekf_h_func h_func;
    eif_ekf_jac_f_func F_jac;
    eif_ekf_jac_h_func H_jac;
    
    float32_t* scratch;
    size_t scratch_size;
} eif_ekf_t;

eif_status_t eif_ekf_init(eif_ekf_t* ekf, eif_memory_pool_t* pool);
eif_status_t eif_ekf_predict(eif_ekf_t* ekf, const float32_t* control_input);
eif_status_t eif_ekf_update(eif_ekf_t* ekf, const float32_t* measurement);

// ============================================================================
// Unscented Kalman Filter (UKF)
// ============================================================================

typedef struct {
    int n;
    int p;
    
    float32_t* x;
    float32_t* P;
    float32_t* Q;
    float32_t* R;
    
    float32_t* sigma_points;     ///< (2n+1) x n
    float32_t* weights_m;        ///< (2n+1)
    float32_t* weights_c;        ///< (2n+1)
    int num_sigma;
    
    float32_t alpha;
    float32_t kappa;
    float32_t beta;
    float32_t lambda;
    
    float32_t* K;
    float32_t* y;
    float32_t* S;
    
    eif_ekf_f_func f_func;
    eif_ekf_h_func h_func;
    
    float32_t* scratch;
    size_t scratch_size;
} eif_ukf_t;

eif_status_t eif_ukf_init(eif_ukf_t* ukf, eif_memory_pool_t* pool);
eif_status_t eif_ukf_predict(eif_ukf_t* ukf, const float32_t* control_input);
eif_status_t eif_ukf_update(eif_ukf_t* ukf, const float32_t* measurement);

// ============================================================================
// Complementary Filter
// ============================================================================

typedef struct {
    float32_t alpha;
    float32_t angle;
} eif_complementary_filter_t;

void eif_complementary_filter_init(eif_complementary_filter_t* cf, float32_t alpha, float32_t init_angle);
float32_t eif_complementary_filter_update(eif_complementary_filter_t* cf, float32_t accel_angle, float32_t gyro_rate, float32_t dt);

#ifdef __cplusplus
}
#endif

#endif // EIF_BF_KALMAN_H
