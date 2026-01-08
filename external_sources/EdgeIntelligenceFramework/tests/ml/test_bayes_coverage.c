#include "../framework/eif_test_runner.h"
#include "eif_ml.h"
#include "eif_utils.h"
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Helper to access private members if needed, but struct is public in eif_ml.h

static bool test_bayes_edge_cases(void) {
    eif_naive_bayes_t model;
    uint8_t pool_buffer[4096];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));

    // Case 1: Training with single sample in a class (variance padding check)
    // Setup: 2 features, 2 classes. 
    // Class 0: No samples (will stay init values)
    // Class 1: 1 sample
    
    int num_features = 2;
    int num_classes = 2;
    
    eif_status_t status = eif_nb_init(&model, num_features, num_classes, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);

    float32_t data[] = {1.0f, 2.0f}; // 1 sample, 2 features
    int labels[] = {1}; // Only class 1 present
    
    // Fit with 1 sample
    status = eif_nb_fit(&model, data, labels, 1);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);

    // Verify Class 0 (empty)
    // Count should be 0
    TEST_ASSERT_EQUAL_INT(0, model.class_counts[0]);
    
    // Verify Class 1 (1 sample)
    TEST_ASSERT_EQUAL_INT(1, model.class_counts[1]);
    
    // Mean should be exactly data
    TEST_ASSERT_EQUAL_FLOAT(1.0f, model.means[1 * num_features + 0], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, model.means[1 * num_features + 1], 0.001f);
    
    // Variance for single sample is 0, effectively. 
    // Implementation usually adds epsilon. Check if variance is small constant.
    float32_t var0 = model.variances[1 * num_features + 0];
    float32_t var1 = model.variances[1 * num_features + 1];
    
    // It should be > 0 (padded)
    TEST_ASSERT_TRUE(var0 > 0.0f);
    TEST_ASSERT_TRUE(var1 > 0.0f);
    
    // Test Prediction
    float32_t input[] = {1.0f, 2.0f};
    int pred = eif_nb_predict(&model, input);
    TEST_ASSERT_EQUAL_INT(1, pred);

    // Test Probabilities with usage of missing class (Class 0 has 0 prior)
    // This hits the branch where prior <= 0
    float32_t probs[2];
    status = eif_nb_predict_proba(&model, input, probs);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Class 0 (missing) should be 0
    TEST_ASSERT_EQUAL_FLOAT(0.0f, probs[0], 0.0001f);
    // Class 1 should be 1.0
    TEST_ASSERT_EQUAL_FLOAT(1.0f, probs[1], 0.0001f);
    
    return true;
}

static bool test_bayes_null_checks(void) {
    eif_naive_bayes_t model;
    uint8_t pool_buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_nb_init(NULL, 1, 2, &pool));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_nb_init(&model, 0, 2, &pool)); // 0 features
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_nb_init(&model, 1, 1, &pool)); // 1 class (need >=2)
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_nb_init(&model, 1, 2, NULL));
    
    eif_nb_init(&model, 1, 2, &pool);
    // Fit null checks
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_nb_fit(NULL, NULL, NULL, 0));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_nb_fit(&model, NULL, NULL, 1));
    
    // Predict null checks
    TEST_ASSERT_EQUAL_INT(0, eif_nb_predict(NULL, NULL));
    TEST_ASSERT_EQUAL_INT(0, eif_nb_predict(&model, NULL));
    
    // Predict Proba null checks
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_nb_predict_proba(NULL, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_nb_predict_proba(&model, NULL, NULL));
    float32_t p[2];
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_nb_predict_proba(&model, p, NULL));

    return true;
}

int run_bayes_coverage_tests(void) {
    tests_failed = 0;
    RUN_TEST(test_bayes_edge_cases);
    RUN_TEST(test_bayes_null_checks);
    return tests_failed;
}
