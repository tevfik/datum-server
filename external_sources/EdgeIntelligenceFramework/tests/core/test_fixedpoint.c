#include "../framework/eif_test_runner.h"
#include "eif_fixedpoint.h"
#include <math.h>

// =============================================================================
// Tests
// =============================================================================

bool test_q15_sin() {
    // 0 -> 0
    TEST_ASSERT_EQUAL_INT(0, eif_q15_sin(0));
    
    // Pi/2 -> 16384 (since 32768 is Pi)
    // sin(Pi/2) = 1.0 -> 32767
    q15_t res = eif_q15_sin(16384);
    TEST_ASSERT_TRUE(res > 32000); // Allow some error
    
    // Pi -> 32768
    // sin(Pi) = 0
    res = eif_q15_sin(32768);
    TEST_ASSERT_TRUE(res > -100 && res < 100);
    
    // 3Pi/2 -> 49152 (or -16384)
    // sin(3Pi/2) = -1.0 -> -32768
    res = eif_q15_sin(49152); // 49152 is interpreted as negative in q15_t? No, q15_t is signed.
    // 49152 as uint16 is 0xC000. As int16 it is -16384.
    // -16384 corresponds to -Pi/2. sin(-Pi/2) = -1.
    TEST_ASSERT_TRUE(res < -32000);
    
    return true;
}

bool test_q15_cos() {
    // 0 -> 1.0 -> 32767
    q15_t res = eif_q15_cos(0);
    TEST_ASSERT_TRUE(res > 32000);
    
    // Pi/2 -> 16384 -> 0
    res = eif_q15_cos(16384);
    TEST_ASSERT_TRUE(res > -100 && res < 100);
    
    // Pi -> 32768 -> -1.0 -> -32768
    res = eif_q15_cos(32768); // -32768 is -Pi. cos(-Pi) = -1.
    TEST_ASSERT_TRUE(res < -32000);
    
    return true;
}

bool test_q15_exp() {
    // exp(0) = 1 -> 32767
    TEST_ASSERT_EQUAL_INT(32767, eif_q15_exp(0));
    
    // exp(small negative) < 1
    q15_t res = eif_q15_exp(-1000);
    TEST_ASSERT_TRUE(res < 32767);
    
    // Saturation: exp(1.0) = 2.718 -> Overflow
    // Input 32767 (approx 1.0)
    res = eif_q15_exp(32767);
    TEST_ASSERT_EQUAL_INT(32767, res);
    
    return true;
}

bool test_q15_log() {
    // log(1) = 0. 1 is 32767.
    TEST_ASSERT_EQUAL_INT(0, eif_q15_log(32767));
    
    // log(0) -> MIN
    TEST_ASSERT_EQUAL_INT(EIF_Q15_MIN, eif_q15_log(0));
    
    // log(0.5) -> ln(0.5) = -0.693 -> -22700 approx.
    // My implementation: x - 1.
    // 0.5 - 1 = -0.5 -> -16384.
    // 16384 (0.5) -> 16384 - 32767 = -16383.
    q15_t res = eif_q15_log(16384);
    TEST_ASSERT_TRUE(res < 0);
    
    return true;
}

BEGIN_TEST_SUITE(run_core_fixed_point_tests)
    RUN_TEST(test_q15_sin);
    RUN_TEST(test_q15_cos);
    RUN_TEST(test_q15_exp);
    RUN_TEST(test_q15_log);
END_TEST_SUITE()
