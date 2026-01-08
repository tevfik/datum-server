#include <stdio.h>
#include <stdlib.h>
#include "eif_bayesian_fixed.h"

// Simple 1D Voltage Tracking in Q15
// x = [voltage]
// F = [1]
// H = [1]
// Q = [0.0001]
// R = [0.1]
// Initial x = [0]
// True voltage = 0.5 (approx 16384 in Q15)

#include "eif_memory.h"

void test_kalman_q15() {
    printf("Testing Q15 Kalman Filter (MCU Optimized)...\n");
    
    // Memory Pool Init
    uint8_t buffer[4096]; // 4KB buffer
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, buffer, sizeof(buffer));
    
    int n = 1;
    int p = 1;
    
    q15_t x[] = {0};
    q15_t P[] = {EIF_FLOAT_TO_Q15(0.5f)}; // 0.5 to avoid S overflow
    q15_t F[] = {EIF_Q15_MAX}; // 1.0
    q15_t H[] = {EIF_Q15_MAX}; // 1.0
    q15_t Q[] = {EIF_FLOAT_TO_Q15(0.001f)}; // Small process noise
    q15_t R[] = {EIF_FLOAT_TO_Q15(0.1f)};   // Measurement noise
    
    eif_kalman_filter_q15_t kf = {
        .n = n, .m = 0, .p = p,
        .x = x, .P = P, .F = F, .B = NULL, .H = H,
        .Q = Q, .R = R
    };
    
    // Init (allocates K from pool)
    eif_kalman_init_q15(&kf, &pool);
    
    // True voltage 0.5V
    q15_t true_val = EIF_FLOAT_TO_Q15(0.5f);
    
    for (int i = 0; i < 10; i++) {
        // Reset pool for temp buffers (keep K safe? No, K is allocated once in init)
        // Wait, eif_memory_reset clears EVERYTHING.
        // If K is allocated from pool in init, reset will wipe it.
        // We need a separate pool for temp buffers OR a way to save state.
        // Standard pattern: 
        // 1. Persistent Pool (for Init)
        // 2. Temp Pool (for Predict/Update)
        // OR: Alloc K from heap/static, use pool only for temp.
        // But our init takes pool.
        
        // Let's use a "mark/release" or just not reset the whole pool if we have enough space.
        // Or better: Use a separate temp buffer for the loop.
        
        // For this test, let's just use the same pool and NOT reset it, assuming 4KB is enough for 10 iterations.
        // Actually, 10 iterations * (lots of temp allocs) might overflow 4KB.
        // We should implement a "checkpoint" in memory pool or use two pools.
        // Let's use a simple "checkpoint" by saving `used` offset? 
        // `eif_memory_pool_t` struct is exposed.
        
        size_t checkpoint = pool.used;
        
        eif_kalman_predict_q15(&kf, NULL, &pool);
        
        // Measurement = true_val
        q15_t z[] = {true_val};
        eif_kalman_update_q15(&kf, z, &pool);
        
        printf("Step %d: Est=%.4f (True=%.4f)\n", 
               i+1, EIF_Q15_TO_FLOAT(kf.x[0]), EIF_Q15_TO_FLOAT(true_val));
               
        // Restore pool to checkpoint (free temp memory, keep K)
        pool.used = checkpoint;
    }
    
    // No free needed
}

int main() {
    test_kalman_q15();
    return 0;
}
