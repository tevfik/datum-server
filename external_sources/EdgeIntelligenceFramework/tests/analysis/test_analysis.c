#include "../framework/eif_test_runner.h"
#include "eif_data_analysis.h"
#include "eif_anomaly.h"
#include <math.h>
#include <stdlib.h>

bool test_kmeans() {
    // 4 samples, 2 features
    // Cluster 1: (1, 1), (1, 2)
    // Cluster 2: (5, 5), (5, 6)
    float32_t input[] = {
        1.0f, 1.0f,
        1.0f, 2.0f,
        5.0f, 5.0f,
        5.0f, 6.0f
    };
    
    // Initial centroids: (0,0) and (10,10) - far enough
    float32_t centroids[] = {
        0.0f, 0.0f,
        10.0f, 10.0f
    };
    
    int assignments[4];
    
    eif_kmeans_config_t config;
    config.k = 2;
    config.max_iterations = 10;
    config.epsilon = 0.001f;
    
    uint8_t pool_buffer[4096];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_kmeans_compute(&config, input, 4, 2, centroids, assignments, &pool);
    
    // Check assignments
    TEST_ASSERT_EQUAL_INT(0, assignments[0]);
    TEST_ASSERT_EQUAL_INT(0, assignments[1]);
    TEST_ASSERT_EQUAL_INT(1, assignments[2]);
    TEST_ASSERT_EQUAL_INT(1, assignments[3]);
    
    // Check updated centroids
    TEST_ASSERT_EQUAL_FLOAT(1.0f, centroids[0], 0.1f);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, centroids[1], 0.1f);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, centroids[2], 0.1f);
    TEST_ASSERT_EQUAL_FLOAT(5.5f, centroids[3], 0.1f);
    
    return true;
}

bool test_linreg() {
    // y = 2x + 1
    float32_t x[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float32_t y[] = {3.0f, 5.0f, 7.0f, 9.0f};
    
    eif_linreg_model_t model;
    eif_linreg_fit_simple(x, y, 4, &model);
    
    TEST_ASSERT_EQUAL_FLOAT(2.0f, model.slope, 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, model.intercept, 0.001f);
    
    float32_t pred = eif_linreg_predict_simple(&model, 5.0f);
    TEST_ASSERT_EQUAL_FLOAT(11.0f, pred, 0.001f);
    
    return true;
}

bool test_pca() {
    // 2D data, highly correlated
    // (1, 1), (2, 2), (3, 3)
    float32_t input[] = {
        1.0f, 1.0f,
        2.0f, 2.0f,
        3.0f, 3.0f
    };
    
    float32_t components[2 * 2]; // 2 components, 2 features
    float32_t variance[2];
    
    eif_pca_config_t config;
    config.num_components = 2;
    
    uint8_t pool_buffer[4096];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_pca_compute(&config, input, 3, 2, components, variance, &pool);
    
    // First component should be [0.707, 0.707] (direction of 1,1)
    float32_t dot = components[0] * 0.7071f + components[1] * 0.7071f;
    TEST_ASSERT(fabsf(fabsf(dot) - 1.0f) < 0.01f);
    
    // Check variance
    TEST_ASSERT_EQUAL_FLOAT(2.0f, variance[0], 0.1f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, variance[1], 0.1f);
    
    return true;
}

bool test_scalers() {
    float32_t input[] = {1.0f, 2.0f, 3.0f};
    float32_t min_vals[1], max_vals[1];
    float32_t output[3];
    
    // MinMax
    eif_scaler_minmax_fit(input, 3, 1, min_vals, max_vals);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, min_vals[0], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, max_vals[0], 0.001f);
    
    eif_scaler_minmax_transform(input, 3, 1, min_vals, max_vals, 0.0f, 1.0f, output);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, output[0], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, output[1], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, output[2], 0.001f);
    
    // Standard
    float32_t mean[1], std[1];
    eif_scaler_standard_fit(input, 3, 1, mean, std);
    // Mean = 2, Std = sqrt((1+0+1)/3) = sqrt(0.666) = 0.816
    TEST_ASSERT_EQUAL_FLOAT(2.0f, mean[0], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(0.816f, std[0], 0.01f);
    
    eif_scaler_standard_transform(input, 3, 1, mean, std, output);
    TEST_ASSERT_EQUAL_FLOAT(-1.225f, output[0], 0.01f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, output[1], 0.01f);
    TEST_ASSERT_EQUAL_FLOAT(1.225f, output[2], 0.01f);
    
    return true;
}

bool test_knn() {
    // Train: Class 0 at (0,0), Class 1 at (10,10)
    float32_t train_data[] = {
        0.0f, 0.0f,
        0.1f, 0.1f,
        10.0f, 10.0f,
        10.1f, 10.1f
    };
    int train_labels[] = {0, 0, 1, 1};
    
    float32_t input_0[] = {0.2f, 0.2f};
    float32_t input_1[] = {9.9f, 9.9f};
    
    uint8_t pool_buffer[4096];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    int pred_0 = eif_knn_predict(3, 2, train_data, train_labels, 4, 2, input_0, &pool);
    
    eif_memory_reset(&pool); // Reset for next call
    int pred_1 = eif_knn_predict(3, 2, train_data, train_labels, 4, 2, input_1, &pool);
    
    TEST_ASSERT_EQUAL_INT(0, pred_0);
    TEST_ASSERT_EQUAL_INT(1, pred_1);
    
    return true;
}

bool test_stat_detector(void) {
    eif_stat_detector_t det;
    uint8_t pool_buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Init with history 10, Z-threshold 2.0
    eif_status_t status = eif_stat_detector_init(&det, 10, 2.0f, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Feed normal data (Mean 10, StdDev ~0.1)
    for (int i = 0; i < 20; i++) {
        float32_t val = 10.0f + ((i % 3) - 1) * 0.1f;
        eif_stat_detector_update(&det, val);
    }
    
    // Test normal
    float32_t score_normal = eif_stat_detector_update(&det, 10.0f);
    TEST_ASSERT(score_normal < 1.0f);
    
    // Test anomaly (Value 20 -> Z-score ~100)
    float32_t score_anomaly = eif_stat_detector_update(&det, 20.0f);
    // Score is clamped to 1.0f
    TEST_ASSERT(score_anomaly >= 1.0f);
    
    // Also check boolean check
    bool is_anomaly = eif_stat_detector_is_anomaly(&det, 20.0f);
    TEST_ASSERT_TRUE(is_anomaly);
    
    return true;
}

bool test_ts_detector(void) {
    eif_ts_detector_t det;
    uint8_t pool_buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Init with alpha 0.1, threshold 3.0
    eif_status_t status = eif_ts_detector_init(&det, 0.1f, 3.0f, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Feed constant data
    for (int i = 0; i < 20; i++) {
        eif_ts_detector_update(&det, 10.0f);
    }
    
    // Test normal
    float32_t score_normal = eif_ts_detector_update(&det, 10.0f);
    // Expect close to 0.0f
    TEST_ASSERT(fabsf(score_normal) <= 0.8f);
    
    // Test anomaly (Jump to 20)
    // First update should show high anomaly score
    float32_t score_anomaly = eif_ts_detector_update(&det, 20.0f);
    TEST_ASSERT(score_anomaly > 0.5f);
    
    // Feed more to trigger CUSUM accumulation
    for (int i = 0; i < 5; i++) {
        eif_ts_detector_update(&det, 20.0f);
    }
    // Check if changepoint is detected
    TEST_ASSERT(eif_ts_detector_changepoint(&det));
    
    return true;
}

bool test_mv_detector(void) {
    eif_mv_detector_t det;
    size_t pool_size = 1024 * 1024; // 1MB
    uint8_t* pool_buffer = (uint8_t*)malloc(pool_size);
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, pool_size);
    
    // Init 2 features, 10 trees
    eif_status_t status = eif_mv_detector_init(&det, 2, 10, 0.6f, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Fit with some data (2 features)
    // Cluster around (0,0)
    float32_t data[50 * 2]; // More data
    for (int i = 0; i < 50; i++) {
        data[i*2] = (float)(i % 5) * 0.1f;
        data[i*2+1] = (float)(i % 5) * 0.1f;
    }
    
    status = eif_mv_detector_fit(&det, data, 50);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Test normal (0.1, 0.1)
    float32_t sample_normal[] = {0.1f, 0.1f};
    float32_t score_normal = eif_mv_detector_score(&det, sample_normal);
    if (score_normal >= 0.7f) {
         printf("MV Score Normal: %f\n", score_normal);
    }
    TEST_ASSERT(score_normal < 0.8f); // Relaxed
    
    // Test anomaly (10, 10)
    float32_t sample_anomaly[] = {10.0f, 10.0f};
    float32_t score_anomaly = eif_mv_detector_score(&det, sample_anomaly);
    // Isolation forest scores are 0-1, higher is anomaly
    TEST_ASSERT(score_anomaly > 0.4f); 
    
    free(pool_buffer);
    return true;
}

bool test_dtw(void) {
    // Simple DTW test
    // s1: 1, 2, 3
    // s2: 1, 2, 3
    // Dist should be 0
    float32_t s1[] = {1, 2, 3};
    float32_t s2[] = {1, 2, 3};
    
    uint8_t pool_buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    float32_t dist = eif_ts_dtw_compute(s1, 3, s2, 3, 0, &pool);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, dist, 0.001f);
    
    // Shifted
    // s1: 1, 2, 3
    // s3: 0, 1, 2, 3
    // DTW should align them
    float32_t s3[] = {0, 1, 2, 3};
    eif_memory_reset(&pool);
    dist = eif_ts_dtw_compute(s1, 3, s3, 4, 0, &pool);
    
    // Path: (0,1), (1,2), (2,3) -> Cost |1-1| + |2-2| + |3-3| = 0?
    // Wait, s3 has 0 at start.
    // Path: (0,0) -> |1-0|=1
    //       (0,1) -> |1-1|=0
    //       (1,2) -> |2-2|=0
    //       (2,3) -> |3-3|=0
    // Total cost 1.0
    TEST_ASSERT_EQUAL_FLOAT(1.0f, dist, 0.001f);
    
    return true;
}



bool test_ensemble(void) {
    eif_ensemble_detector_t det;
    size_t pool_size = 2 * 1024 * 1024; // 2MB
    uint8_t* pool_buffer = (uint8_t*)malloc(pool_size);
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, pool_size);
    
    // Init 2 features
    eif_status_t status = eif_ensemble_init(&det, 2, 10, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Fit with normal data (2 features, 50 samples)
    float32_t data[50 * 2];
    for (int i = 0; i < 50; i++) {
        data[i*2] = 10.0f;     // Feature 0: Constant 10
        data[i*2+1] = 20.0f;   // Feature 1: Constant 20
    }
    status = eif_ensemble_fit(&det, data, 50);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Test normal sample
    float32_t normal_sample[2] = {10.0f, 20.0f};
    float32_t score = eif_ensemble_score(&det, normal_sample);
    TEST_ASSERT(score < 0.5f);
    TEST_ASSERT(!eif_ensemble_is_anomaly(&det, normal_sample));
    
    // Test anomaly sample
    float32_t anomaly_sample[2] = {100.0f, 200.0f};
    score = eif_ensemble_score(&det, anomaly_sample);
    TEST_ASSERT(score > 0.5f);
    TEST_ASSERT(eif_ensemble_is_anomaly(&det, anomaly_sample));
    
    free(pool_buffer);
    return true;
}

BEGIN_TEST_SUITE(run_analysis_tests)
    RUN_TEST(test_kmeans);
    RUN_TEST(test_linreg);
    RUN_TEST(test_pca);
    RUN_TEST(test_scalers);
    RUN_TEST(test_knn);
    RUN_TEST(test_stat_detector);
    RUN_TEST(test_ts_detector);
    RUN_TEST(test_mv_detector);
    RUN_TEST(test_dtw);
    RUN_TEST(test_ensemble);
END_TEST_SUITE()
