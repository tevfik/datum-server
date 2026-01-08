#include <stdio.h>
#include <stdlib.h>
#include "../framework/eif_test_runner.h"

#include "../../ml/include/eif_ml_knn_fixed.h"
#include "../../ml/include/eif_ml_svm_fixed.h"
#include "../../ml/include/eif_ml_rf_fixed.h"
#include "../../ml/include/eif_ml_nb_fixed.h"
#include "../../ml/include/eif_ml_logistic_fixed.h"
#include "../../ml/include/eif_ml_pca_fixed.h"

// =============================================================================
// ML: K-Nearest Neighbors Fixed Point Tests
// =============================================================================
bool test_knn_fixed_basic(void) {
    // 2 Classes, 3 samples each
    // Class 0: Negative / Small numbers
    // Class 1: Positive / Large numbers
    
    q15_t train_data[] = {
        // Class 0
        EIF_FLOAT_TO_Q15(0.1f), EIF_FLOAT_TO_Q15(0.1f),
        EIF_FLOAT_TO_Q15(0.2f), EIF_FLOAT_TO_Q15(0.2f),
        EIF_FLOAT_TO_Q15(0.1f), EIF_FLOAT_TO_Q15(0.2f),
        // Class 1
        EIF_FLOAT_TO_Q15(0.8f), EIF_FLOAT_TO_Q15(0.8f),
        EIF_FLOAT_TO_Q15(0.9f), EIF_FLOAT_TO_Q15(0.9f),
        EIF_FLOAT_TO_Q15(0.8f), EIF_FLOAT_TO_Q15(0.9f)
    };
    
    int32_t train_labels[] = {
        0, 0, 0,
        1, 1, 1
    };
    
    int num_samples = 6;
    int dim = 2;
    int k = 3;
    
    eif_ml_knn_fixed_t knn;
    eif_ml_knn_init_fixed(&knn, train_data, train_labels, num_samples, dim, k);
    
    // Test Case 1: Should be Class 0
    q15_t input1[] = {EIF_FLOAT_TO_Q15(0.15f), EIF_FLOAT_TO_Q15(0.15f)};
    int32_t p1 = eif_ml_knn_predict_fixed(&knn, input1);
    TEST_ASSERT_EQUAL_INT(0, p1);
    
    // Test Case 2: Should be Class 1
    q15_t input2[] = {EIF_FLOAT_TO_Q15(0.85f), EIF_FLOAT_TO_Q15(0.85f)};
    int32_t p2 = eif_ml_knn_predict_fixed(&knn, input2);
    TEST_ASSERT_EQUAL_INT(1, p2);
    
    return true;
}

// =============================================================================
// ML: SVM Fixed Point Tests
// =============================================================================
bool test_svm_fixed_basic(void) {
    // 2 Classes, 2 Features
    // Deciding based on X-coordinate sign/magnitude
    
    // Weights for 2 classes [num_classes x num_features]
    // Class 0: Favors negative X (multiplies by -1)
    // Class 1: Favors positive X (multiplies by 1)
    q15_t weights[] = {
        EIF_FLOAT_TO_Q15(-1.0f), 0,  // Class 0
        EIF_FLOAT_TO_Q15(1.0f), 0    // Class 1
    };
    
    // Bias [num_classes] (Q31)
    q31_t bias[] = {0, 0};
    
    eif_linear_svm_fixed_t svm;
    eif_linear_svm_init_fixed(&svm, 2, 2, weights, bias);
    
    // Input Positive: (0.5, 0.5)
    // C0 score: -1.0 * 0.5 = -0.5
    // C1 score: 1.0 * 0.5 = 0.5
    // Prediction: 1
    q15_t in_pos[] = {EIF_FLOAT_TO_Q15(0.5f), EIF_FLOAT_TO_Q15(0.5f)};
    int32_t p_pos = eif_linear_svm_predict_fixed(&svm, in_pos);
    TEST_ASSERT_EQUAL_INT(1, p_pos);
    
    // Input Negative: (-0.5, 0.5)
    // C0 score: -1.0 * -0.5 = 0.5
    // C1 score: 1.0 * -0.5 = -0.5
    // Prediction: 0
    q15_t in_neg[] = {EIF_FLOAT_TO_Q15(-0.5f), EIF_FLOAT_TO_Q15(0.5f)};
    int32_t p_neg = eif_linear_svm_predict_fixed(&svm, in_neg);
    TEST_ASSERT_EQUAL_INT(0, p_neg);
    
    return true;
}

// =============================================================================
// ML: Random Forest Fixed Point Tests
// =============================================================================
bool test_rf_fixed_basic(void) {
    // Simple 1-tree forest (Decision Stump)
    // Feature 0 > 0.5 -> Class 1, else Class 0
    
    eif_rf_node_fixed_t nodes[] = {
        // Node 0: Root
        // Split feature 0, value 0.5
        {0, EIF_FLOAT_TO_Q15(0.5f), 1, 2, 0},
        
        // Node 1: Left Child (Leaf), Class 0
        {-1, 0, 0, 0, 0},
        
        // Node 2: Right Child (Leaf), Class 1
        {-1, 0, 0, 0, 1}
    };
    
    eif_rf_tree_fixed_t tree = {nodes, 3};
    eif_rf_fixed_t rf;
    eif_rf_init_fixed(&rf, 1, 2, &tree);
    
    // Case 1: Low values -> Class 0
    q15_t in_low[] = {EIF_FLOAT_TO_Q15(0.2f)};
    int32_t p_low = eif_rf_predict_fixed(&rf, in_low);
    TEST_ASSERT_EQUAL_INT(0, p_low);
    
    // Case 2: High values -> Class 1
    q15_t in_high[] = {EIF_FLOAT_TO_Q15(0.8f)};
    int32_t p_high = eif_rf_predict_fixed(&rf, in_high);
    TEST_ASSERT_EQUAL_INT(1, p_high);
    
    return true;
}

// =============================================================================
// ML: Naive Bayes Fixed Point Tests
// =============================================================================
bool test_nb_fixed_basic(void) {
    // 2 Classes, 1 Feature
    // Class 0: Mean 0.2, Var 0.01 (Tight around 0.2)
    // Class 1: Mean 0.8, Var 0.01 (Tight around 0.8)
    
    q15_t means[] = {EIF_FLOAT_TO_Q15(0.2f), EIF_FLOAT_TO_Q15(0.8f)};
    q15_t vars[] = {EIF_FLOAT_TO_Q15(0.01f), EIF_FLOAT_TO_Q15(0.01f)};
    q15_t priors[] = {0, 0}; // Equal priors
    
    eif_gaussian_nb_fixed_t nb;
    eif_gaussian_nb_init_fixed(&nb, 1, 2, means, vars, priors);
    
    // Input 0.25 (Closer to 0.2)
    q15_t in_0[] = {EIF_FLOAT_TO_Q15(0.25f)};
    int32_t p0 = eif_gaussian_nb_predict_fixed(&nb, in_0);
    TEST_ASSERT_EQUAL_INT(0, p0);
    
    // Input 0.75 (Closer to 0.8)
    q15_t in_1[] = {EIF_FLOAT_TO_Q15(0.75f)};
    int32_t p1 = eif_gaussian_nb_predict_fixed(&nb, in_1);
    TEST_ASSERT_EQUAL_INT(1, p1);
    
    return true;
}

// =============================================================================
// ML: Logistic Regression Fixed Point Tests
// =============================================================================
bool test_logistic_fixed_basic(void) {
    // Binary Classification
    // Feature 0: Positive correlation with Class 1
    // Sigmoid(w*x + b)
    
    // Weight: 2.0 (Q15 ~ 16384 * 2? No, max 1.0. Let's use 0.8)
    // 0.8 in Q15 = 26214
    q15_t weights[] = {EIF_FLOAT_TO_Q15(0.8f)};
    q31_t bias = 0;
    
    eif_binary_logistic_fixed_t lr;
    eif_binary_logistic_init_fixed(&lr, 1, weights, bias);
    
    // Input 1.0 -> dot = 0.8 -> Sigmoid(0.8) > 0.5 -> Class 1
    q15_t in_pos[] = {EIF_FLOAT_TO_Q15(1.0f)};
    int32_t p1 = eif_binary_logistic_predict_fixed(&lr, in_pos);
    TEST_ASSERT_EQUAL_INT(1, p1);
    
    // Input -1.0 -> dot = -0.8 -> Sigmoid(-0.8) < 0.5 -> Class 0
    q15_t in_neg[] = {EIF_FLOAT_TO_Q15(-1.0f)};
    int32_t p0 = eif_binary_logistic_predict_fixed(&lr, in_neg);
    TEST_ASSERT_EQUAL_INT(0, p0);
    
    return true;
}

// =============================================================================
// ML: PCA Fixed Point Tests
// =============================================================================
bool test_pca_fixed_basic(void) {
    // 2D -> 1D
    // Data along x=y line
    // PC1 should be [0.707, 0.707]
    
    q15_t mean[] = {0, 0};
    q15_t components[] = {EIF_FLOAT_TO_Q15(0.707f), EIF_FLOAT_TO_Q15(0.707f)};
    
    eif_pca_fixed_t pca;
    eif_pca_init_fixed(&pca, 2, 1, mean, components);
    
    // Input (1.0, 1.0) -> Proj = 1.0*0.707 + 1.0*0.707 = 1.414
    q15_t input[] = {EIF_FLOAT_TO_Q15(1.0f), EIF_FLOAT_TO_Q15(1.0f)};
    q15_t output[1];
    
    eif_pca_transform_fixed(&pca, input, output);
    
    // 1.414 is > 1.0, Q15 saturates at ~1.0 if we interpret 32767 as 1.0
    // But fixed point usually just wraps or saturates integer value
    // EIF_FLOAT_TO_Q15(1.414) -> Overflow if we assume range [-1, 1).
    // Our PCA implementation should perhaps handle scaling.
    // However, if we assume dot product can be large, output Q15 might clip.
    // Let's test with smaller input to stay in range.
    
    // Input (0.5, 0.5) -> 0.707
    q15_t in_small[] = {EIF_FLOAT_TO_Q15(0.5f), EIF_FLOAT_TO_Q15(0.5f)};
    eif_pca_transform_fixed(&pca, in_small, output);
    
    // Expect ~0.707 (23170)
    // 0.5*0.707 + 0.5*0.707 = 0.707
    // Q15 calc: (16384 * 23170) >> 15 = 11585. sum = 23170.
    
    int expected = EIF_FLOAT_TO_Q15(0.707f);
    int diff = abs(output[0] - expected);
    
    TEST_ASSERT_TRUE(diff < 100); // Tolerance
    
    return true;
}

int run_ml_classifiers_fixed_tests(void) {
    int failed = 0;
    if (!test_knn_fixed_basic()) { printf("test_knn_fixed_basic FAILED\n"); failed++; }
    if (!test_svm_fixed_basic()) { printf("test_svm_fixed_basic FAILED\n"); failed++; }
    if (!test_rf_fixed_basic()) { printf("test_rf_fixed_basic FAILED\n"); failed++; }
    if (!test_nb_fixed_basic()) { printf("test_nb_fixed_basic FAILED\n"); failed++; }
    if (!test_logistic_fixed_basic()) { printf("test_logistic_fixed_basic FAILED\n"); failed++; }
    if (!test_pca_fixed_basic()) { printf("test_pca_fixed_basic FAILED\n"); failed++; }
    return failed;
}
