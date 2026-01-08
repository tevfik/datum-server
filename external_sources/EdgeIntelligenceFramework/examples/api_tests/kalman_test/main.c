#include <stdio.h>
#include <stdlib.h>
#include "eif_bayesian.h"

// Test Scenario: 1D Voltage Tracking
// True Voltage = 1.25V (Constant)
// Measurement Noise = 0.1V
// Process Noise = 1e-5 (Very small, since constant)

#include "eif_memory.h"

void test_kalman_1d() {
    printf("Testing Kalman Filter (1D)...\n");
    
    // 1. Setup Matrices
    int n = 1; // State dim
    int m = 0; // Control dim
    int p = 1; // Measurement dim
    
    float32_t x[] = {0.0f}; // Initial estimate (wrong)
    float32_t P[] = {1.0f}; // High initial uncertainty
    float32_t F[] = {1.0f}; // Constant model: x_k = x_{k-1}
    float32_t H[] = {1.0f}; // Direct measurement: z_k = x_k
    float32_t Q[] = {1e-5f}; // Low process noise
    float32_t R[] = {0.01f}; // Measurement noise variance (0.1^2)
    
    eif_kalman_filter_t kf = {
        .n = n, .m = m, .p = p,
        .x = x, .P = P, .F = F, .H = H, .Q = Q, .R = R,
        .B = NULL
    };
    
    uint8_t pool_buffer[4096];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_kalman_init(&kf, &pool);
    
    // 2. Simulation Loop
    float32_t true_voltage = 1.25f;
    float32_t measurements[] = {1.15f, 1.30f, 1.22f, 1.28f, 1.19f}; // Noisy measurements
    
    for (int i = 0; i < 5; i++) {
        // Predict
        eif_kalman_predict(&kf, NULL);
        
        // Update
        float32_t z[] = {measurements[i]};
        eif_kalman_update(&kf, z);
        
        printf("Step %d: Meas=%.2f, Est=%.4f, Var=%.4f\n", 
               i+1, z[0], kf.x[0], kf.P[0]);
    }
    
    // Check convergence
    if (kf.x[0] > 1.20f && kf.x[0] < 1.30f) {
        printf("SUCCESS: Filter converged to approx 1.25V\n");
    } else {
        printf("FAILURE: Filter did not converge\n");
    }
    
    // Cleanup (kf.K was malloced)
    // No free needed with pool
}

int main() {
    test_kalman_1d();
    return 0;
}
