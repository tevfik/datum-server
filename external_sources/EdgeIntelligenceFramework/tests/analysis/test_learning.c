#include "eif_data_analysis.h"
#include "eif_test_runner.h"
#include <math.h>

bool test_kmeans_online() {
    // Test Incremental K-Means
    // 2 Clusters: (0,0) and (10,10)
    
    eif_kmeans_online_t model;
    uint8_t pool_buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_kmeans_online_init(&model, 2, 2, &pool);
    
    // Seed centroids manually for deterministic test
    model.centroids[0] = 0.1f; model.centroids[1] = 0.1f;
    model.centroids[2] = 9.9f; model.centroids[3] = 9.9f;
    
    // Update with points near (0,0)
    float32_t p1[] = {0.2f, 0.2f};
    eif_kmeans_online_update(&model, p1, 0.1f);
    
    // Update with points near (10,10)
    float32_t p2[] = {10.1f, 10.1f};
    eif_kmeans_online_update(&model, p2, 0.1f);
    
    // Check if centroids moved slightly
    // C0 should move towards (0.2, 0.2)
    // C1 should move towards (10.1, 10.1)
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.11f, model.centroids[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 9.92f, model.centroids[2]);
    
    return true;
}

bool test_linreg_online() {
    // Test Online Linear Regression (RLS)
    // Target: y = 2*x + 1
    
    eif_linreg_online_t model;
    uint8_t pool_buffer[4096];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_linreg_online_init(&model, 1, 0.99f, &pool);
    
    // Train on a few points
    for (int i = 0; i < 50; i++) {
        float32_t x = (float32_t)i;
        float32_t y = 2.0f * x + 1.0f;
        eif_linreg_online_update(&model, &x, y, &pool);
    }
    
    // Predict
    float32_t test_x = 100.0f;
    float32_t pred = eif_linreg_online_predict(&model, &test_x);
    float32_t expected = 2.0f * test_x + 1.0f;
    
    TEST_ASSERT_FLOAT_WITHIN(0.1f, expected, pred);
    
    // Check weights
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 2.0f, model.weights[0]); // Slope
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1.0f, model.weights[1]); // Bias
    
    return true;
}

bool test_anomaly_online() {
    // Test Online Anomaly Detection
    // Normal data: Mean 10, Var small
    
    eif_anomaly_online_t model;
    uint8_t pool_buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_anomaly_online_init(&model, 1, &pool);
    
    // Train with normal data
    for (int i = 0; i < 100; i++) {
        float32_t val = 10.0f + ((i % 5) - 2) * 0.1f; // 9.8 to 10.2
        eif_anomaly_online_update(&model, &val);
    }
    
    // Test normal point
    float32_t normal = 10.0f;
    float32_t score_normal = eif_anomaly_online_score(&model, &normal);
    
    // Test anomaly
    float32_t anomaly = 20.0f;
    float32_t score_anomaly = eif_anomaly_online_score(&model, &anomaly);
    
    TEST_ASSERT_TRUE(score_anomaly > score_normal * 10.0f);
    
    return true;
}

BEGIN_TEST_SUITE(run_learning_tests)
    RUN_TEST(test_kmeans_online);
    RUN_TEST(test_linreg_online);
    RUN_TEST(test_anomaly_online);
END_TEST_SUITE()
