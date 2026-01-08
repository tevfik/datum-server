#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../framework/eif_test_runner.h"

#include "../../ml/include/eif_ml_kmeans_fixed.h"
#include "../../nlp/include/eif_nlp_embedding_fixed.h"

// =============================================================================
// ML: K-Means Fixed Point Tests (Q15)
// =============================================================================
bool test_kmeans_fixed_basic(void) {
    // 2D Data, 2 Clusters
    // Cluster 1: Near 0.1 (Q15 ~ 3276)
    // Cluster 2: Near 0.9 (Q15 ~ 29491)
    
    // Q15 Format: 1.0 = 32768 (strictly 32767 is max, so we use slightly less)
    
    q15_t data[] = {
        EIF_FLOAT_TO_Q15(0.1f), EIF_FLOAT_TO_Q15(0.1f),
        EIF_FLOAT_TO_Q15(0.2f), EIF_FLOAT_TO_Q15(0.0f),
        EIF_FLOAT_TO_Q15(0.8f), EIF_FLOAT_TO_Q15(0.8f),
        EIF_FLOAT_TO_Q15(0.9f), EIF_FLOAT_TO_Q15(0.9f),
    };
    
    int num_samples = 4;
    int dim = 2;
    int k = 2;
    
    eif_kmeans_fixed_t km = {0};
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_kmeans_init_fixed(&km, k, dim));
    
    // Fit
    int iters = eif_kmeans_fit_fixed(&km, data, num_samples);
    TEST_ASSERT_TRUE(iters > 0);
    
    // Predict
    q15_t sample1[] = {EIF_FLOAT_TO_Q15(0.15f), EIF_FLOAT_TO_Q15(0.15f)};
    int p1 = eif_kmeans_predict_fixed(&km, sample1);
    
    q15_t sample2[] = {EIF_FLOAT_TO_Q15(0.85f), EIF_FLOAT_TO_Q15(0.85f)};
    int p2 = eif_kmeans_predict_fixed(&km, sample2);
    
    TEST_ASSERT_TRUE(p1 != p2);
    
    eif_kmeans_free_fixed(&km);
    return true;
}

// =============================================================================
// NLP: Embedding Fixed Point Tests (Q7)
// =============================================================================
bool test_embedding_fixed_basic(void) {
    eif_embedding_fixed_t emb;
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_word_embedding_init_fixed(&emb, 10, 3));
    
    // Vectors [-1.0, 1.0] -> Q7 [-128, 127]
    float v_king[] = {1.0f, 0.0f, 0.0f};
    float v_queen[] = {1.0f, 0.1f, 0.0f}; // Close
    float v_apple[] = {0.0f, 1.0f, 1.0f}; // Far
    
    eif_embedding_add_fixed(&emb, "king", v_king);
    eif_embedding_add_fixed(&emb, "queen", v_queen);
    eif_embedding_add_fixed(&emb, "apple", v_apple);
    
    // Check quantization quality
    const q7_t* vec_q7 = eif_embedding_get_fixed(&emb, "king");
    TEST_ASSERT_NOT_NULL(vec_q7);
    TEST_ASSERT_EQUAL_INT(127, vec_q7[0]); // 1.0 -> 127
    TEST_ASSERT_EQUAL_INT(0, vec_q7[1]);
    
    // Similarity
    // Using Q7 dot product
    float sim1 = eif_embedding_similarity_fixed(&emb, "king", "queen");
    float sim2 = eif_embedding_similarity_fixed(&emb, "king", "apple");
    
    // Q7 precision loss is expected, but relative order should hold
    TEST_ASSERT_TRUE(sim1 > 0.8f);
    TEST_ASSERT_TRUE(sim2 < 0.5f);
    
    eif_word_embedding_free_fixed(&emb);
    return true;
}

#include "../../ml/include/eif_ml_gradient_boost_fixed.h"
#include "../../ml/include/eif_time_series_fixed.h"
#include "../../ml/include/eif_adaptive_threshold_fixed.h"

// =============================================================================
// ML: Gradient Boosting Fixed Point Tests
// =============================================================================
bool test_gbm_fixed_basic(void) {
    // 1 Tree, depth 1 (Stump).
    // Feature 0 <= 0.5 -> Left (+1000)
    // Feature 0 > 0.5 -> Right (-1000)
    
    eif_gbm_node_fixed_t nodes[] = {
        {0, EIF_FLOAT_TO_Q15(0.5f), 1, 2, 0},   // Root
        {-1, 0, 0, 0, 1000},                    // Left Leaf
        {-1, 0, 0, 0, -1000}                    // Right Leaf
    };
    
    eif_gbm_tree_fixed_t tree = {nodes, 3, 0};
    
    // GBM with 1 tree, base_score = 0
    eif_gbm_fixed_t gbm;
    eif_gbm_init_fixed(&gbm, 1, 2, &tree, 0);
    
    // Test Left branch
    q15_t in_left[] = {EIF_FLOAT_TO_Q15(0.2f)};
    q15_t out_left = eif_gbm_predict_fixed(&gbm, in_left);
    TEST_ASSERT_EQUAL_INT(1000, out_left);
    
    // Test Right branch
    q15_t in_right[] = {EIF_FLOAT_TO_Q15(0.8f)};
    q15_t out_right = eif_gbm_predict_fixed(&gbm, in_right);
    TEST_ASSERT_EQUAL_INT(-1000, out_right);
    
    return true;
}

// =============================================================================
// Time Series Fixed Point Tests
// =============================================================================
bool test_ts_fixed_basic(void) {
    // SES Test
    // Alpha 0.5
    // Series: 1000, 1000, 1000 -> Should converge to 1000
    
    eif_ses_fixed_t ses;
    eif_ses_init_fixed(&ses, EIF_FLOAT_TO_Q15(0.5f));
    
    q15_t val1 = eif_ses_update_fixed(&ses, 1000);
    TEST_ASSERT_EQUAL_INT(1000, val1); // First value init
    
    q15_t val2 = eif_ses_update_fixed(&ses, 1200);
    // 0.5*1200 + 0.5*1000 = 1100
    TEST_ASSERT_TRUE(abs(val2 - 1100) < 5);
    
    // SMA Test
    q15_t buf[3];
    eif_sma_fixed_t sma;
    eif_sma_init_fixed(&sma, buf, 3);
    
    eif_sma_update_fixed(&sma, 100);
    eif_sma_update_fixed(&sma, 200);
    q15_t avg = eif_sma_update_fixed(&sma, 300);
    
    // (100+200+300)/3 = 200
    TEST_ASSERT_EQUAL_INT(200, avg);
    
    return true;
}

// =============================================================================
// Adaptive Threshold Fixed Point Tests
// =============================================================================
bool test_adapt_fixed_basic(void) {
    // Noisy signal centered at 1000
    // Noise +/- 50
    // Anomaly 2000
    
    eif_adaptive_threshold_fixed_t at;
    // Alpha small (0.1) to stabilize
    eif_adaptive_thresh_init_fixed(&at, EIF_FLOAT_TO_Q15(0.1f));
    
    // Train phase with noise to establish variance
    for (int i = 0; i < 40; i++) {
        // Alternating 950 and 1050
        q15_t val = (i % 2 == 0) ? 950 : 1050;
        eif_adaptive_thresh_update_fixed(&at, val);
    }
    
    // Mean should be close to 1000
    // Variance roughly 2500 (50^2)
    TEST_ASSERT_TRUE(abs(at.mean - 1000) < 50);
    
    // Check normal value within noise range
    // Factor 48 (3.0 in Q4, since 16=1.0) -> 3 sigma
    // 3 * 50 = 150 margin. Range [850, 1150]
    bool is_anom = eif_adaptive_thresh_check_fixed(&at, 1050, 48);
    TEST_ASSERT_TRUE(!is_anom);
    
    // Check anomaly way outside
    is_anom = eif_adaptive_thresh_check_fixed(&at, 2000, 48);
    TEST_ASSERT_TRUE(is_anom);
    
    return true;
}

int run_new_features_fixed_tests(void) {
    int failed = 0;
    if (!test_kmeans_fixed_basic()) { printf("test_kmeans_fixed_basic FAILED\n"); failed++; }
    if (!test_embedding_fixed_basic()) { printf("test_embedding_fixed_basic FAILED\n"); failed++; }
    if (!test_gbm_fixed_basic()) { printf("test_gbm_fixed_basic FAILED\n"); failed++; }
    if (!test_ts_fixed_basic()) { printf("test_ts_fixed_basic FAILED\n"); failed++; }
    if (!test_adapt_fixed_basic()) { printf("test_adapt_fixed_basic FAILED\n"); failed++; }
    return failed;
}
