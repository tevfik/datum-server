#include "../framework/eif_test_runner.h"
#include "eif_matrix_profile_fixed.h"
#include <stdlib.h>
#include <string.h>

#define POOL_SIZE 40960
static uint8_t pool_buffer[POOL_SIZE];
static eif_memory_pool_t pool;

static void setup_pool(void) {
    eif_memory_pool_init(&pool, pool_buffer, POOL_SIZE);
}

// Test 1: Identical constant signal (Distance should be 0)
bool test_mp_fixed_constant(void) {
    setup_pool();
    int len = 20;
    int win = 4;
    q15_t ts[20];
    for(int i=0; i<len; i++) ts[i] = 1000; // Constant
    
    eif_matrix_profile_fixed_t mp;
    eif_status_t status = eif_mp_compute_fixed(ts, len, win, &mp, &pool);
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // For constant signal, correlation is undefined (std=0).
    // Our code sets std=1 if 0.
    // Mean=1000. Std=1.
    // Normalized constant signal is 0,0,0...
    // All subsequences match perfectly. Dist should be 0.
    
    for(int i=0; i<mp.profile_length; i++) {
        // We expect distance 0.
        // But exclusion zone might force it to look far away.
        // It's all identical, so any match is distance 0.
        if (mp.profile[i] > 10) { // Tolerance
            printf("Profile[%d] = %d\n", i, mp.profile[i]);
            return false;
        }
    }
    return true;
}

// Test 2: Sine Wave with repeating pattern
bool test_mp_fixed_sine(void) {
    setup_pool();
    int len = 64;
    int win = 16;
    q15_t ts[64];
    
    // Generate 2 cycles of sine
    // Period 32.
    // 0..31: Cycle 1
    // 32..63: Cycle 2
    for(int i=0; i<len; i++) {
        // sin(2*pi*i/32)
        // using simple float generation then convert to q15
        float v = sinf(2.0f * 3.14159f * i / 32.0f);
        ts[i] = (q15_t)(v * 10000.0f); // Scale to Q15 range roughly 1/3
    }
    
    eif_matrix_profile_fixed_t mp;
    eif_status_t status = eif_mp_compute_fixed(ts, len, win, &mp, &pool);
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Inspect profile
    // Index 0 (Start of C1) should match Index 32 (Start of C2).
    // Distance should be very low.
    
    // printf("MP[0] dist: %d, idx: %d\n", mp.profile[0], mp.profile_index[0]);
    
    // Allow some quantization error. 10000 scale means Q15 precision is good.
    // Distance calculation involves many steps. 
    // Tolerance: sqrt(2 * 16 * (1 - 0.99)) = sqrt(32 * 0.01) = sqrt(0.32) approx 0?
    // In our units: sqrt(2*16 * (32768 - 32000)) approx.
    // Let's expect < 500.
    
    if (mp.profile[0] > 1000) {
        printf("Failure: MP[0] distance %d too high (>1000)\n", mp.profile[0]);
        return false;
    }
    
    // Ideally index should be around 32
    if (abs(mp.profile_index[0] - 32) > 2) {
         printf("Failure: MP[0] matched index %d, expected around 32\n", mp.profile_index[0]);
         return false;
    }

    return true;
}

int run_mp_fixed_tests(void) {
    int failed = 0;
    if (!test_mp_fixed_constant()) { printf("test_mp_fixed_constant FAILED\n"); failed++; }
    if (!test_mp_fixed_sine()) { printf("test_mp_fixed_sine FAILED\n"); failed++; }
    return failed;
}
