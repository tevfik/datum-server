#include "eif_ml.h"
#include "eif_test_runner.h"
#include <string.h>
#include <math.h>

// Mock pool for testing
static uint8_t pool_buffer[1024 * 1024];
static eif_memory_pool_t pool;

static void setup(void) {
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
}

static void teardown(void) {
    // No cleanup needed
}

static bool test_iforest_predict_coverage(void) {
    eif_iforest_t forest;
    int num_trees = 5;
    int max_samples = 32;
    int max_depth = 5;
    int num_features = 2;
    
    eif_status_t status = eif_iforest_init(&forest, num_trees, max_samples, max_depth, num_features, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);

    // Create dummy data
    float32_t data[64]; // 32 samples * 2 features
    for (int i = 0; i < 64; i++) data[i] = (float32_t)(i % 10);

    status = eif_iforest_fit(&forest, data, 32, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);

    float32_t sample[2] = {5.0f, 5.0f};
    
    // Call score first to verify standard path
    float32_t score = eif_iforest_score(&forest, sample);
    TEST_ASSERT_TRUE(score >= 0.0f && score <= 1.0f);

    // Call predict explicitly (missed coverage)
    // Threshold 0.5
    float32_t pred = eif_iforest_predict(&forest, sample, 0.5f);
    TEST_ASSERT_TRUE(pred == 0.0f || pred == 1.0f);
    
    return true;
}

static bool test_rforest_predict_coverage(void) {
    eif_rforest_t forest;
    eif_rforest_config_t config = {
        .num_trees = 3,
        .max_depth = 3,
        .min_samples_split = 2,
        .max_features = 0, // Should trigger sqrt default
        .sample_ratio = 1.0f
    };
    
    // Regression setup to trigger regression branch in predict
    eif_status_t status = eif_rforest_init(&forest, &config, EIF_DTREE_REGRESSION, 2, 1, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);

    // Dummy data for regression
    float32_t X[40]; // 20 samples * 2
    float32_t y[20];
    for (int i = 0; i < 20; i++) {
        X[i*2] = (float)i;
        X[i*2+1] = (float)i;
        y[i] = (float)i;
    }

    status = eif_rforest_fit(&forest, X, y, 20, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);

    float32_t sample[2] = {5.0f, 5.0f};
    float32_t pred = eif_rforest_predict(&forest, sample);
    TEST_ASSERT_TRUE(pred >= 0.0f);

    // Test Classification prediction via float interface
    eif_rforest_t forest_cls;
    eif_rforest_config_t config_cls = config;
    status = eif_rforest_init(&forest_cls, &config_cls, EIF_DTREE_CLASSIFICATION, 2, 2, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);

    // Data for classification (y must be 0 or 1)
    for (int i = 0; i < 20; i++) y[i] = (float)(i % 2);
    
    status = eif_rforest_fit(&forest_cls, X, y, 20, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    pred = eif_rforest_predict(&forest_cls, sample); // Should call predict_class internally
    TEST_ASSERT_TRUE(pred == 0.0f || pred == 1.0f);
    
    return true;
}

static bool test_rforest_bad_config(void) {
    eif_rforest_t forest;
    eif_rforest_config_t config = {
        .num_trees = 3,
        .sample_ratio = 1.5f // Invalid > 1.0
    };
    
    eif_status_t status = eif_rforest_init(&forest, &config, EIF_DTREE_CLASSIFICATION, 2, 2, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status); // Should convert ratio to 1.0 and succeed
    TEST_ASSERT_EQUAL_FLOAT(1.0f, forest.config.sample_ratio, 1e-5);
    
    return true;
}

static bool test_iforest_recursion_limit(void) {
    eif_iforest_t forest;
    eif_status_t status = eif_iforest_init(&forest, 1, 32, 1, 2, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    float32_t data[20];
    for(int i=0; i<20; i++) data[i] = (float)i;
    status = eif_iforest_fit(&forest, data, 10, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    return true;
}

static bool test_dtree_wrapper(void) {
    eif_rforest_t forest;
    eif_rforest_config_t config = {
        .num_trees = 1, .max_depth = 2, .min_samples_split = 2, .max_features = 1, .sample_ratio = 1.0f
    };
    eif_rforest_init(&forest, &config, EIF_DTREE_CLASSIFICATION, 2, 2, &pool);
    float32_t X[4] = {0,0, 1,1};
    float32_t y[2] = {0, 1};
    eif_rforest_fit(&forest, X, y, 2, &pool);
    float32_t sample[2] = {0,0};
    int cls = eif_dtree_predict_class(&forest.trees[0], sample);
    TEST_ASSERT_EQUAL_INT(0, cls);
    return true;
}

int run_trees_coverage_tests(void) {
    setup();
    int failed = 0;

    if (!test_iforest_recursion_limit()) {
        printf("test_iforest_recursion_limit failed\n");
        failed++;
    }
    
    if (!test_dtree_wrapper()) {
        printf("test_dtree_wrapper failed\n");
        failed++;
    }
    
    if (!test_iforest_predict_coverage()) {
        printf("test_iforest_predict_coverage failed\n");
        failed++;
    }
    
    if (!test_rforest_predict_coverage()) {
        printf("test_rforest_predict_coverage failed\n");
        failed++;
    }
    
    if (!test_rforest_bad_config()) {
        printf("test_rforest_bad_config failed\n");
        failed++;
    }
    
    teardown();
    return failed;
}
