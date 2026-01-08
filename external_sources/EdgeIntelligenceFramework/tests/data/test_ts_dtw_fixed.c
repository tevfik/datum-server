#include "../framework/eif_test_runner.h"
#include "eif_ts_dtw_fixed.h"
#include <stdlib.h>

// Helper: Convert float array to Q15
static void to_q15(const float *src, q15_t *dst, int len) {
    for (int i = 0; i < len; i++) {
        dst[i] = EIF_FLOAT_TO_Q15(src[i]);
    }
}

// Basic exact match test
bool test_dtw_fixed_exact(void) {
    float s1_f[] = {0.1f, 0.2f, 0.3f, 0.4f};
    q15_t s1[4];
    to_q15(s1_f, s1, 4);

    q31_t dist = eif_ts_dtw_compute_fixed(s1, 4, s1, 4, 0);
    
    // Identical sequences should have 0 distance
    TEST_ASSERT_EQUAL_INT(0, dist);
    return true;
}

// Simple warp test
bool test_dtw_fixed_warp(void) {
    // S1: 1, 1, 2, 3
    // S2: 1, 2, 2, 3
    // Shift in time
    float s1_f[] = {0.1f, 0.1f, 0.2f, 0.3f};
    float s2_f[] = {0.1f, 0.2f, 0.2f, 0.3f};
    
    q15_t s1[4], s2[4];
    to_q15(s1_f, s1, 4);
    to_q15(s2_f, s2, 4);

    q31_t dist = eif_ts_dtw_compute_fixed(s1, 4, s2, 4, 0);
    
    // Expected path:
    // (1,1) -> diff 0
    // (2,2) -> diff |0.1-0.2|=0.1  (Or (2,1) match?)
    // Let's manually trace optimal:
    //   1(0.1) 2(0.1) 3(0.2) 4(0.3)
    // 1(0.1) M(0)   
    // 2(0.2)        M(0.1) 
    // 3(0.2)               M(0)
    // 4(0.3)                      M(0)
    
    // Path: (1,1)->(2,2)->(3,3)->(4,4) costs 0 + 0.1 + 0 + 0 = 0.1
    // Is there cheaper?
    // (1,1) -> 0.
    // (2,1) -> |0.1-0.1| = 0.  (s1[1] vs s2[0])
    // (3,2) -> |0.2-0.2| = 0.
    // (3,3) -> |0.2-0.2| = 0.
    // (4,4) -> |0.3-0.3| = 0.
    
    // Wait, (1,1)->(2,1)->(3,2)->(4,4) ?
    // Indices:
    // i=1,j=1: |0.1-0.1|=0
    // i=2,j=1: |0.1-0.1|=0
    // i=3,j=2: |0.2-0.2|=0
    // i=3,j=3: |0.2-0.2|=0  (Wait, can't start 3 again)
    // Path must be monotonic.
    // (1,1) -> (2,1) -> (3,2) -> (3,3) -> (4,4)
    // Costs: 0 + 0 + 0 + 0 + 0 = 0?
    // Lengths:
    // s1: 0.1, 0.1, 0.2, 0.3
    // s2: 0.1, 0.2, 0.2, 0.3
    
    // Path:
    // s1[0] vs s2[0]: 0
    // s1[1] vs s2[0]: 0
    // s1[2] vs s2[1]: 0
    // s1[2] vs s2[2]: 0
    // s1[3] vs s2[3]: 0
    // Total 0.
    
    // Let's verify if my manual trace is correct.
    // The algorithm finds minimum. 0 is possible.
    
    TEST_ASSERT_EQUAL_INT(0, dist);
    
    return true;
}

// Verify different sequences
bool test_dtw_fixed_diff(void) {
    // S1: 0, 0, 0
    // S2: 0.1, 0.1, 0.1
    // Dist should be sum |0-0.1| * 3 = 0.3
    
    float s1_f[] = {0.0f, 0.0f, 0.0f};
    float s2_f[] = {0.1f, 0.1f, 0.1f};
    
    q15_t s1[3], s2[3];
    to_q15(s1_f, s1, 3);
    to_q15(s2_f, s2, 3);
    
    q31_t dist = eif_ts_dtw_compute_fixed(s1, 3, s2, 3, 0);
    
    // 0.1 in Q15 is roughly 3276.
    // 3 * 3276 = 9828.
    q31_t expected = EIF_FLOAT_TO_Q15(0.3f);
    
    TEST_ASSERT_TRUE(abs(dist - expected) < 20); // Tolerance for rounding
    return true;
}

int run_ts_dtw_fixed_tests(void) {
    int failed = 0;
    if (!test_dtw_fixed_exact()) { printf("test_dtw_fixed_exact FAILED\n"); failed++; }
    if (!test_dtw_fixed_warp()) { printf("test_dtw_fixed_warp FAILED\n"); failed++; }
    if (!test_dtw_fixed_diff()) { printf("test_dtw_fixed_diff FAILED\n"); failed++; }
    return failed;
}
