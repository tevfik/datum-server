#include "eif_bayesian.h"
#include "eif_matrix.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Helper to normalize angle to [-pi, pi]
static float32_t normalize_angle(float32_t angle) {
    while (angle > M_PI) angle -= 2.0f * M_PI;
    while (angle < -M_PI) angle += 2.0f * M_PI;
    return angle;
}

eif_status_t eif_ekf_slam_init(eif_ekf_slam_t* slam, int num_landmarks, eif_memory_pool_t* pool) {
    if (!slam || num_landmarks <= 0 || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    slam->num_landmarks = num_landmarks;
    int dim = 3 + 2 * num_landmarks;
    
    slam->state = (float32_t*)eif_memory_alloc(pool, dim * sizeof(float32_t), 4);
    slam->P = (float32_t*)eif_memory_alloc(pool, dim * dim * sizeof(float32_t), 4);
    
    if (!slam->state || !slam->P) return EIF_STATUS_OUT_OF_MEMORY;
    
    // Initialize state to 0
    memset(slam->state, 0, dim * sizeof(float32_t));
    
    // Initialize P to small value for robot pose, large value for landmarks (unknown)
    memset(slam->P, 0, dim * dim * sizeof(float32_t));
    
    // Robot pose uncertainty (small)
    slam->P[0 * dim + 0] = 0.001f;
    slam->P[1 * dim + 1] = 0.001f;
    slam->P[2 * dim + 2] = 0.001f;
    
    // Landmark uncertainty (large)
    for (int i = 3; i < dim; i++) {
        slam->P[i * dim + i] = 1000.0f;
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_ekf_slam_predict(eif_ekf_slam_t* slam, float32_t v, float32_t w, float32_t dt, float32_t std_v, float32_t std_w) {
    if (!slam) return EIF_STATUS_INVALID_ARGUMENT;
    
    int dim = 3 + 2 * slam->num_landmarks;
    float32_t theta = slam->state[2];
    
    // 1. State Prediction (Motion Model)
    // x' = x + v*cos(theta)*dt
    // y' = y + v*sin(theta)*dt
    // theta' = theta + w*dt
    
    // Avoid division by zero if w is small? No, standard model is fine.
    // If w is very small, use linear approximation?
    // Here simple Euler integration.
    
    slam->state[0] += v * cosf(theta) * dt;
    slam->state[1] += v * sinf(theta) * dt;
    slam->state[2] += w * dt;
    slam->state[2] = normalize_angle(slam->state[2]);
    
    // 2. Jacobian G (Motion Model Jacobian w.r.t State)
    // G is Identity except for 3x3 block for robot pose.
    // G_x = [ 1, 0, -v*sin(theta)*dt ]
    // G_y = [ 0, 1,  v*cos(theta)*dt ]
    // G_th= [ 0, 0,  1 ]
    
    float32_t G[3][3] = {
        {1.0f, 0.0f, -v * sinf(theta) * dt},
        {0.0f, 1.0f,  v * cosf(theta) * dt},
        {0.0f, 0.0f,  1.0f}
    };
    
    // 3. Covariance Update: P = G * P * G^T + Q
    // Since G is identity for landmarks, we only update the robot-robot, robot-landmark, and landmark-robot blocks.
    // Landmark-Landmark block remains unchanged (except for adding Q which is 0 for landmarks).
    // Actually, P' = G P G^T.
    // Let P_rr be 3x3 robot cov.
    // P_rl be 3x(2N) robot-landmark cov.
    // P_lr be (2N)x3 landmark-robot cov.
    // P_ll be (2N)x(2N) landmark-landmark cov.
    
    // New P_rr = G_r * P_rr * G_r^T + Q_r
    // New P_rl = G_r * P_rl
    // New P_lr = P_lr * G_r^T
    // New P_ll = P_ll
    
    // We can do this in place carefully or use temp buffer for affected rows/cols.
    // Let's use a simplified update for the 3x3 part and the cross terms.
    
    // Update P_rr (3x3)
    // Need temp copy of P_rr and P_rl?
    // Yes.
    
    // Since we don't have a matrix library handy for partial updates, let's do it element-wise for the 3 rows/cols.
    
    // Q matrix (Process Noise)
    // V = [v, w]^T
    // M = [std_v^2, 0; 0, std_w^2]
    // Jacobian V w.r.t control input...
    // Let's just assume additive noise Q_r on x, y, theta directly for simplicity, or project it.
    // Simple: Q_r = diag(std_v^2 * dt^2, std_v^2 * dt^2, std_w^2 * dt^2) (Approx)
    
    float32_t Q[3] = {std_v * std_v * dt * dt, std_v * std_v * dt * dt, std_w * std_w * dt * dt}; // Very rough approx
    
    // Update P (Naive full matrix multiplication is too slow O(N^2), but G is sparse)
    // We only need to update first 3 rows and first 3 columns.
    
    // 1. Update P_rl (first 3 rows, cols 3..end)
    // P_rl_new = G_r * P_rl
    for (int j = 3; j < dim; j++) {
        float32_t p0 = slam->P[0 * dim + j];
        float32_t p1 = slam->P[1 * dim + j];
        float32_t p2 = slam->P[2 * dim + j];
        
        slam->P[0 * dim + j] = G[0][0]*p0 + G[0][1]*p1 + G[0][2]*p2;
        slam->P[1 * dim + j] = G[1][0]*p0 + G[1][1]*p1 + G[1][2]*p2;
        slam->P[2 * dim + j] = G[2][0]*p0 + G[2][1]*p1 + G[2][2]*p2;
        
        // Symmetry: P_lr = P_rl^T
        slam->P[j * dim + 0] = slam->P[0 * dim + j];
        slam->P[j * dim + 1] = slam->P[1 * dim + j];
        slam->P[j * dim + 2] = slam->P[2 * dim + j];
    }
    
    // 2. Update P_rr (3x3)
    // P_rr_new = G_r * P_rr * G_r^T + Q
    float32_t P_rr[3][3];
    for(int i=0; i<3; i++) for(int j=0; j<3; j++) P_rr[i][j] = slam->P[i*dim+j];
    
    float32_t Temp[3][3]; // G * P_rr
    for(int i=0; i<3; i++) {
        for(int j=0; j<3; j++) {
            Temp[i][j] = 0;
            for(int k=0; k<3; k++) Temp[i][j] += G[i][k] * P_rr[k][j];
        }
    }
    
    for(int i=0; i<3; i++) {
        for(int j=0; j<3; j++) {
            float32_t val = 0;
            for(int k=0; k<3; k++) val += Temp[i][k] * G[j][k]; // G^T
            slam->P[i*dim+j] = val;
            if (i==j) slam->P[i*dim+j] += Q[i];
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_ekf_slam_update(eif_ekf_slam_t* slam, int landmark_id, float32_t range, float32_t bearing, float32_t std_range, float32_t std_bearing, eif_memory_pool_t* pool) {
    if (!slam || landmark_id < 0 || landmark_id >= slam->num_landmarks || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    int dim = 3 + 2 * slam->num_landmarks;
    int l_idx = 3 + 2 * landmark_id;
    
    // Robot state
    float32_t rx = slam->state[0];
    float32_t ry = slam->state[1];
    float32_t rth = slam->state[2];
    
    // Landmark state
    float32_t lx = slam->state[l_idx];
    float32_t ly = slam->state[l_idx + 1];
    
    // If landmark is uninitialized (high variance), initialize it
    if (slam->P[l_idx * dim + l_idx] > 100.0f) {
        lx = rx + range * cosf(rth + bearing);
        ly = ry + range * sinf(rth + bearing);
        slam->state[l_idx] = lx;
        slam->state[l_idx + 1] = ly;
        
        // We should also initialize covariance properly, but for now let EKF handle it in next steps?
        // Or better, set it now.
        // But standard EKF-SLAM initializes on first sighting.
        // Let's assume it's initialized now.
    }
    
    // 1. Expected measurement
    float32_t dx = lx - rx;
    float32_t dy = ly - ry;
    float32_t q = dx*dx + dy*dy;
    float32_t sqrt_q = sqrtf(q);
    
    float32_t z_hat_range = sqrt_q;
    float32_t z_hat_bearing = normalize_angle(atan2f(dy, dx) - rth);
    
    // 2. Jacobian H
    // H has size 2 x dim.
    // Non-zero for robot (0,1,2) and landmark (l_idx, l_idx+1)
    
    // H_r = ...
    // H_l = ...
    
    // Allocate H, K, S, PHt...
    // Use pool for temporary matrices.
    
    // Since dim can be large, we should be careful.
    // But usually N is small in embedded.
    // Let's implement a simplified update that only touches relevant columns?
    // No, K (Kalman Gain) is dim x 2. It updates ALL states (loop closure).
    
    // Let's allocate K (dim x 2) and H (2 x dim)
    // Actually H is sparse. We can optimize.
    
    // H matrix elements:
    // H_range w.r.t rx, ry, rth, lx, ly
    // dr/drx = -dx/sqrt(q)
    // dr/dry = -dy/sqrt(q)
    // dr/drth = 0
    // dr/dlx = dx/sqrt(q)
    // dr/dly = dy/sqrt(q)
    
    // H_bearing w.r.t ...
    // db/drx = dy/q
    // db/dry = -dx/q
    // db/drth = -1
    // db/dlx = -dy/q
    // db/dly = dx/q
    
    float32_t* K = (float32_t*)eif_memory_alloc(pool, dim * 2 * sizeof(float32_t), 4);
    if (!K) return EIF_STATUS_OUT_OF_MEMORY;
    
    // Innovation S (2x2) = H P H^T + R
    // R = diag(std_range^2, std_bearing^2)
    
    // Calculate PH^T (dim x 2)
    // PH^T[:, 0] = P * H_range^T
    // PH^T[:, 1] = P * H_bearing^T
    
    // H is sparse, so P * H^T is linear combination of columns of P.
    // Col 0 of PH^T = P[:,0]*H_r0 + P[:,1]*H_r1 + P[:,l]*H_l0 + P[:,l+1]*H_l1
    
    float32_t Hr[3] = {-dx/sqrt_q, -dy/sqrt_q, 0};
    float32_t Hl[2] = {dx/sqrt_q, dy/sqrt_q};
    float32_t Hb_r[3] = {dy/q, -dx/q, -1};
    float32_t Hb_l[2] = {-dy/q, dx/q};
    
    float32_t* PHt = K; // Reuse K buffer for PHt initially? No, K is needed later.
    // Let's allocate PHt
    float32_t* PHt_buf = (float32_t*)eif_memory_alloc(pool, dim * 2 * sizeof(float32_t), 4);
    if (!PHt_buf) return EIF_STATUS_OUT_OF_MEMORY;
    
    for (int i = 0; i < dim; i++) {
        // Range row
        PHt_buf[i*2 + 0] = slam->P[i*dim + 0]*Hr[0] + slam->P[i*dim + 1]*Hr[1] + slam->P[i*dim + 2]*Hr[2] +
                           slam->P[i*dim + l_idx]*Hl[0] + slam->P[i*dim + l_idx + 1]*Hl[1];
                           
        // Bearing row
        PHt_buf[i*2 + 1] = slam->P[i*dim + 0]*Hb_r[0] + slam->P[i*dim + 1]*Hb_r[1] + slam->P[i*dim + 2]*Hb_r[2] +
                           slam->P[i*dim + l_idx]*Hb_l[0] + slam->P[i*dim + l_idx + 1]*Hb_l[1];
    }
    
    // S = H * PH^T + R
    // S is 2x2
    float32_t S[2][2];
    // S[0][0] = H_range * PHt[:, 0]
    // But H is sparse.
    S[0][0] = Hr[0]*PHt_buf[0*2+0] + Hr[1]*PHt_buf[1*2+0] + Hr[2]*PHt_buf[2*2+0] +
              Hl[0]*PHt_buf[l_idx*2+0] + Hl[1]*PHt_buf[(l_idx+1)*2+0] + std_range*std_range;
              
    S[0][1] = Hr[0]*PHt_buf[0*2+1] + Hr[1]*PHt_buf[1*2+1] + Hr[2]*PHt_buf[2*2+1] +
              Hl[0]*PHt_buf[l_idx*2+1] + Hl[1]*PHt_buf[(l_idx+1)*2+1];
              
    S[1][0] = S[0][1]; // Symmetric
    
    S[1][1] = Hb_r[0]*PHt_buf[0*2+1] + Hb_r[1]*PHt_buf[1*2+1] + Hb_r[2]*PHt_buf[2*2+1] +
              Hb_l[0]*PHt_buf[l_idx*2+1] + Hb_l[1]*PHt_buf[(l_idx+1)*2+1] + std_bearing*std_bearing;
              
    // Invert S
    float32_t det = S[0][0]*S[1][1] - S[0][1]*S[1][0];
    if (fabs(det) < 1e-6f) return EIF_STATUS_ERROR;
    float32_t invDet = 1.0f / det;
    float32_t Sinv[2][2];
    Sinv[0][0] = S[1][1] * invDet;
    Sinv[0][1] = -S[0][1] * invDet;
    Sinv[1][0] = -S[1][0] * invDet;
    Sinv[1][1] = S[0][0] * invDet;
    
    // K = PH^T * S^-1
    for (int i = 0; i < dim; i++) {
        K[i*2 + 0] = PHt_buf[i*2 + 0]*Sinv[0][0] + PHt_buf[i*2 + 1]*Sinv[1][0];
        K[i*2 + 1] = PHt_buf[i*2 + 0]*Sinv[0][1] + PHt_buf[i*2 + 1]*Sinv[1][1];
    }
    
    // Update State: x = x + K * y
    float32_t y_range = range - z_hat_range;
    float32_t y_bearing = normalize_angle(bearing - z_hat_bearing);
    
    for (int i = 0; i < dim; i++) {
        slam->state[i] += K[i*2 + 0]*y_range + K[i*2 + 1]*y_bearing;
    }
    slam->state[2] = normalize_angle(slam->state[2]);
    
    // Update Covariance: P = (I - K*H) * P = P - K * (H * P)
    // H * P is (PH^T)^T
    // So P_new = P - K * PHt^T
    
    for (int i = 0; i < dim; i++) {
        for (int j = 0; j < dim; j++) {
            // K[i] * PHt[j]^T
            float32_t term = K[i*2 + 0]*PHt_buf[j*2 + 0] + K[i*2 + 1]*PHt_buf[j*2 + 1];
            slam->P[i*dim + j] -= term;
        }
    }
    
    return EIF_STATUS_OK;
}

// --- UKF-SLAM Implementation ---

eif_status_t eif_ukf_slam_init(eif_ukf_slam_t* slam, int num_landmarks, eif_memory_pool_t* pool) {
    if (!slam || num_landmarks <= 0 || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    slam->num_landmarks = num_landmarks;
    slam->state_dim = 3 + 2 * num_landmarks;
    int n = slam->state_dim;
    
    // Parameters
    slam->alpha = 0.001f;
    slam->kappa = 0.0f;
    slam->beta = 2.0f;
    slam->lambda = slam->alpha * slam->alpha * (n + slam->kappa) - n;
    
    // Allocate State & Covariance
    slam->state = (float32_t*)eif_memory_alloc(pool, n * sizeof(float32_t), 4);
    slam->P = (float32_t*)eif_memory_alloc(pool, n * n * sizeof(float32_t), 4);
    
    // Allocate Sigma Points & Weights
    int num_sig = 2 * n + 1;
    slam->sigma_points = (float32_t*)eif_memory_alloc(pool, num_sig * n * sizeof(float32_t), 4);
    slam->wm = (float32_t*)eif_memory_alloc(pool, num_sig * sizeof(float32_t), 4);
    slam->wc = (float32_t*)eif_memory_alloc(pool, num_sig * sizeof(float32_t), 4);
    
    if (!slam->state || !slam->P || !slam->sigma_points || !slam->wm || !slam->wc) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    // Initialize State & P
    memset(slam->state, 0, n * sizeof(float32_t));
    memset(slam->P, 0, n * n * sizeof(float32_t));
    
    // Robot pose uncertainty
    slam->P[0*n+0] = 0.001f;
    slam->P[1*n+1] = 0.001f;
    slam->P[2*n+2] = 0.001f;
    
    // Landmarks uncertainty (large)
    for(int i=3; i<n; i++) slam->P[i*n+i] = 1000.0f;
    
    // Initialize Weights
    slam->wm[0] = slam->lambda / (n + slam->lambda);
    slam->wc[0] = slam->wm[0] + (1 - slam->alpha*slam->alpha + slam->beta);
    
    for (int i = 1; i < num_sig; i++) {
        slam->wm[i] = 1.0f / (2 * (n + slam->lambda));
        slam->wc[i] = slam->wm[i];
    }
    
    return EIF_STATUS_OK;
}

// Helper: Generate Sigma Points
static eif_status_t generate_sigma_points(eif_ukf_slam_t* slam, eif_memory_pool_t* pool) {
    int n = slam->state_dim;
    int num_sig = 2 * n + 1;
    
    float32_t* L = (float32_t*)eif_memory_alloc(pool, n * n * sizeof(float32_t), 4);
    if (!L) return EIF_STATUS_OUT_OF_MEMORY;
    
    // Cholesky
    if (eif_mat_cholesky_f32(slam->P, L, n) != EIF_STATUS_OK) {
        // Fallback or error?
        // If P is not positive definite, UKF fails.
        // Try adding epsilon to diagonal?
        for(int i=0; i<n; i++) slam->P[i*n+i] += 1e-6f;
        if (eif_mat_cholesky_f32(slam->P, L, n) != EIF_STATUS_OK) return EIF_STATUS_ERROR;
    }
    
    float32_t scale = sqrtf(n + slam->lambda);
    
    // Sigma 0: Mean
    for(int i=0; i<n; i++) slam->sigma_points[0*n + i] = slam->state[i];
    
    // Sigma 1..n: Mean + scale * L_col
    // Sigma n+1..2n: Mean - scale * L_col
    for(int i=0; i<n; i++) {
        for(int j=0; j<n; j++) {
            // L is lower triangular, so L[j*n+i] is row j, col i?
            // eif_mat_cholesky produces L such that L*L^T = P.
            // We need columns of L.
            float32_t val = L[j*n + i] * scale;
            slam->sigma_points[(i+1)*n + j] = slam->state[j] + val;
            slam->sigma_points[(i+1+n)*n + j] = slam->state[j] - val;
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_ukf_slam_predict(eif_ukf_slam_t* slam, float32_t v, float32_t w, float32_t dt, float32_t std_v, float32_t std_w, eif_memory_pool_t* pool) {
    if (!slam || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    int n = slam->state_dim;
    int num_sig = 2 * n + 1;
    
    // 1. Generate Sigma Points
    if (generate_sigma_points(slam, pool) != EIF_STATUS_OK) return EIF_STATUS_ERROR;
    
    // 2. Propagate Sigma Points through Motion Model
    for (int i = 0; i < num_sig; i++) {
        float32_t* sp = &slam->sigma_points[i * n];
        float32_t theta = sp[2];
        
        // Motion Update (only affects robot pose 0,1,2)
        // Add noise? UKF usually augments state with noise or adds Q at the end.
        // Standard UKF: x_k+1 = f(x_k, u_k)
        // P_k+1 = ... + Q
        
        sp[0] += v * cosf(theta) * dt;
        sp[1] += v * sinf(theta) * dt;
        sp[2] += w * dt;
        sp[2] = normalize_angle(sp[2]);
    }
    
    // 3. Calculate Predicted Mean
    memset(slam->state, 0, n * sizeof(float32_t));
    
    // Handle angle mean correctly (vector sum)
    float32_t sum_sin = 0, sum_cos = 0;
    
    for (int i = 0; i < num_sig; i++) {
        float32_t weight = slam->wm[i];
        for (int j = 0; j < n; j++) {
            if (j == 2) { // Theta
                sum_sin += weight * sinf(slam->sigma_points[i*n + j]);
                sum_cos += weight * cosf(slam->sigma_points[i*n + j]);
            } else {
                slam->state[j] += weight * slam->sigma_points[i*n + j];
            }
        }
    }
    slam->state[2] = atan2f(sum_sin, sum_cos);
    
    // 4. Calculate Predicted Covariance
    memset(slam->P, 0, n * n * sizeof(float32_t));
    for (int i = 0; i < num_sig; i++) {
        float32_t weight = slam->wc[i];
        
        // Diff = sp - state
        float32_t* diff = (float32_t*)eif_memory_alloc(pool, n * sizeof(float32_t), 4);
        for(int j=0; j<n; j++) diff[j] = slam->sigma_points[i*n + j] - slam->state[j];
        diff[2] = normalize_angle(diff[2]);
        
        // P += weight * diff * diff^T
        for(int r=0; r<n; r++) {
            for(int c=0; c<n; c++) {
                slam->P[r*n + c] += weight * diff[r] * diff[c];
            }
        }
    }
    
    // Add Process Noise Q (Only to robot pose)
    slam->P[0*n+0] += std_v * std_v * dt * dt;
    slam->P[1*n+1] += std_v * std_v * dt * dt;
    slam->P[2*n+2] += std_w * std_w * dt * dt;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_ukf_slam_update(eif_ukf_slam_t* slam, int landmark_id, float32_t range, float32_t bearing, float32_t std_range, float32_t std_bearing, eif_memory_pool_t* pool) {
    if (!slam || landmark_id < 0 || landmark_id >= slam->num_landmarks || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    int n = slam->state_dim;
    int num_sig = 2 * n + 1;
    int l_idx = 3 + 2 * landmark_id;
    
    // Initialize landmark if needed
    if (slam->P[l_idx * n + l_idx] > 100.0f) {
        float32_t rx = slam->state[0];
        float32_t ry = slam->state[1];
        float32_t rth = slam->state[2];
        slam->state[l_idx] = rx + range * cosf(rth + bearing);
        slam->state[l_idx+1] = ry + range * sinf(rth + bearing);
        // Reset covariance for this landmark?
        // In UKF, we just set the state. The covariance will converge.
        // But we should probably reduce the variance from 1000 to something reasonable based on measurement noise.
        // For now, leave it.
    }
    
    // 1. Generate Sigma Points (Redraw from predicted state/cov)
    if (generate_sigma_points(slam, pool) != EIF_STATUS_OK) return EIF_STATUS_ERROR;
    
    // 2. Predict Measurements for each sigma point
    // Z_sigma (2 x num_sig)
    float32_t* Z_sig = (float32_t*)eif_memory_alloc(pool, 2 * num_sig * sizeof(float32_t), 4);
    if (!Z_sig) return EIF_STATUS_OUT_OF_MEMORY;
    
    float32_t z_mean[2] = {0, 0};
    float32_t sum_sin_b = 0, sum_cos_b = 0;
    
    for (int i = 0; i < num_sig; i++) {
        float32_t* sp = &slam->sigma_points[i * n];
        float32_t rx = sp[0];
        float32_t ry = sp[1];
        float32_t rth = sp[2];
        float32_t lx = sp[l_idx];
        float32_t ly = sp[l_idx+1];
        
        float32_t dx = lx - rx;
        float32_t dy = ly - ry;
        float32_t dist = sqrtf(dx*dx + dy*dy);
        float32_t angle = normalize_angle(atan2f(dy, dx) - rth);
        
        Z_sig[i*2 + 0] = dist;
        Z_sig[i*2 + 1] = angle;
        
        z_mean[0] += slam->wm[i] * dist;
        
        sum_sin_b += slam->wm[i] * sinf(angle);
        sum_cos_b += slam->wm[i] * cosf(angle);
    }
    z_mean[1] = atan2f(sum_sin_b, sum_cos_b);
    
    // 3. Calculate Innovation Covariance S and Cross Covariance Pxz
    float32_t S[2][2] = {{0}};
    size_t pxz_size = (size_t)n * 2 * sizeof(float32_t);
    float32_t* Pxz = (float32_t*)eif_memory_alloc(pool, pxz_size, 4);
    if (!Pxz) return EIF_STATUS_OUT_OF_MEMORY;
    memset(Pxz, 0, pxz_size);
    
    for (int i = 0; i < num_sig; i++) {
        float32_t weight = slam->wc[i];
        
        float32_t z_diff[2];
        z_diff[0] = Z_sig[i*2 + 0] - z_mean[0];
        z_diff[1] = normalize_angle(Z_sig[i*2 + 1] - z_mean[1]);
        
        float32_t* x_diff = (float32_t*)eif_memory_alloc(pool, n * sizeof(float32_t), 4);
        for(int j=0; j<n; j++) x_diff[j] = slam->sigma_points[i*n + j] - slam->state[j];
        x_diff[2] = normalize_angle(x_diff[2]);
        
        // S += w * z_diff * z_diff^T
        S[0][0] += weight * z_diff[0] * z_diff[0];
        S[0][1] += weight * z_diff[0] * z_diff[1];
        S[1][0] += weight * z_diff[1] * z_diff[0];
        S[1][1] += weight * z_diff[1] * z_diff[1];
        
        // Pxz += w * x_diff * z_diff^T
        for(int j=0; j<n; j++) {
            Pxz[j*2 + 0] += weight * x_diff[j] * z_diff[0];
            Pxz[j*2 + 1] += weight * x_diff[j] * z_diff[1];
        }
    }
    
    // Add Measurement Noise R to S
    S[0][0] += std_range * std_range;
    S[1][1] += std_bearing * std_bearing;
    
    // 4. Kalman Gain K = Pxz * S^-1
    // Invert S
    float32_t det = S[0][0]*S[1][1] - S[0][1]*S[1][0];
    if (fabs(det) < 1e-6f) return EIF_STATUS_ERROR;
    float32_t invDet = 1.0f / det;
    float32_t Sinv[2][2];
    Sinv[0][0] = S[1][1] * invDet;
    Sinv[0][1] = -S[0][1] * invDet;
    Sinv[1][0] = -S[1][0] * invDet;
    Sinv[1][1] = S[0][0] * invDet;
    
    float32_t* K = (float32_t*)eif_memory_alloc(pool, n * 2 * sizeof(float32_t), 4);
    if (!K) return EIF_STATUS_OUT_OF_MEMORY;
    
    for(int i=0; i<n; i++) {
        K[i*2 + 0] = Pxz[i*2 + 0]*Sinv[0][0] + Pxz[i*2 + 1]*Sinv[1][0];
        K[i*2 + 1] = Pxz[i*2 + 0]*Sinv[0][1] + Pxz[i*2 + 1]*Sinv[1][1];
    }
    
    // 5. Update State and Covariance
    float32_t y[2];
    y[0] = range - z_mean[0];
    y[1] = normalize_angle(bearing - z_mean[1]);
    
    // x = x + K*y
    for(int i=0; i<n; i++) {
        slam->state[i] += K[i*2 + 0]*y[0] + K[i*2 + 1]*y[1];
    }
    slam->state[2] = normalize_angle(slam->state[2]);
    
    // P = P - K * S * K^T
    // P = P - K * (S * K^T)
    // Let Temp = S * K^T (2 x n)
    // Temp[0, j] = S00*K[j,0] + S01*K[j,1]
    // Temp[1, j] = S10*K[j,0] + S11*K[j,1]
    
    for(int i=0; i<n; i++) {
        for(int j=0; j<n; j++) {
            // Calculate (K * S * K^T)[i, j]
            // = sum_k (K[i, k] * (S*K^T)[k, j])
            // = K[i,0]*Temp[0,j] + K[i,1]*Temp[1,j]
            
            float32_t temp0 = S[0][0]*K[j*2+0] + S[0][1]*K[j*2+1];
            float32_t temp1 = S[1][0]*K[j*2+0] + S[1][1]*K[j*2+1];
            
            float32_t val = K[i*2+0]*temp0 + K[i*2+1]*temp1;
            slam->P[i*n + j] -= val;
        }
    }
    
    return EIF_STATUS_OK;
}
