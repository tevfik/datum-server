#include "../framework/eif_test_runner.h"
#include "eif_ml_knn.h"
#include <float.h>
#include <string.h>

#define TEST_ASSERT_EQUAL_PTR(expected, actual) TEST_ASSERT((expected) == (actual))

static bool test_knn_init(void) {
    eif_ml_knn_t knn;
    float32_t data[] = {1.0f};
    int32_t labels[] = {1};
    
    eif_ml_knn_init(&knn, data, labels, 1, 1, 1);
    
    TEST_ASSERT_EQUAL_PTR(data, knn.train_data);
    TEST_ASSERT_EQUAL_PTR(labels, knn.train_labels);
    TEST_ASSERT_EQUAL_INT(1, knn.num_samples);
    TEST_ASSERT_EQUAL_INT(1, knn.num_features);
    TEST_ASSERT_EQUAL_INT(1, knn.k);
    
    // Test null check
    eif_ml_knn_init(NULL, data, labels, 1, 1, 1);
    return true;
}

static bool test_knn_predict_simple(void) {
    eif_ml_knn_t knn;
    // 2D data:
    // Class 0: (0,0), (0,1)
    // Class 1: (10,10), (10,11)
    float32_t data[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        10.0f, 10.0f,
        10.0f, 11.0f
    };
    int32_t labels[] = {0, 0, 1, 1};
    
    eif_ml_knn_init(&knn, data, labels, 4, 2, 1);
    
    float32_t input0[] = {0.1f, 0.1f}; // Close to class 0
    int32_t p0 = eif_ml_knn_predict(&knn, input0);
    TEST_ASSERT_EQUAL_INT(0, p0);
    
    float32_t input1[] = {10.1f, 10.1f}; // Close to class 1
    int32_t p1 = eif_ml_knn_predict(&knn, input1);
    TEST_ASSERT_EQUAL_INT(1, p1);
    return true;
}

static bool test_knn_predict_vote(void) {
    eif_ml_knn_t knn;
    // 1D data
    // 3 neighbors close to 2.0
    // 2 are Class 1, 1 is Class 0
    // 1.9 (1), 2.0 (0), 2.1 (1)
    float32_t data[] = {1.9f, 2.0f, 2.1f, 10.0f};
    int32_t labels[] = {1, 0, 1, 0};
    
    // k=3, inputs 1.9, 2.0, 2.1 will be neighbors
    // Votes: Class 1 (2 votes), Class 0 (1 vote) => Expect 1
    eif_ml_knn_init(&knn, data, labels, 4, 1, 3);
    
    float32_t input[] = {2.0f};
    int32_t pred = eif_ml_knn_predict(&knn, input);
    TEST_ASSERT_EQUAL_INT(1, pred);
    return true;
}

static bool test_knn_invalid(void) {
    eif_ml_knn_t knn;
    float32_t input[] = {0.0f};
    
    // Null knn
    TEST_ASSERT_EQUAL_INT(-1, eif_ml_knn_predict(NULL, input));
    
    // Null data
    memset(&knn, 0, sizeof(knn));
    TEST_ASSERT_EQUAL_INT(-1, eif_ml_knn_predict(&knn, input));
    
    // Invalid K (> MAX_K usually 20)
    float32_t data[] = {0};
    int32_t labels[] = {0};
    eif_ml_knn_init(&knn, data, labels, 1, 1, 100); 
    TEST_ASSERT_EQUAL_INT(-1, eif_ml_knn_predict(&knn, input));
    return true;
}

BEGIN_TEST_SUITE(run_ml_knn_coverage_tests)
    RUN_TEST(test_knn_init);
    RUN_TEST(test_knn_predict_simple);
    RUN_TEST(test_knn_predict_vote);
    RUN_TEST(test_knn_invalid);
END_TEST_SUITE()
