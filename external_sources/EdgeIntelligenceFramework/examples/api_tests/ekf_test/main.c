#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "eif_bayesian.h"

// Test Scenario: Radar Tracking (Range only)
// State: x = [position, velocity]
// System: x_k = [1 dt; 0 1] * x_{k-1} + noise
// Measurement: z_k = sqrt(pos^2 + alt^2) (Non-linear)
// For simplicity, let's do 1D non-linear measurement: z = x^2 / 10

void f_func(const float32_t* x, const float32_t* u, float32_t* out_x) {
    // Linear dynamics: x_k = x_{k-1} + v_{k-1}*dt
    // State: [pos, vel]
    float32_t dt = 0.1f;
    out_x[0] = x[0] + x[1] * dt;
    out_x[1] = x[1];
}

void F_jac(const float32_t* x, const float32_t* u, float32_t* out_F) {
    // Jacobian of f
    float32_t dt = 0.1f;
    out_F[0] = 1.0f; out_F[1] = dt;
    out_F[2] = 0.0f; out_F[3] = 1.0f;
}

void h_func(const float32_t* x, float32_t* out_z) {
    // Non-linear measurement: z = x[0]^2 / 10
    out_z[0] = (x[0] * x[0]) / 10.0f;
}

void H_jac(const float32_t* x, float32_t* out_H) {
    // Jacobian of h: dz/dx = [2*x[0]/10, 0]
    out_H[0] = 0.2f * x[0];
    out_H[1] = 0.0f;
}

#include "eif_memory.h"

// ... (f_func, F_jac, h_func, H_jac same as before) ...

void test_ekf() {
    printf("Testing EKF...\n");
    
    int n = 2;
    int p = 1;
    
    float32_t x[] = {2.0f, 1.0f}; // Initial: Pos=2, Vel=1
    float32_t P[] = {0.1f, 0.0f, 0.0f, 0.1f};
    float32_t Q[] = {0.01f, 0.0f, 0.0f, 0.01f};
    float32_t R[] = {0.1f};
    
    eif_ekf_t ekf = {
        .n = n, .p = p,
        .x = x, .P = P, .Q = Q, .R = R,
        .f_func = f_func, .F_jac = F_jac,
        .h_func = h_func, .H_jac = H_jac
    };
    
    uint8_t pool_buffer[4096];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_ekf_init(&ekf, &pool);
    
    // Simulate
    for (int i = 0; i < 5; i++) {
        // Predict
        eif_ekf_predict(&ekf, NULL);
        
        // True state approx: Pos increases by 0.1 each step (Vel=1, dt=0.1)
        // Step 1: Pos=2.1. Meas = 2.1^2 / 10 = 0.441
        float32_t true_pos = 2.0f + (i+1)*0.1f;
        float32_t meas_val = (true_pos * true_pos) / 10.0f;
        float32_t z[] = {meas_val};
        
        eif_ekf_update(&ekf, z);
        
        printf("Step %d: Pos=%.4f, Vel=%.4f (True Pos=%.4f)\n", 
               i+1, ekf.x[0], ekf.x[1], true_pos);
    }
}

void test_ukf() {
    printf("Testing UKF...\n");
    
    int n = 2;
    int p = 1;
    
    float32_t x[] = {2.0f, 1.0f}; 
    float32_t P[] = {0.1f, 0.0f, 0.0f, 0.1f};
    float32_t Q[] = {0.01f, 0.0f, 0.0f, 0.01f};
    float32_t R[] = {0.1f};
    
    eif_ukf_t ukf = {
        .n = n, .p = p,
        .x = x, .P = P, .Q = Q, .R = R,
        .f_func = f_func, // Same dynamics
        .h_func = h_func, // Same measurement
        .alpha = 1e-3f, .beta = 2.0f, .kappa = 0.0f
    };
    
    uint8_t pool_buffer[4096];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_ukf_init(&ukf, &pool);
    
    for (int i = 0; i < 5; i++) {
        eif_ukf_predict(&ukf, NULL);
        
        float32_t true_pos = 2.0f + (i+1)*0.1f;
        float32_t meas_val = (true_pos * true_pos) / 10.0f;
        float32_t z[] = {meas_val};
        
        eif_ukf_update(&ukf, z);
        
        printf("Step %d: Pos=%.4f, Vel=%.4f (True Pos=%.4f)\n", 
               i+1, ukf.x[0], ukf.x[1], true_pos);
    }
    
    // Cleanup
    // No free needed
}

int main() {
    test_ekf();
    test_ukf();
    return 0;
}
