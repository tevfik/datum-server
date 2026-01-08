/**
 * @file test_ml_algorithms.c
 * @brief Unit tests for Phase 1 ML algorithms
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>

#include "eif_types.h"
#include "eif_memory.h"
#include "eif_data_analysis.h"

static uint8_t pool_buffer[1024 * 1024];
static eif_memory_pool_t pool;
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("Running %s... ", #name); \
    tests_run++; \
    if (name()) { tests_passed++; printf("PASS\n"); } \
    else { printf("FAIL\n"); } \
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer)); \
} while(0)

// ============================================================================
// Isolation Forest Tests
// ============================================================================

static bool test_iforest_init(void) {
    eif_iforest_t forest;
    eif_status_t status = eif_iforest_init(&forest, 10, 32, 5, 2, &pool);
    
    if (status != EIF_STATUS_OK) return false;
    if (forest.num_trees != 10) return false;
    if (forest.num_features != 2) return false;
    
    return true;
}

static bool test_iforest_fit_score(void) {
    // Generate simple normal data centered at origin
    float32_t data[20 * 2];
    for (int i = 0; i < 20; i++) {
        data[i * 2] = ((float)rand() / RAND_MAX - 0.5f) * 0.5f;
        data[i * 2 + 1] = ((float)rand() / RAND_MAX - 0.5f) * 0.5f;
    }
    
    eif_iforest_t forest;
    eif_iforest_init(&forest, 20, 16, 4, 2, &pool);
    eif_iforest_fit(&forest, data, 20, &pool);
    
    // Normal sample should have lower score
    float32_t normal_sample[2] = {0.1f, 0.1f};
    float32_t normal_score = eif_iforest_score(&forest, normal_sample);
    
    // Anomaly should have higher score
    float32_t anomaly_sample[2] = {5.0f, 5.0f};
    float32_t anomaly_score = eif_iforest_score(&forest, anomaly_sample);
    
    // Anomaly score should generally be higher
    return anomaly_score > normal_score;
}

// ============================================================================
// Logistic Regression Tests
// ============================================================================

static bool test_logreg_init(void) {
    eif_logreg_t model;
    eif_status_t status = eif_logreg_init(&model, 3, 0.1f, 0.01f, &pool);
    
    if (status != EIF_STATUS_OK) return false;
    if (model.num_features != 3) return false;
    if (model.weights == NULL) return false;
    
    return true;
}

static bool test_logreg_binary(void) {
    // Simple linearly separable data
    float32_t X[8] = {
        -1.0f, -1.0f,  // class 0
        -0.5f, -0.5f,  // class 0
        0.5f, 0.5f,    // class 1
        1.0f, 1.0f     // class 1
    };
    int y[4] = {0, 0, 1, 1};
    
    eif_logreg_t model;
    eif_logreg_init(&model, 2, 0.5f, 0.0f, &pool);
    eif_logreg_fit(&model, X, y, 4, 100);
    
    // Test predictions
    float32_t test1[2] = {-0.8f, -0.8f};
    float32_t test2[2] = {0.8f, 0.8f};
    
    int pred1 = eif_logreg_predict(&model, test1);
    int pred2 = eif_logreg_predict(&model, test2);
    
    return pred1 == 0 && pred2 == 1;
}

static bool test_logreg_multiclass(void) {
    // 3-class data
    float32_t X[12] = {
        1.0f, 0.0f,   // class 0
        0.8f, 0.2f,   // class 0
        0.0f, 1.0f,   // class 1
        0.2f, 0.8f,   // class 1
        -1.0f, 0.0f,  // class 2
        -0.8f, 0.2f   // class 2
    };
    int y[6] = {0, 0, 1, 1, 2, 2};
    
    eif_logreg_multiclass_t model;
    eif_logreg_multiclass_init(&model, 2, 3, 0.5f, 0.0f, &pool);
    eif_logreg_multiclass_fit(&model, X, y, 6, 100);
    
    // Test predictions
    float32_t test1[2] = {0.9f, 0.1f};
    float32_t test2[2] = {0.1f, 0.9f};
    float32_t test3[2] = {-0.9f, 0.1f};
    
    int pred1 = eif_logreg_multiclass_predict(&model, test1);
    int pred2 = eif_logreg_multiclass_predict(&model, test2);
    int pred3 = eif_logreg_multiclass_predict(&model, test3);
    
    return pred1 == 0 && pred2 == 1 && pred3 == 2;
}

// ============================================================================
// Decision Tree Tests
// ============================================================================

static bool test_dtree_init(void) {
    eif_dtree_t tree;
    eif_status_t status = eif_dtree_init(&tree, EIF_DTREE_CLASSIFICATION, 
                                          5, 2, 1, 3, 2, &pool);
    
    if (status != EIF_STATUS_OK) return false;
    if (tree.max_depth != 5) return false;
    if (tree.num_features != 3) return false;
    
    return true;
}

static bool test_dtree_classification(void) {
    // XOR-like pattern
    float32_t X[8] = {
        0.0f, 0.0f,   // class 0
        1.0f, 1.0f,   // class 0
        0.0f, 1.0f,   // class 1
        1.0f, 0.0f    // class 1
    };
    float32_t y[4] = {0, 0, 1, 1};
    
    eif_dtree_t tree;
    eif_dtree_init(&tree, EIF_DTREE_CLASSIFICATION, 5, 2, 1, 2, 2, &pool);
    eif_dtree_fit(&tree, X, y, 4, &pool);
    
    // Should be able to solve XOR
    float32_t test1[2] = {0.1f, 0.1f};
    float32_t test2[2] = {0.9f, 0.1f};
    
    int pred1 = eif_dtree_predict_class(&tree, test1);
    int pred2 = eif_dtree_predict_class(&tree, test2);
    
    return pred1 == 0 && pred2 == 1;
}

static bool test_dtree_regression(void) {
    // Simple linear data: y = 2*x
    float32_t X[8] = {1.0f, 0.0f, 2.0f, 0.0f, 3.0f, 0.0f, 4.0f, 0.0f};
    float32_t y[4] = {2.0f, 4.0f, 6.0f, 8.0f};
    
    eif_dtree_t tree;
    eif_dtree_init(&tree, EIF_DTREE_REGRESSION, 5, 2, 1, 2, 0, &pool);
    eif_dtree_fit(&tree, X, y, 4, &pool);
    
    // Prediction should be close to training data
    float32_t test[2] = {2.5f, 0.0f};
    float32_t pred = eif_dtree_predict(&tree, test);
    
    // Should be around 5.0
    return pred > 3.0f && pred < 7.0f;
}

// ============================================================================
// Time Series Tests
// ============================================================================

static bool test_ts_moving_average(void) {
    float32_t data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    float32_t output[10];
    
    eif_status_t status = eif_ts_moving_average(data, 10, 3, output);
    
    if (status != EIF_STATUS_OK) return false;
    
    // Check smoothing effect
    return output[4] == 5.0f; // (4+5+6)/3 = 5
}

static bool test_ts_decompose(void) {
    // Seasonal data: trend + seasonal
    float32_t data[24];
    for (int i = 0; i < 24; i++) {
        data[i] = 10.0f + 0.5f * i + 2.0f * sinf(2 * 3.14159f * i / 6.0f);
    }
    
    eif_ts_decomposition_t result;
    eif_status_t status = eif_ts_decompose(data, 24, 6, &result, &pool);
    
    if (status != EIF_STATUS_OK) return false;
    if (result.trend == NULL) return false;
    if (result.seasonal == NULL) return false;
    
    return true;
}

static bool test_changepoint_detect(void) {
    // Data with clear change at index 25
    float32_t data[50];
    for (int i = 0; i < 25; i++) data[i] = 10.0f;
    for (int i = 25; i < 50; i++) data[i] = 20.0f;
    
    int change_points[10];
    int num_changes;
    
    eif_status_t status = eif_changepoint_detect(data, 50, 3.0f, 
                                                   change_points, &num_changes, 10);
    
    if (status != EIF_STATUS_OK) return false;
    
    // Should detect at least one change point near index 25
    return num_changes >= 1;
}

static bool test_ts_acf(void) {
    float32_t data[30];
    for (int i = 0; i < 30; i++) {
        data[i] = (float)i + ((float)rand() / RAND_MAX - 0.5f);
    }
    
    float32_t acf[11];
    eif_status_t status = eif_ts_acf(data, 30, acf, 10);
    
    if (status != EIF_STATUS_OK) return false;
    
    // ACF at lag 0 should be 1.0
    return fabsf(acf[0] - 1.0f) < 0.01f;
}

static bool test_correlation_pearson(void) {
    // Perfect positive correlation
    float32_t x[5] = {1, 2, 3, 4, 5};
    float32_t y[5] = {2, 4, 6, 8, 10};
    
    float32_t r = eif_correlation_pearson(x, y, 5);
    
    return fabsf(r - 1.0f) < 0.001f;
}

// ============================================================================
// Random Forest Tests
// ============================================================================

static bool test_rforest_init(void) {
    eif_rforest_t forest;
    eif_rforest_config_t config = {
        .num_trees = 5,
        .max_depth = 4,
        .max_features = 0,  // auto (sqrt)
        .min_samples_split = 2,
        .sample_ratio = 1.0f
    };
    
    eif_status_t status = eif_rforest_init(&forest, &config, 
                                            EIF_DTREE_CLASSIFICATION, 3, 2, &pool);
    
    if (status != EIF_STATUS_OK) return false;
    if (forest.config.num_trees != 5) return false;
    
    return true;
}

static bool test_rforest_classification(void) {
    // Simple classification data
    float32_t X[16] = {
        0.0f, 0.0f,  0.1f, 0.1f,  0.2f, 0.0f,  0.0f, 0.2f,  // class 0
        1.0f, 1.0f,  0.9f, 0.9f,  1.0f, 0.8f,  0.8f, 1.0f   // class 1
    };
    float32_t y[8] = {0, 0, 0, 0, 1, 1, 1, 1};
    
    eif_rforest_t forest;
    eif_rforest_config_t config = {
        .num_trees = 10,
        .max_depth = 3,
        .max_features = 0,
        .min_samples_split = 2,
        .sample_ratio = 1.0f
    };
    
    eif_rforest_init(&forest, &config, EIF_DTREE_CLASSIFICATION, 2, 2, &pool);
    eif_rforest_fit(&forest, X, y, 8, &pool);
    
    // Test predictions
    float32_t test1[2] = {0.1f, 0.1f};
    float32_t test2[2] = {0.9f, 0.9f};
    
    int pred1 = eif_rforest_predict_class(&forest, test1);
    int pred2 = eif_rforest_predict_class(&forest, test2);
    
    return pred1 == 0 && pred2 == 1;
}

// ============================================================================
// Naive Bayes Tests
// ============================================================================

static bool test_nb_init(void) {
    eif_naive_bayes_t model;
    eif_status_t status = eif_nb_init(&model, 3, 2, &pool);
    
    if (status != EIF_STATUS_OK) return false;
    if (model.num_features != 3) return false;
    if (model.num_classes != 2) return false;
    
    return true;
}

static bool test_nb_fit_predict(void) {
    // Simple 2-class data
    float32_t X[16] = {
        1.0f, 2.0f,  1.5f, 1.8f,  1.2f, 2.2f,  0.8f, 1.9f,  // class 0
        5.0f, 6.0f,  5.5f, 5.8f,  5.2f, 6.2f,  4.8f, 5.9f   // class 1
    };
    int y[8] = {0, 0, 0, 0, 1, 1, 1, 1};
    
    eif_naive_bayes_t model;
    eif_nb_init(&model, 2, 2, &pool);
    eif_nb_fit(&model, X, y, 8);
    
    // Test predictions
    float32_t test1[2] = {1.0f, 2.0f};
    float32_t test2[2] = {5.0f, 6.0f};
    
    int pred1 = eif_nb_predict(&model, test1);
    int pred2 = eif_nb_predict(&model, test2);
    
    return pred1 == 0 && pred2 == 1;
}

static bool test_nb_predict_proba(void) {
    float32_t X[8] = {
        1.0f, 1.0f,  1.2f, 0.8f,  // class 0
        5.0f, 5.0f,  5.2f, 4.8f   // class 1
    };
    int y[4] = {0, 0, 1, 1};
    
    eif_naive_bayes_t model;
    eif_nb_init(&model, 2, 2, &pool);
    eif_nb_fit(&model, X, y, 4);
    
    float32_t proba[2];
    float32_t test[2] = {1.1f, 0.9f};
    
    eif_status_t status = eif_nb_predict_proba(&model, test, proba);
    if (status != EIF_STATUS_OK) return false;
    
    // Probabilities should sum to 1
    float32_t sum = proba[0] + proba[1];
    if (fabsf(sum - 1.0f) > 0.01f) return false;
    
    // Class 0 should have higher probability for this test point
    return proba[0] > proba[1];
}

// ============================================================================
// DBSCAN Tests
// ============================================================================

static bool test_dbscan_basic(void) {
    // Two clear clusters with noise
    float32_t data[20] = {
        // Cluster 0: around (0,0)
        0.0f, 0.0f,
        0.1f, 0.1f,
        -0.1f, 0.1f,
        0.0f, 0.2f,
        // Cluster 1: around (5,5)
        5.0f, 5.0f,
        5.1f, 5.0f,
        5.0f, 5.1f,
        5.1f, 5.1f,
        // Noise
        10.0f, 10.0f,
        -10.0f, -10.0f
    };
    
    eif_dbscan_result_t result;
    eif_status_t status = eif_dbscan_compute(data, 10, 2, 0.5f, 2, &result, &pool);
    
    if (status != EIF_STATUS_OK) return false;
    if (result.num_clusters != 2) return false;
    if (result.num_noise != 2) return false;
    
    // Check cluster labels
    if (result.labels[0] != result.labels[1]) return false;  // Same cluster
    if (result.labels[4] != result.labels[5]) return false;  // Same cluster
    if (result.labels[0] == result.labels[4]) return false;  // Different clusters
    if (result.labels[8] != EIF_DBSCAN_NOISE) return false;  // Noise
    if (result.labels[9] != EIF_DBSCAN_NOISE) return false;  // Noise
    
    return true;
}

static bool test_dbscan_predict(void) {
    float32_t data[8] = {
        0.0f, 0.0f,
        0.1f, 0.1f,
        5.0f, 5.0f,
        5.1f, 5.1f
    };
    
    eif_dbscan_result_t result;
    eif_dbscan_compute(data, 4, 2, 0.5f, 2, &result, &pool);
    
    // Predict new points
    float32_t point1[2] = {0.05f, 0.05f};  // Should be cluster 0
    float32_t point2[2] = {5.05f, 5.05f};  // Should be cluster 1
    float32_t point3[2] = {100.0f, 100.0f}; // Should be noise
    
    int pred1 = eif_dbscan_predict(data, &result, 2, 0.5f, point1);
    int pred2 = eif_dbscan_predict(data, &result, 2, 0.5f, point2);
    int pred3 = eif_dbscan_predict(data, &result, 2, 0.5f, point3);
    
    if (pred1 != result.labels[0]) return false;
    if (pred2 != result.labels[2]) return false;
    if (pred3 != EIF_DBSCAN_NOISE) return false;
    
    return true;
}

// ============================================================================
// SVM Tests
// ============================================================================

static bool test_svm_init(void) {
    eif_svm_t svm;
    if (eif_svm_init(&svm, 4, 0.01f, &pool) != EIF_STATUS_OK) return false;
    if (svm.num_features != 4) return false;
    if (svm.weights == NULL) return false;
    return true;
}

static bool test_svm_binary(void) {
    eif_svm_t svm;
    eif_svm_init(&svm, 2, 0.01f, &pool);
    
    // Simple linearly separable data
    float32_t X[] = {
        0.0f, 0.0f,
        0.1f, 0.1f,
        1.0f, 1.0f,
        1.1f, 1.1f
    };
    int y[] = {0, 0, 1, 1};
    
    eif_svm_fit(&svm, X, y, 4, 100);
    
    // Test predictions
    float32_t p1[] = {0.05f, 0.05f};
    float32_t p2[] = {1.05f, 1.05f};
    
    if (eif_svm_predict(&svm, p1) != 0) return false;
    if (eif_svm_predict(&svm, p2) != 1) return false;
    
    return true;
}

// ============================================================================
// AdaBoost Tests
// ============================================================================

static bool test_adaboost_init(void) {
    eif_adaboost_t model;
    if (eif_adaboost_init(&model, 10, 4, &pool) != EIF_STATUS_OK) return false;
    if (model.num_estimators != 10) return false;
    if (model.stumps == NULL) return false;
    if (model.alphas == NULL) return false;
    return true;
}

static bool test_adaboost_fit(void) {
    eif_adaboost_t model;
    eif_adaboost_init(&model, 5, 2, &pool);
    
    // Simple binary classification data
    float32_t X[] = {
        0.0f, 0.0f,
        0.1f, 0.2f,
        0.2f, 0.1f,
        1.0f, 1.0f,
        1.1f, 1.2f,
        1.2f, 1.1f
    };
    int y[] = {0, 0, 0, 1, 1, 1};
    
    eif_adaboost_fit(&model, X, y, 6, &pool);
    
    if (model.num_fitted != 5) return false;
    
    // Test predictions
    float32_t p1[] = {0.05f, 0.05f};
    float32_t p2[] = {1.05f, 1.05f};
    
    if (eif_adaboost_predict(&model, p1) != 0) return false;
    if (eif_adaboost_predict(&model, p2) != 1) return false;
    
    return true;
}

// ============================================================================
// Mini-Batch K-Means Tests
// ============================================================================

static bool test_minibatch_kmeans(void) {
    eif_minibatch_kmeans_t model;
    if (eif_minibatch_kmeans_init(&model, 2, 2, 4, &pool) != EIF_STATUS_OK) return false;
    
    // First batch (initializes centroids)
    float32_t batch1[] = {
        0.0f, 0.0f,
        0.1f, 0.1f,
        5.0f, 5.0f,
        5.1f, 5.1f
    };
    eif_minibatch_kmeans_partial_fit(&model, batch1, 4);
    
    // Second batch (updates centroids)
    float32_t batch2[] = {
        0.2f, 0.2f,
        5.2f, 5.2f
    };
    eif_minibatch_kmeans_partial_fit(&model, batch2, 2);
    
    // Predict
    float32_t p1[] = {0.05f, 0.05f};
    float32_t p2[] = {5.05f, 5.05f};
    
    int c1 = eif_minibatch_kmeans_predict(&model, p1);
    int c2 = eif_minibatch_kmeans_predict(&model, p2);
    
    // They should be in different clusters
    if (c1 == c2) return false;
    
    return true;
}

// ============================================================================
// Main
// ============================================================================

int run_ml_algorithms_tests(void) {
    printf("=== ML Algorithms Tests ===\n");
    
    srand(42);
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Isolation Forest
    TEST(test_iforest_init);
    TEST(test_iforest_fit_score);
    
    // Logistic Regression
    TEST(test_logreg_init);
    TEST(test_logreg_binary);
    TEST(test_logreg_multiclass);
    
    // Decision Tree
    TEST(test_dtree_init);
    TEST(test_dtree_classification);
    TEST(test_dtree_regression);
    
    // Random Forest
    TEST(test_rforest_init);
    TEST(test_rforest_classification);
    
    // Naive Bayes
    TEST(test_nb_init);
    TEST(test_nb_fit_predict);
    TEST(test_nb_predict_proba);
    
    // DBSCAN
    TEST(test_dbscan_basic);
    TEST(test_dbscan_predict);
    
    // SVM
    TEST(test_svm_init);
    TEST(test_svm_binary);
    
    // AdaBoost  
    TEST(test_adaboost_init);
    TEST(test_adaboost_fit);
    
    // Mini-Batch K-Means
    TEST(test_minibatch_kmeans);
    
    // Time Series
    TEST(test_ts_moving_average);
    TEST(test_ts_decompose);
    TEST(test_changepoint_detect);
    TEST(test_ts_acf);
    TEST(test_correlation_pearson);
    
    printf("Results: %d Run, %d Passed, %d Failed\n", 
           tests_run, tests_passed, tests_run - tests_passed);
    
    return tests_run - tests_passed;
}


