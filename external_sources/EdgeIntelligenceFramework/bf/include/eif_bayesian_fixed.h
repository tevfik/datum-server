#ifndef EIF_BAYESIAN_FIXED_H
#define EIF_BAYESIAN_FIXED_H

#include "eif_fixedpoint.h"
#include "eif_status.h"
#include "eif_memory.h"

// Fixed-Point Kalman Filter Structure (Q15)
typedef struct {
    int n; // State dimension
    int m; // Control dimension
    int p; // Measurement dimension
    
    // State Vector (n x 1)
    q15_t* x;
    
    // Covariance Matrix (n x n)
    q15_t* P;
    
    // System Matrices
    q15_t* F; // State Transition (n x n)
    q15_t* B; // Control Input (n x m)
    q15_t* H; // Observation (p x n)
    
    // Noise Covariance
    q15_t* Q; // Process Noise (n x n)
    q15_t* R; // Measurement Noise (p x p)
    
    // Kalman Gain (n x p) - Internal buffer
    q15_t* K;
    
} eif_kalman_filter_q15_t;

// Initialize Fixed-Point Kalman Filter
// Note: Allocates K buffer from pool (persistent)
eif_status_t eif_kalman_init_q15(eif_kalman_filter_q15_t* kf, eif_memory_pool_t* pool);

// Predict Step
// Note: Uses pool for temporary buffers
eif_status_t eif_kalman_predict_q15(eif_kalman_filter_q15_t* kf, const q15_t* control_input, eif_memory_pool_t* pool);

// Update Step
// Note: Uses pool for temporary buffers
eif_status_t eif_kalman_update_q15(eif_kalman_filter_q15_t* kf, const q15_t* measurement, eif_memory_pool_t* pool);

#endif // EIF_BAYESIAN_FIXED_H
