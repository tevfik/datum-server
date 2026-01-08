#include "../framework/eif_test_runner.h"
#include "eif_async.h"
#include "eif_assert.h"

// Dummy definition for test
struct eif_model_s {
    int dummy;
};

// =============================================================================
// Tests
// =============================================================================

bool test_async_timeout() {
    eif_async_handle_t handle;
    eif_async_init(&handle);
    
    // Manually set to running to simulate a long operation
    handle.state = EIF_ASYNC_RUNNING;
    handle.start_time_ms = 0; // Assuming fake time starts at 0 or close
    
    // Wait with timeout
    // In generic mode, eif_get_time_ms increments on every call.
    // So it should timeout quickly.
    eif_status_t status = eif_dma_wait(&handle, 5);
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_TIMEOUT, status);
    TEST_ASSERT_EQUAL_INT(EIF_ASYNC_ERROR, handle.state);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_TIMEOUT, handle.result);
    
    return true;
}

bool test_async_inference() {
    // Mock model (pointer only)
    struct eif_model_s model;
    int input = 1;
    int output = 0;
    eif_async_handle_t handle;
    
    eif_status_t status = eif_inference_async_start(&model, &input, &output, &handle);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_TRUE(eif_async_done(&handle));
    
    status = eif_inference_async_wait(&handle, 100);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    return true;
}

bool test_dma_available() {
    // Generic mode: returns false
    bool avail = eif_dma_available();
    TEST_ASSERT_TRUE(!avail);
    return true;
}

BEGIN_TEST_SUITE(run_async_coverage_tests)
    RUN_TEST(test_async_timeout);
    RUN_TEST(test_async_inference);
    RUN_TEST(test_dma_available);
END_TEST_SUITE()
