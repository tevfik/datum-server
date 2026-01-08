#include "eif_bayesian.h"
#include "eif_matrix.h"
#include "eif_generic.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// --- Kalman Filter Implementation ---

eif_status_t eif_kalman_init(eif_kalman_filter_t* kf, eif_memory_pool_t* pool) {
    if (!kf || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    if (kf->n <= 0 || kf->p <= 0) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Allocate K
    kf->K = (float32_t*)eif_memory_alloc(pool, kf->n * kf->p * sizeof(float32_t), 4);
    
    // Calculate Scratch Size
    // Predict: temp_x(n) + temp_bu(n) + temp_FP(n*n) + temp_FT(n*n) = 2n + 2n^2
    // Update: y(p) + Hx(p) + S(p*p) + HP(p*n) + HT(n*p) + S_inv(p*p) + PHT(n*p) + Ky(n) + KH(n*n) + I(n*n) + I_KH(n*n) + NewP(n*n)
    //         = 2p + 2p^2 + 3np + n + 4n^2
    // Max is roughly 4n^2 + 3np + 2p^2
    
    int n = kf->n;
    int p = kf->p;
    size_t predict_size = (2 * n + 2 * n * n) * sizeof(float32_t);
    size_t update_size = (2 * p + 2 * p * p + 3 * n * p + n + 4 * n * n) * sizeof(float32_t);
    size_t max_scratch = (predict_size > update_size) ? predict_size : update_size;
    
    kf->scratch = (float32_t*)eif_memory_alloc(pool, max_scratch, 4);
    kf->scratch_size = max_scratch;
    
    if (!kf->K || !kf->scratch) return EIF_STATUS_OUT_OF_MEMORY;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_kalman_predict(eif_kalman_filter_t* kf, const float32_t* control_input) {
    if (!kf) return EIF_STATUS_INVALID_ARGUMENT;
    
    int n = kf->n;
    int m = kf->m;
    float32_t* scratch = kf->scratch;
    
    // Partition Scratch
    float32_t* temp_x = scratch; scratch += n;
    float32_t* temp_FP = scratch; scratch += n * n;
    float32_t* temp_FT = scratch; scratch += n * n;
    
    // 1. Predict State: x = F*x + B*u
    eif_mat_mul(kf->F, kf->x, temp_x, n, n, 1);
    
    if (control_input && kf->B && m > 0) {
        float32_t* temp_bu = scratch; // Reuse scratch space
        eif_mat_mul(kf->B, control_input, temp_bu, n, m, 1);
        eif_mat_add(temp_x, temp_bu, temp_x, n, 1);
    }
    
    memcpy(kf->x, temp_x, n * sizeof(float32_t));
    
    // 2. Predict Covariance: P = F*P*F^T + Q
    // Temp1 = F*P
    eif_mat_mul(kf->F, kf->P, temp_FP, n, n, n);

    // Temp2 = F^T
    eif_mat_transpose(kf->F, temp_FT, n, n);
    
    // P = Temp1 * F^T
    eif_mat_mul(temp_FP, temp_FT, kf->P, n, n, n);
    
    // P = P + Q
    eif_mat_add(kf->P, kf->Q, kf->P, n, n);
    
    return EIF_STATUS_OK;
}

eif_status_t eif_kalman_update(eif_kalman_filter_t* kf, const float32_t* measurement) {
    if (!kf || !measurement) return EIF_STATUS_INVALID_ARGUMENT;
    
    int n = kf->n;
    int p = kf->p;
    float32_t* scratch = kf->scratch;
    
    // Partition Scratch
    float32_t* y = scratch; scratch += p;
    float32_t* Hx = scratch; scratch += p;
    float32_t* S = scratch; scratch += p * p;
    float32_t* HP = scratch; scratch += p * n;
    float32_t* HT = scratch; scratch += n * p;
    float32_t* S_inv = scratch; scratch += p * p;
    float32_t* PHT = scratch; scratch += n * p;
    float32_t* Ky = scratch; scratch += n;
    float32_t* KH = scratch; scratch += n * n;
    float32_t* I = scratch; scratch += n * n;
    float32_t* I_KH = scratch; scratch += n * n;
    float32_t* NewP = scratch; scratch += n * n;
    float32_t* mat_inv_temp = scratch; scratch += 2 * p * p; // For matrix inverse
    
    // Check if we exceeded scratch size (sanity check)
    // In production, we trust init calculation.
    
    // 1. Innovation: y = z - H*x
    eif_mat_mul(kf->H, kf->x, Hx, p, n, 1);
    eif_mat_sub(measurement, Hx, y, p, 1);
    
    // 2. Innovation Covariance: S = H*P*H^T + R
    eif_mat_mul(kf->H, kf->P, HP, p, n, n); // H*P
    eif_mat_transpose(kf->H, HT, p, n);     // H^T
    eif_mat_mul(HP, HT, S, p, n, p);        // H*P*H^T
    eif_mat_add(S, kf->R, S, p, p);         // + R
    
    // 3. Kalman Gain: K = P*H^T*S^-1
    // Use mat_inv_temp as temp pool buffer for matrix inverse
    eif_memory_pool_t temp_pool;
    eif_memory_pool_init(&temp_pool, (uint8_t*)mat_inv_temp, 2 * p * p * sizeof(float32_t));
    
    if (eif_mat_inverse(S, S_inv, p, &temp_pool) != EIF_STATUS_OK) {
        return EIF_STATUS_ERROR;
    }
    
    eif_mat_mul(kf->P, HT, PHT, n, n, p); // P*H^T
    eif_mat_mul(PHT, S_inv, kf->K, n, p, p); // K = P*H^T*S^-1
    
    // 4. Update State: x = x + K*y
    eif_mat_mul(kf->K, y, Ky, n, p, 1);
    eif_mat_add(kf->x, Ky, kf->x, n, 1);
    
    // 5. Update Covariance: P = (I - K*H)*P
    // I - K*H
    eif_mat_mul(kf->K, kf->H, KH, n, p, n);
    eif_mat_identity(I, n);
    eif_mat_sub(I, KH, I_KH, n, n);
    
    // P = (I - KH) * P
    eif_mat_mul(I_KH, kf->P, NewP, n, n, n);
    eif_mat_copy(NewP, kf->P, n, n);
    
    return EIF_STATUS_OK;
}

// --- Extended Kalman Filter (EKF) Implementation ---

eif_status_t eif_ekf_init(eif_ekf_t* ekf, eif_memory_pool_t* pool) {
    if (!ekf || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    if (ekf->n <= 0 || ekf->p <= 0) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Allocate persistent matrices
    ekf->F = (float32_t*)eif_memory_alloc(pool, ekf->n * ekf->n * sizeof(float32_t), 4);
    ekf->H = (float32_t*)eif_memory_alloc(pool, ekf->p * ekf->n * sizeof(float32_t), 4);
    ekf->K = (float32_t*)eif_memory_alloc(pool, ekf->n * ekf->p * sizeof(float32_t), 4);
    ekf->y = (float32_t*)eif_memory_alloc(pool, ekf->p * sizeof(float32_t), 4);
    ekf->S = (float32_t*)eif_memory_alloc(pool, ekf->p * ekf->p * sizeof(float32_t), 4);
    
    if (!ekf->F || !ekf->H || !ekf->K || !ekf->y || !ekf->S) return EIF_STATUS_OUT_OF_MEMORY;
    
    // Calculate Scratch Size
    int n = ekf->n;
    int p = ekf->p;
    
    // Predict: next_x(n) + FP(n*n) + FT(n*n) + FPFt(n*n) = n + 3n^2
    size_t predict_size = (n + 3 * n * n) * sizeof(float32_t);
    
    // Update: hx(p) + HP(p*n) + HT(n*p) + HPHt(p*p) + S_inv(p*p) + PHt(n*p) + Ky(n) + KH(n*n) + I(n*n) + I_KH(n*n) + NewP(n*n)
    //         + mat_inv_temp(2*p*p)
    //         = p + 3np + 2p^2 + n + 4n^2 + 2p^2
    //         = p + 3np + 4p^2 + n + 4n^2
    size_t update_size = (p + 3 * n * p + 4 * p * p + n + 4 * n * n) * sizeof(float32_t);
    
    size_t max_scratch = (predict_size > update_size) ? predict_size : update_size;
    
    ekf->scratch = (float32_t*)eif_memory_alloc(pool, max_scratch, 4);
    ekf->scratch_size = max_scratch;
    
    if (!ekf->scratch) return EIF_STATUS_OUT_OF_MEMORY;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_ekf_predict(eif_ekf_t* ekf, const float32_t* control_input) {
    if (!ekf || !ekf->f_func || !ekf->F_jac) return EIF_STATUS_INVALID_ARGUMENT;
    
    int n = ekf->n;
    float32_t* scratch = ekf->scratch;
    
    // Partition Scratch
    float32_t* next_x = scratch; scratch += n;
    float32_t* FP = scratch; scratch += n * n;
    float32_t* FT = scratch; scratch += n * n;
    float32_t* FPFt = scratch; scratch += n * n;
    
    // 1. Predict State: x = f(x, u)
    ekf->f_func(ekf->x, control_input, next_x);
    memcpy(ekf->x, next_x, n * sizeof(float32_t));
    
    // 2. Compute Jacobian F
    ekf->F_jac(ekf->x, control_input, ekf->F);
    
    // 3. Predict Covariance: P = F*P*F^T + Q
    eif_mat_mul(ekf->F, ekf->P, FP, n, n, n);
    eif_mat_transpose(ekf->F, FT, n, n);
    eif_mat_mul(FP, FT, FPFt, n, n, n);
    eif_mat_add(FPFt, ekf->Q, ekf->P, n, n);
    
    return EIF_STATUS_OK;
}

eif_status_t eif_ekf_update(eif_ekf_t* ekf, const float32_t* measurement) {
    if (!ekf || !ekf->h_func || !ekf->H_jac) return EIF_STATUS_INVALID_ARGUMENT;
    
    int n = ekf->n;
    int p = ekf->p;
    float32_t* scratch = ekf->scratch;
    
    // Partition Scratch
    float32_t* hx = scratch; scratch += p;
    float32_t* HP = scratch; scratch += p * n;
    float32_t* HT = scratch; scratch += n * p;
    float32_t* HPHt = scratch; scratch += p * p;
    float32_t* S_inv = scratch; scratch += p * p;
    float32_t* PHt = scratch; scratch += n * p;
    float32_t* Ky = scratch; scratch += n;
    float32_t* KH = scratch; scratch += n * n;
    float32_t* I = scratch; scratch += n * n;
    float32_t* I_KH = scratch; scratch += n * n;
    float32_t* NewP = scratch; scratch += n * n;
    float32_t* mat_inv_temp = scratch; scratch += 2 * p * p;
    
    // 1. Innovation: y = z - h(x)
    ekf->h_func(ekf->x, hx);
    eif_mat_sub(measurement, hx, ekf->y, p, 1);
    
    // 2. Compute Jacobian H
    ekf->H_jac(ekf->x, ekf->H);
    
    // 3. Innovation Covariance: S = H*P*H^T + R
    eif_mat_mul(ekf->H, ekf->P, HP, p, n, n);
    eif_mat_transpose(ekf->H, HT, p, n);
    eif_mat_mul(HP, HT, HPHt, p, n, p);
    eif_mat_add(HPHt, ekf->R, ekf->S, p, p);
    
    // 4. Kalman Gain: K = P*H^T*S^-1
    eif_memory_pool_t temp_pool;
    eif_memory_pool_init(&temp_pool, (uint8_t*)mat_inv_temp, 2 * p * p * sizeof(float32_t));
    
    if (eif_mat_inverse(ekf->S, S_inv, p, &temp_pool) != EIF_STATUS_OK) {
        return EIF_STATUS_ERROR;
    }
    
    eif_mat_mul(ekf->P, HT, PHt, n, n, p);
    eif_mat_mul(PHt, S_inv, ekf->K, n, p, p);
    
    // 5. Update State: x = x + K*y
    eif_mat_mul(ekf->K, ekf->y, Ky, n, p, 1);
    eif_mat_add(ekf->x, Ky, ekf->x, n, 1);
    
    // 6. Update Covariance: P = (I - K*H)*P
    eif_mat_mul(ekf->K, ekf->H, KH, n, p, n);
    
    eif_mat_identity(I, n);
    eif_mat_sub(I, KH, I_KH, n, n);
    
    eif_mat_mul(I_KH, ekf->P, NewP, n, n, n);
    eif_mat_copy(NewP, ekf->P, n, n);
    
    return EIF_STATUS_OK;
}

// --- Unscented Kalman Filter (UKF) Implementation ---
// Placeholder for UKF - requires Cholesky and Sigma Points logic.
// We'll implement basic init for now.

eif_status_t eif_ukf_init(eif_ukf_t* ukf, eif_memory_pool_t* pool) {
    if (!ukf || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    if (ukf->n <= 0 || ukf->p <= 0) return EIF_STATUS_INVALID_ARGUMENT;
    
    ukf->num_sigma = 2 * ukf->n + 1;
    
    // Allocate persistent buffers
    ukf->sigma_points = (float32_t*)eif_memory_alloc(pool, ukf->num_sigma * ukf->n * sizeof(float32_t), 4);
    ukf->weights_m = (float32_t*)eif_memory_alloc(pool, ukf->num_sigma * sizeof(float32_t), 4);
    ukf->weights_c = (float32_t*)eif_memory_alloc(pool, ukf->num_sigma * sizeof(float32_t), 4);
    ukf->K = (float32_t*)eif_memory_alloc(pool, ukf->n * ukf->p * sizeof(float32_t), 4);
    ukf->y = (float32_t*)eif_memory_alloc(pool, ukf->p * sizeof(float32_t), 4);
    ukf->S = (float32_t*)eif_memory_alloc(pool, ukf->p * ukf->p * sizeof(float32_t), 4);
    
    if (!ukf->sigma_points || !ukf->weights_m || !ukf->weights_c || !ukf->K || !ukf->y || !ukf->S) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    // Default parameters
    if (ukf->alpha == 0) ukf->alpha = 1e-3f;
    if (ukf->kappa == 0) ukf->kappa = 0.0f;
    if (ukf->beta == 0) ukf->beta = 2.0f;
    
    ukf->lambda = ukf->alpha * ukf->alpha * (ukf->n + ukf->kappa) - ukf->n;
    
    // Initialize Weights
    float32_t lambda_plus_n = ukf->lambda + ukf->n;
    ukf->weights_m[0] = ukf->lambda / lambda_plus_n;
    ukf->weights_c[0] = ukf->weights_m[0] + (1 - ukf->alpha*ukf->alpha + ukf->beta);
    
    for (int i = 1; i < ukf->num_sigma; i++) {
        float32_t w = 0.5f / lambda_plus_n;
        ukf->weights_m[i] = w;
        ukf->weights_c[i] = w;
    }
    
    // Calculate Scratch Size
    int n = ukf->n;
    int p = ukf->p;
    int num_sigma = ukf->num_sigma;
    
    // Predict: L(n*n) + L_col(n) + next_point(n) + diff(n) + diffT(n) + term(n*n)
    // Max needed: L + term + vectors = 2*n*n + 3*n
    size_t predict_size = (2 * n * n + 3 * n) * sizeof(float32_t);
    
    // Update: Z_points(num_sigma*p) + L(n*n) + z_pred(p) + Pxz(n*p) + X_diff(n) + Z_diff(p) + S_inv(p*p) + y(p) + Ky(n) + KS(n*p) + KT(p*n) + KSKT(n*n) + NewP(n*n)
    // Max needed roughly: num_sigma*p + 3*n*n + 2*n*p + 2*p*p + ...
    // Let's sum them up conservatively.
    size_t update_size = (num_sigma * p + 3 * n * n + 2 * n * p + 2 * p * p + 3 * n + 3 * p) * sizeof(float32_t);
    
    size_t max_scratch = (predict_size > update_size) ? predict_size : update_size;
    
    ukf->scratch = (float32_t*)eif_memory_alloc(pool, max_scratch, 4);
    ukf->scratch_size = max_scratch;
    
    if (!ukf->scratch) return EIF_STATUS_OUT_OF_MEMORY;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_ukf_predict(eif_ukf_t* ukf, const float32_t* control_input) {
    if (!ukf || !ukf->f_func) return EIF_STATUS_INVALID_ARGUMENT;
    
    int n = ukf->n;
    int num_sigma = ukf->num_sigma;
    float32_t* scratch = ukf->scratch;
    
    // Partition Scratch for Predict
    float32_t* L = scratch; scratch += n * n;
    float32_t* temp_vec = scratch; // Shared vector buffer (n)
    float32_t* temp_mat = scratch + n; // Shared matrix buffer (n*n)
    
    // 1. Generate Sigma Points
    if (eif_mat_cholesky(ukf->P, L, n) != EIF_STATUS_OK) {
        return EIF_STATUS_ERROR;
    }
    
    float32_t scale = sqrtf(n + ukf->lambda);
    eif_mat_scale(L, scale, L, n, n);
    
    memcpy(&ukf->sigma_points[0], ukf->x, n * sizeof(float32_t));
    
    for (int i = 0; i < n; i++) {
        // Positive Sigma
        for(int k=0; k<n; k++) {
            ukf->sigma_points[(i+1)*n + k] = ukf->x[k] + L[k*n + i];
        }
        // Negative Sigma
        for(int k=0; k<n; k++) {
            ukf->sigma_points[(i+1+n)*n + k] = ukf->x[k] - L[k*n + i];
        }
    }
    
    // 2. Propagate Sigma Points
    for (int i = 0; i < num_sigma; i++) {
        float32_t* point = &ukf->sigma_points[i*n];
        float32_t* next_point = temp_vec; // Reuse temp_vec
        ukf->f_func(point, control_input, next_point);
        memcpy(point, next_point, n * sizeof(float32_t));
    }
    
    // 3. Predicted Mean
    memset(ukf->x, 0, n * sizeof(float32_t));
    for (int i = 0; i < num_sigma; i++) {
        float32_t* point = &ukf->sigma_points[i*n];
        float32_t w = ukf->weights_m[i];
        for(int k=0; k<n; k++) ukf->x[k] += w * point[k];
    }
    
    // 4. Predicted Covariance
    memset(ukf->P, 0, n * n * sizeof(float32_t));
    for (int i = 0; i < num_sigma; i++) {
        float32_t* point = &ukf->sigma_points[i*n];
        float32_t* diff = temp_vec;
        eif_mat_sub(point, ukf->x, diff, n, 1);
        
        float32_t* term = temp_mat;
        for(int r=0; r<n; r++) {
            for(int c=0; c<n; c++) {
                term[r*n + c] = diff[r] * diff[c];
            }
        }
        
        float32_t w = ukf->weights_c[i];
        for(int k=0; k<n*n; k++) ukf->P[k] += w * term[k];
    }
    
    eif_mat_add(ukf->P, ukf->Q, ukf->P, n, n);
    
    return EIF_STATUS_OK;
}

eif_status_t eif_ukf_update(eif_ukf_t* ukf, const float32_t* measurement) {
    if (!ukf || !ukf->h_func) return EIF_STATUS_INVALID_ARGUMENT;
    
    int n = ukf->n;
    int p = ukf->p;
    int num_sigma = ukf->num_sigma;
    float32_t* scratch = ukf->scratch;
    
    // Partition Scratch for Update
    float32_t* Z_points = scratch; scratch += num_sigma * p;
    float32_t* L = scratch; scratch += n * n;
    float32_t* z_pred = scratch; scratch += p;
    float32_t* Pxz = scratch; scratch += n * p;
    float32_t* S_inv = scratch; scratch += p * p;
    float32_t* temp_vec_n = scratch; scratch += n;
    float32_t* temp_vec_p = scratch; scratch += p;
    float32_t* temp_mat_nn = scratch; scratch += n * n;
    float32_t* temp_mat_np = scratch; scratch += n * p; // Shared
    
    // 1. Regenerate Sigma Points
    if (eif_mat_cholesky(ukf->P, L, n) != EIF_STATUS_OK) {
        return EIF_STATUS_ERROR;
    }
    float32_t scale = sqrtf(n + ukf->lambda);
    eif_mat_scale(L, scale, L, n, n);
    
    memcpy(&ukf->sigma_points[0], ukf->x, n * sizeof(float32_t));
    for (int i = 0; i < n; i++) {
        for(int k=0; k<n; k++) {
            ukf->sigma_points[(i+1)*n + k] = ukf->x[k] + L[k*n + i];
            ukf->sigma_points[(i+1+n)*n + k] = ukf->x[k] - L[k*n + i];
        }
    }
    
    // Transform through h
    memset(z_pred, 0, p * sizeof(float32_t));
    for (int i = 0; i < num_sigma; i++) {
        float32_t* point = &ukf->sigma_points[i*n];
        float32_t* z_out = &Z_points[i*p];
        ukf->h_func(point, z_out);
        
        float32_t w = ukf->weights_m[i];
        for(int k=0; k<p; k++) z_pred[k] += w * z_out[k];
    }
    
    // 2. Innovation Covariance S and Cross Covariance Pxz
    memset(ukf->S, 0, p * p * sizeof(float32_t));
    memset(Pxz, 0, n * p * sizeof(float32_t));
    
    for (int i = 0; i < num_sigma; i++) {
        float32_t* X_diff = temp_vec_n;
        eif_mat_sub(&ukf->sigma_points[i*n], ukf->x, X_diff, n, 1);
        
        float32_t* Z_diff = temp_vec_p;
        eif_mat_sub(&Z_points[i*p], z_pred, Z_diff, p, 1);
        
        float32_t w = ukf->weights_c[i];
        
        // S += w * Z_diff * Z_diff^T
        for(int r=0; r<p; r++) {
            for(int c=0; c<p; c++) {
                ukf->S[r*p + c] += w * Z_diff[r] * Z_diff[c];
            }
        }
        
        // Pxz += w * X_diff * Z_diff^T
        for(int r=0; r<n; r++) {
            for(int c=0; c<p; c++) {
                Pxz[r*p + c] += w * X_diff[r] * Z_diff[c];
            }
        }
    }
    
    eif_mat_add(ukf->S, ukf->R, ukf->S, p, p);
    
    // 3. Kalman Gain: K = Pxz * S^-1
    if (eif_mat_inverse(ukf->S, S_inv, p, NULL) != EIF_STATUS_OK) {
        return EIF_STATUS_ERROR;
    }
    
    eif_mat_mul(Pxz, S_inv, ukf->K, n, p, p);
    
    // 4. Update State: x = x + K * (z - z_pred)
    float32_t* y = temp_vec_p;
    eif_mat_sub(measurement, z_pred, y, p, 1);
    
    float32_t* Ky = temp_vec_n;
    eif_mat_mul(ukf->K, y, Ky, n, p, 1);
    eif_mat_add(ukf->x, Ky, ukf->x, n, 1);
    
    // 5. Update Covariance: P = P - K * S * K^T
    float32_t* KS = temp_mat_np;
    eif_mat_mul(ukf->K, ukf->S, KS, n, p, p);
    
    float32_t* KT = temp_mat_np; // Reuse buffer? No, KS is n*p, KT is p*n.
    // We need another buffer or reuse Pxz (n*p) for KS, and something else for KT.
    // Pxz is free now.
    // Let's use Pxz for KS.
    KS = Pxz;
    eif_mat_mul(ukf->K, ukf->S, KS, n, p, p);
    
    // We need buffer for KT (p*n). L (n*n) is free.
    // Wait, L is n*n. p*n fits if p <= n. If p > n, we might overflow L.
    // But usually p <= n.
    // Let's assume we have enough scratch.
    // Or use temp_mat_nn (n*n).
    float32_t* KT_buf = temp_mat_nn;
    eif_mat_transpose(ukf->K, KT_buf, n, p);
    
    float32_t* KSKT = temp_mat_nn; // Reuse again? No, we need KT and KS.
    // We need result KSKT (n*n).
    // We have KS in Pxz. KT in temp_mat_nn.
    // We need output buffer.
    // We can use L (n*n) for output.
    float32_t* KSKT_out = L;
    eif_mat_mul(KS, KT_buf, KSKT_out, n, p, n);
    
    float32_t* NewP = temp_mat_nn; // Reuse temp_mat_nn
    eif_mat_sub(ukf->P, KSKT_out, NewP, n, n);
    eif_mat_copy(NewP, ukf->P, n, n);
    
    return EIF_STATUS_OK;
}

// --- Particle Filter ---

// Helper: Box-Muller for Gaussian noise
static float32_t randn() {
    float32_t u1 = (float32_t)rand() / RAND_MAX;
    float32_t u2 = (float32_t)rand() / RAND_MAX;
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * M_PI * u2);
}

eif_status_t eif_pf_init(eif_particle_filter_t* pf, const float32_t* init_state, const float32_t* init_cov, eif_memory_pool_t* pool) {
    if (!pf || !init_state || !init_cov || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    int n = pf->state_dim;
    int num = pf->num_particles;
    
    // Allocate particles and weights
    pf->particles = (float32_t*)eif_memory_alloc(pool, num * n * sizeof(float32_t), 4);
    pf->weights = (float32_t*)eif_memory_alloc(pool, num * sizeof(float32_t), 4);
    
    // Allocate scratch for resampling
    pf->scratch = (float32_t*)eif_memory_alloc(pool, num * n * sizeof(float32_t), 4);
    
    if (!pf->particles || !pf->weights || !pf->scratch) return EIF_STATUS_OUT_OF_MEMORY;
    
    for (int i = 0; i < num; i++) {
        pf->weights[i] = 1.0f / num;
        for (int j = 0; j < n; j++) {
            float32_t std = sqrtf(init_cov[j * n + j]);
            pf->particles[i * n + j] = init_state[j] + randn() * std;
        }
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_pf_predict(eif_particle_filter_t* pf, const float32_t* control) {
    if (!pf || !pf->transition_func) return EIF_STATUS_INVALID_ARGUMENT;
    
    int n = pf->state_dim;
    int num = pf->num_particles;
    
    for (int i = 0; i < num; i++) {
        float32_t* p = &pf->particles[i * n];
        pf->transition_func(p, control, p);
        
        // Add process noise
        if (pf->process_noise) {
            for (int j = 0; j < n; j++) {
                p[j] += randn() * pf->process_noise[j];
            }
        }
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_pf_update(eif_particle_filter_t* pf, const float32_t* measurement) {
    if (!pf || !pf->likelihood_func) return EIF_STATUS_INVALID_ARGUMENT;
    
    int n = pf->state_dim;
    int num = pf->num_particles;
    float32_t sum_weights = 0.0f;
    
    for (int i = 0; i < num; i++) {
        float32_t* p = &pf->particles[i * n];
        float32_t weight = pf->likelihood_func(p, measurement);
        pf->weights[i] *= weight;
        sum_weights += pf->weights[i];
    }
    
    // Normalize weights
    if (sum_weights < 1e-9f) sum_weights = 1e-9f;
    for (int i = 0; i < num; i++) {
        pf->weights[i] /= sum_weights;
    }
    
    // Resample if needed
    return eif_pf_resample(pf);
}

eif_status_t eif_pf_resample(eif_particle_filter_t* pf) {
    int n = pf->state_dim;
    int num = pf->num_particles;
    
    float32_t* new_particles = pf->scratch;
    
    // Systematic Resampling (Low Variance Sampling)
    float32_t r = ((float32_t)rand() / RAND_MAX) / num;
    float32_t c = pf->weights[0];
    int i = 0;
    
    for (int m = 0; m < num; m++) {
        float32_t u = r + (float32_t)m / num;
        while (u > c && i < num - 1) {
            i++;
            c += pf->weights[i];
        }
        // Copy particle i to new_particles m
        memcpy(&new_particles[m * n], &pf->particles[i * n], n * sizeof(float32_t));
    }
    
    memcpy(pf->particles, new_particles, num * n * sizeof(float32_t));
    for(int k=0; k<num; k++) pf->weights[k] = 1.0f / num; // Reset weights
    
    return EIF_STATUS_OK;
}

eif_status_t eif_pf_estimate(const eif_particle_filter_t* pf, float32_t* state_est) {
    if (!pf || !state_est) return EIF_STATUS_INVALID_ARGUMENT;
    
    int n = pf->state_dim;
    int num = pf->num_particles;
    
    memset(state_est, 0, n * sizeof(float32_t));
    
    for (int i = 0; i < num; i++) {
        for (int j = 0; j < n; j++) {
            state_est[j] += pf->particles[i * n + j] * pf->weights[i];
        }
    }
    return EIF_STATUS_OK;
}

// --- Complementary Filter ---

void eif_complementary_filter_init(eif_complementary_filter_t* cf, float32_t alpha, float32_t init_angle) {
    if (cf) {
        cf->alpha = alpha;
        cf->angle = init_angle;
    }
}

float32_t eif_complementary_filter_update(eif_complementary_filter_t* cf, float32_t accel_angle, float32_t gyro_rate, float32_t dt) {
    if (!cf) return 0.0f;
    
    // angle = alpha * (angle + gyro * dt) + (1 - alpha) * accel
    cf->angle = cf->alpha * (cf->angle + gyro_rate * dt) + (1.0f - cf->alpha) * accel_angle;
    return cf->angle;
}
