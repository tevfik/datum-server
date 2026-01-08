#include "../framework/eif_test_runner.h"
#include "eif_ts_hw_fixed.h"

#define POOL_SIZE 4096
static uint8_t pool_buffer[POOL_SIZE];
static eif_memory_pool_t pool;

static void setup_pool(void) {
    eif_memory_pool_init(&pool, pool_buffer, POOL_SIZE);
}

bool test_hw_fixed_linear(void) {
    setup_pool();
    eif_ts_hw_fixed_t hw;
    
    // Season length 4
    eif_status_t status = eif_ts_hw_init_fixed(&hw, 4, EIF_TS_HW_ADDITIVE, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Train on linear data: 0, 100, 200, ... (Scaled to Q15)
    // 1.0 -> 1000 in Q15.
    
    q15_t val = 0;
    for (int i=0; i<20; i++) {
        status = eif_ts_hw_update_fixed(&hw, val);
        TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
        val += 500; // Increment
    }
    
    // Predict next
    // Last val was 19*500 = 9500.
    // Next should roughly be 10000.
    q15_t pred = eif_ts_hw_predict_fixed(&hw, 1);
    
    // Allow error margin due to lag
    int diff = abs(pred - 10000);
    // printf("Pred: %d, Expected: 10000, Diff: %d\n", pred, diff);
    
    if (diff > 500) {
        printf("Failure: HW Pred %d far from 10000\n", pred);
        return false;
    }
    
    return true;
}

int run_ts_hw_fixed_tests(void) {
    int failed = 0;
    if (!test_hw_fixed_linear()) { printf("test_hw_fixed_linear FAILED\n"); failed++; }
    return failed;
}
