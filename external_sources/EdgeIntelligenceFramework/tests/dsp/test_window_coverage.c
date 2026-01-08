#include "../framework/eif_test_runner.h"
#include "eif_dsp.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

bool test_eif_dsp_window_hamming_f32(void) {
    size_t length = 10;
    float32_t window[10];
    
    eif_dsp_window_hamming_f32(window, length);
    
    // Check first and last elements (should be 0.08)
    // 0.54 - 0.46 * cos(0) = 0.54 - 0.46 = 0.08
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.08f, window[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.08f, window[length-1]);
    
    // Check range
    for(size_t i=0; i<length; i++) {
        TEST_ASSERT(window[i] >= 0.0f);
        TEST_ASSERT(window[i] <= 1.0f);
    }
    
    return true;
}

bool test_eif_dsp_window_hanning_f32(void) {
    size_t length = 10;
    float32_t window[10];
    
    eif_dsp_window_hanning_f32(window, length);
    
    // Check first and last elements (should be 0)
    // 0.5 * (1 - cos(0)) = 0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, window[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, window[length-1]);
    
    // Check range
    for(size_t i=0; i<length; i++) {
        TEST_ASSERT(window[i] >= 0.0f);
        TEST_ASSERT(window[i] <= 1.0f);
    }
    
    return true;
}

BEGIN_TEST_SUITE(run_window_coverage_tests)
    RUN_TEST(test_eif_dsp_window_hamming_f32);
    RUN_TEST(test_eif_dsp_window_hanning_f32);
END_TEST_SUITE()
