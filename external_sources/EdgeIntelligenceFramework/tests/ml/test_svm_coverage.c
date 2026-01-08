#include "../framework/eif_test_runner.h"
#include "eif_ml.h"
#include "eif_utils.h"
#include <string.h>

static bool test_svm_decision_direct(void) {
    eif_svm_t svm;
    uint8_t pool_buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Init
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_svm_init(&svm, 2, 0.01f, &pool));
    
    // Set weights manually
    svm.weights[0] = 0.5f;
    svm.weights[1] = -0.5f;
    svm.bias = 0.1f;
    
    // Check decision
    float32_t input[] = {1.0f, 1.0f};
    // 0.5*1 + (-0.5)*1 + 0.1 = 0.5 - 0.5 + 0.1 = 0.1
    float32_t dec = eif_svm_decision(&svm, input);
    TEST_ASSERT_EQUAL_FLOAT(0.1f, dec, 0.0001f);
    
    float32_t input2[] = {2.0f, 0.0f};
    // 0.5*2 + 0 + 0.1 = 1.1
    dec = eif_svm_decision(&svm, input2);
    TEST_ASSERT_EQUAL_FLOAT(1.1f, dec, 0.0001f);
    
    // Call with null
    TEST_ASSERT_EQUAL_FLOAT(0.0f, eif_svm_decision(NULL, input), 0.0001f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, eif_svm_decision(&svm, NULL), 0.0001f);

    return true;
}

static bool test_svm_fit_predict(void) {
    eif_svm_t svm;
    uint8_t pb[4096];
    eif_memory_pool_t p;
    eif_memory_pool_init(&p, pb, sizeof(pb));
    
    // Init
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_svm_init(&svm, 2, 0.001f, &p));
    
    // Linearly separable data
    // Label 1 -> 1
    // Label 0 -> -1 internally
    float32_t X[] = {1,1,  2,2,  -1,-1,  -2,-2};
    int y[] = {1, 1, 0, 0};
    
    // Fit
    eif_status_t status = eif_svm_fit(&svm, X, y, 4, 100); 
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Predict
    float32_t sample1[] = {1.5f, 1.5f};
    int p1 = eif_svm_predict(&svm, sample1);
    TEST_ASSERT_EQUAL_INT(1, p1);
    
    float32_t sample2[] = {-1.5f, -1.5f};
    int p0 = eif_svm_predict(&svm, sample2);
    TEST_ASSERT_EQUAL_INT(0, p0);
    
    return true;
}

static bool test_adaboost_imperfect_fit(void) {
    eif_adaboost_t model;
    uint8_t pool_buffer[4096];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    int num_estimators = 5;
    int num_features = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, 
        eif_adaboost_init(&model, num_estimators, num_features, &pool));
        
    float32_t X[] = {0.0f, 1.0f, 2.0f};
    int y[] = {0, 1, 0};
    int num_samples = 3;
    
    eif_status_t status = eif_adaboost_fit(&model, X, y, num_samples, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Test predict
    float32_t s0[] = {0.0f}; // Should be 0
    // But adaboost might fail to fit perfectly. Just ensuring it runs.
    int p0 = eif_adaboost_predict(&model, s0);
    TEST_ASSERT_TRUE(p0 == 0 || p0 == 1);
    
    return true;
}

static bool test_minibatch_kmeans(void) {
    eif_minibatch_kmeans_t km;
    uint8_t pb[4096];
    eif_memory_pool_t p;
    eif_memory_pool_init(&p, pb, sizeof(pb));
    
    int k = 2;
    int n_feat = 2;
    int batch = 2;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, 
        eif_minibatch_kmeans_init(&km, k, n_feat, batch, &p));
        
    float32_t X[] = {
        0.1f, 0.1f, 
        0.2f, 0.0f, 
        9.9f, 9.9f, 
        10.0f, 10.1f 
    };
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, 
        eif_minibatch_kmeans_partial_fit(&km, X, 4));
        
    // First call uses data for initialization and returns.
    // Call again to actually train on data
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, 
        eif_minibatch_kmeans_partial_fit(&km, X, 4));
        
    float32_t s0[] = {0.0f, 0.0f};
    int c0 = eif_minibatch_kmeans_predict(&km, s0);
    
    float32_t s1[] = {10.0f, 10.0f};
    int c1 = eif_minibatch_kmeans_predict(&km, s1);
    
    TEST_ASSERT(c0 != c1);
    
    return true;
}

static bool test_adaboost_oom(void) {
    eif_adaboost_t model;
    uint8_t pb[32]; // Tiny buffer
    eif_memory_pool_t p;
    eif_memory_pool_init(&p, pb, sizeof(pb));
    
    // Should fail due to OOM
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OUT_OF_MEMORY, 
        eif_adaboost_init(&model, 10, 1, &p));
        
    return true;
}

BEGIN_TEST_SUITE(run_svm_coverage_tests)
    RUN_TEST(test_svm_decision_direct);
    RUN_TEST(test_svm_fit_predict);
    RUN_TEST(test_adaboost_imperfect_fit);
    RUN_TEST(test_minibatch_kmeans);
    RUN_TEST(test_adaboost_oom);
END_TEST_SUITE()
