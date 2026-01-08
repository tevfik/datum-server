#include "../framework/eif_test_runner.h"
#include "eif_ml_rf.h"
#include <string.h>

// Mock pool
static uint8_t pool_buffer[4096 * 4];
static eif_memory_pool_t pool;

static void setup_rf_env(void) {
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
}

static bool test_rf_accessors(void) {
    eif_rf_t rf = {0};
    rf.oob_score = 0.95f;
    float32_t imp[2] = {0.1f, 0.2f};
    rf.feature_importance = imp;
    rf.num_features = 2;
    
    // Test OOB Score
    float32_t score = 0.0f;
    eif_status_t status = eif_rf_get_oob_score(&rf, &score);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_EQUAL_FLOAT(0.95f, score, 0.001f);
    
    // Test Invalid OOB
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_rf_get_oob_score(NULL, &score));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_rf_get_oob_score(&rf, NULL));

    // Test Feature Importance
    float32_t read_imp[2];
    status = eif_rf_get_feature_importance(&rf, read_imp);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_EQUAL_FLOAT(0.1f, read_imp[0], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(0.2f, read_imp[1], 0.001f);

    // Test Invalid Importance
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_rf_get_feature_importance(NULL, read_imp));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_rf_get_feature_importance(&rf, NULL));
    
    return true;
}

static bool test_rf_cleanup_coverage(void) {
    eif_rf_t rf = {0};
    // Cleanup should handle NULL or initialized struct
    eif_rf_cleanup(NULL);
    eif_rf_cleanup(&rf);
    
    // Check if pointers are nulled
    TEST_ASSERT(rf.trees == NULL);
    TEST_ASSERT(rf.feature_importance == NULL);
    return true;
}

static bool test_rf_split_coverage(void) {
    setup_rf_env();
    eif_rf_t rf;
    // Init with min_samples_split = 1 to hit n_samples < 2 in find_best_split
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_rf_init(&rf, 1, 3, 1, 1, 2, 2, &pool));
    
    // Data with 2 samples
    float32_t X[4] = {1.0f, 1.0f, 2.0f, 2.0f};
    uint16_t y[2] = {0, 1};
    
    // Fit should recurse until leaf size 1, calling find_best_split with 1 sample
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_rf_fit(&rf, X, y, 2, &pool));
    
    eif_rf_cleanup(&rf);
    return true;
}

static bool test_rf_constant_data_coverage(void) {
    setup_rf_env();
    eif_rf_t rf;
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_rf_init(&rf, 1, 2, 2, 1, 1, 2, &pool));
    
    // Constant data points - splits will fail (n_left=0 or n_right=0)
    float32_t X[6] = {1.0f, 1.0f, 1.0f}; 
    uint16_t y[3] = {0, 1, 0};
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_rf_fit(&rf, X, y, 3, &pool));
    
    eif_rf_cleanup(&rf);
    return true;
}

BEGIN_TEST_SUITE(run_rf_coverage_tests)
    RUN_TEST(test_rf_accessors);
    RUN_TEST(test_rf_cleanup_coverage);
    RUN_TEST(test_rf_split_coverage);
    RUN_TEST(test_rf_constant_data_coverage);

END_TEST_SUITE()
