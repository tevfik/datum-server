#include "eif_test_runner.h"
#include "eif_ts.h"
#include <math.h>

static uint8_t pool_buffer[1024 * 1024];
static eif_memory_pool_t pool;

bool test_arima_simple(void) {
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_ts_arima_t arima;
    // AR(1), d=0, q=0
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_ts_arima_init(&arima, 1, 0, 0, &pool));
    
    // Set coeff manually: y(t) = 0.5 * y(t-1)
    arima.ar_coeffs[0] = 0.5f;
    
    float32_t pred;
    
    // t=0, input=10. Predict t=1.
    eif_ts_arima_predict(&arima, 10.0f, &pred);
    // pred = 0.5 * 10 = 5.0
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, pred);
    
    // t=1, input=5. Predict t=2.
    eif_ts_arima_predict(&arima, 5.0f, &pred);
    // pred = 0.5 * 5 = 2.5
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.5f, pred);
    
    return true;
}

bool test_arima_full(void) {
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_ts_arima_t arima;
    // AR(1), d=1, MA(1)
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_ts_arima_init(&arima, 1, 1, 1, &pool));
    
    // Set coeffs
    arima.ar_coeffs[0] = 0.5f;
    arima.ma_coeffs[0] = 0.2f;
    
    // Initial state
    // d=1 means we work with diffs.
    // t=0: input=10. diff_history[0] becomes 10.
    // t=1: input=12. diff = 12-10=2. 
    //      Update AR history: history[0]=2.
    //      Update MA error: error=0 (simplified).
    //      Predict next diff: 0.5*2 + 0.2*0 = 1.0.
    //      Predict next val: 12 + 1.0 = 13.0.
    
    float32_t pred;
    
    // t=0
    eif_ts_arima_predict(&arima, 10.0f, &pred);
    
    // t=1
    eif_ts_arima_predict(&arima, 12.0f, &pred);
    
    // Check prediction for t=2
    // diff(t=1) = 12 - 10 = 2.0
    // pred_diff(t=2) = 0.5 * 2.0 + 0 = 1.0
    // pred_val(t=2) = input(t=1) + pred_diff = 12.0 + 1.0 = 13.0
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 13.0f, pred);
    
    return true;
}

bool test_arima_fit(void) {
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    eif_ts_arima_t arima;
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_ts_arima_init(&arima, 1, 0, 0, &pool));
    
    // Generate AR(1) process: y(t) = 0.5 * y(t-1)
    float32_t data[100];
    data[0] = 1.0f;
    for(int i=1; i<100; i++) {
        data[i] = 0.5f * data[i-1];
    }
    
    // Fit
    eif_ts_arima_fit(&arima, data, 100);
    
    // Check coeff (should be close to 0.5)
    // Note: Yule-Walker on short/clean data might be slightly off but close.
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.5f, arima.ar_coeffs[0]);
    
    return true;
}

bool test_hw_seasonal(void) {
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_ts_hw_t hw;
    // Season length 4
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_ts_hw_init(&hw, 4, EIF_TS_HW_ADDITIVE, &pool));
    
    // Feed periodic data: 10, 20, 10, 20...
    // Seasonality is +0, +10, +0, +10 relative to base 10?
    // Or Level 15, Season -5, +5, -5, +5.
    
    for (int i = 0; i < 100; i++) {
        float32_t val = (i % 2 == 0) ? 10.0f : 20.0f;
        eif_ts_hw_update(&hw, val);
    }
    
    // Forecast next 2 steps
    float32_t forecast[2];
    eif_ts_hw_forecast(&hw, 2, forecast);
    
    // Next step (i=20) should be 10.
    // Step after (i=21) should be 20.
    // HW should adapt to this pattern.
    
    // printf("Forecast: %f, %f\n", forecast[0], forecast[1]);
    
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 10.0f, forecast[0]);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 20.0f, forecast[1]);
    
    return true;
}

BEGIN_TEST_SUITE(run_ts_tests)
    RUN_TEST(test_arima_simple);
    RUN_TEST(test_arima_full);
    RUN_TEST(test_arima_fit);
    RUN_TEST(test_hw_seasonal);
END_TEST_SUITE()
