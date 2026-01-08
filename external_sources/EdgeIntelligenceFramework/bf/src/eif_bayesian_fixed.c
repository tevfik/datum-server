#include "eif_bayesian_fixed.h"
#include "eif_matrix_fixed.h"
#include "eif_utils.h"
#include <stdlib.h>

eif_status_t eif_kalman_init_q15(eif_kalman_filter_q15_t* kf, eif_memory_pool_t* pool) {
    if (!kf || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    if (kf->n <= 0 || kf->p <= 0) return EIF_STATUS_INVALID_ARGUMENT;
    
    kf->K = (q15_t*)eif_memory_alloc(pool, kf->n * kf->p * sizeof(q15_t), 4);
    if (!kf->K) return EIF_STATUS_OUT_OF_MEMORY;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_kalman_predict_q15(eif_kalman_filter_q15_t* kf, const q15_t* control_input, eif_memory_pool_t* pool) {
    if (!kf || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    int n = kf->n;
    int m = kf->m;
    
    // 1. Predict State: x = F*x + B*u
    q15_t* temp_x = (q15_t*)eif_memory_alloc(pool, n * sizeof(q15_t), 4);
    if (!temp_x) return EIF_STATUS_OUT_OF_MEMORY;
    
    // F*x
    eif_mat_mul_q15(kf->F, kf->x, temp_x, n, n, 1);
    
    // + B*u
    if (control_input && kf->B && m > 0) {
        q15_t* temp_bu = (q15_t*)eif_memory_alloc(pool, n * sizeof(q15_t), 4);
        if (temp_bu) {
            eif_mat_mul_q15(kf->B, control_input, temp_bu, n, m, 1);
            eif_mat_add_q15(temp_x, temp_bu, temp_x, n, 1);
            // No free needed
        }
    }
    
    // Update x
    EIF_MEMCPY(kf->x, temp_x, n * sizeof(q15_t));
    // No free needed
    
    // 2. Predict Covariance: P = F*P*F^T + Q
    // Temp1 = F*P
    q15_t* temp_FP = (q15_t*)eif_memory_alloc(pool, n * n * sizeof(q15_t), 4);
    eif_mat_mul_q15(kf->F, kf->P, temp_FP, n, n, n);
    
    // Temp2 = F^T
    q15_t* temp_FT = (q15_t*)eif_memory_alloc(pool, n * n * sizeof(q15_t), 4);
    eif_mat_transpose_q15(kf->F, temp_FT, n, n);
    
    // P = Temp1 * F^T
    eif_mat_mul_q15(temp_FP, temp_FT, kf->P, n, n, n);
    
    // P = P + Q
    eif_mat_add_q15(kf->P, kf->Q, kf->P, n, n);
    
    return EIF_STATUS_OK;
}

eif_status_t eif_kalman_update_q15(eif_kalman_filter_q15_t* kf, const q15_t* measurement, eif_memory_pool_t* pool) {
    if (!kf || !measurement || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    int n = kf->n;
    int p = kf->p;
    
    // 1. Innovation: y = z - H*x
    q15_t* y = (q15_t*)eif_memory_alloc(pool, p * sizeof(q15_t), 4);
    q15_t* Hx = (q15_t*)eif_memory_alloc(pool, p * sizeof(q15_t), 4);
    
    eif_mat_mul_q15(kf->H, kf->x, Hx, p, n, 1);
    eif_mat_sub_q15(measurement, Hx, y, p, 1);
    
    // 2. Innovation Covariance: S = H*P*H^T + R
    q15_t* S = (q15_t*)eif_memory_alloc(pool, p * p * sizeof(q15_t), 4);
    q15_t* HP = (q15_t*)eif_memory_alloc(pool, p * n * sizeof(q15_t), 4);
    q15_t* HT = (q15_t*)eif_memory_alloc(pool, n * p * sizeof(q15_t), 4);
    
    eif_mat_mul_q15(kf->H, kf->P, HP, p, n, n); // H*P
    eif_mat_transpose_q15(kf->H, HT, p, n);     // H^T
    eif_mat_mul_q15(HP, HT, S, p, n, p);        // H*P*H^T
    eif_mat_add_q15(S, kf->R, S, p, p);         // + R
    
    // 3. Kalman Gain: K = P*H^T*S^-1
    q15_t* S_inv = (q15_t*)eif_memory_alloc(pool, p * p * sizeof(q15_t), 4);
    if (eif_mat_inverse_q15(S, S_inv, p, pool) != EIF_STATUS_OK) {
        return EIF_STATUS_ERROR;
    }
    
    q15_t* PHT = (q15_t*)eif_memory_alloc(pool, n * p * sizeof(q15_t), 4);
    eif_mat_mul_q15(kf->P, HT, PHT, n, n, p); // P*H^T
    eif_mat_mul_q15(PHT, S_inv, kf->K, n, p, p); // K = P*H^T*S^-1
    
    // 4. Update State: x = x + K*y
    q15_t* Ky = (q15_t*)eif_memory_alloc(pool, n * sizeof(q15_t), 4);
    eif_mat_mul_q15(kf->K, y, Ky, n, p, 1);
    eif_mat_add_q15(kf->x, Ky, kf->x, n, 1);
    
    // 5. Update Covariance: P = (I - K*H)*P
    // I - K*H
    q15_t* KH = (q15_t*)eif_memory_alloc(pool, n * n * sizeof(q15_t), 4);
    eif_mat_mul_q15(kf->K, kf->H, KH, n, p, n);
    
    q15_t* I = (q15_t*)eif_memory_alloc(pool, n * n * sizeof(q15_t), 4);
    eif_mat_identity_q15(I, n);
    
    q15_t* I_KH = (q15_t*)eif_memory_alloc(pool, n * n * sizeof(q15_t), 4);
    eif_mat_sub_q15(I, KH, I_KH, n, n);
    
    // P = (I - KH) * P
    q15_t* NewP = (q15_t*)eif_memory_alloc(pool, n * n * sizeof(q15_t), 4);
    eif_mat_mul_q15(I_KH, kf->P, NewP, n, n, n);
    eif_mat_copy_q15(NewP, kf->P, n, n);
    
    return EIF_STATUS_OK;
}
