/**
 * @file test_ml_classifiers.c
 * @brief Unit tests for ML classifiers (SVM, Naive Bayes, Logistic, RF, GBM)
 */

#include "../framework/eif_test_runner.h"
#include "eif_ml_gradient_boost.h"
#include "eif_ml_logistic.h"
#include "eif_ml_naive_bayes.h"
#include "eif_ml_random_forest.h"
#include "eif_ml_svm.h"
#include <math.h>

// =============================================================================
// SVM Tests
// =============================================================================

bool test_linear_svm(void) {
  eif_linear_svm_t svm;
  eif_linear_svm_init(&svm, 2, 2);

  // Simple weights for XOR-like separation
  float weights[] = {1.0f, 1.0f, -1.0f, -1.0f};
  float bias[] = {0.0f, 0.0f};
  svm.weights = weights;
  svm.bias = bias;

  // Test prediction
  float feat1[] = {1.0f, 1.0f};
  float feat2[] = {-1.0f, -1.0f};

  int pred1 = eif_linear_svm_predict(&svm, feat1);
  int pred2 = eif_linear_svm_predict(&svm, feat2);

  // Class 0 has positive weights, class 1 has negative
  TEST_ASSERT_EQUAL_INT(0, pred1);
  TEST_ASSERT_EQUAL_INT(1, pred2);

  return true;
}

bool test_rbf_kernel(void) {
  // Test RBF kernel function
  float x1[] = {0.0f, 0.0f};
  float x2[] = {0.0f, 0.0f};

  float k_same = eif_rbf_kernel(x1, x2, 2, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, k_same, 0.001f); // Same point

  float x3[] = {10.0f, 10.0f};
  float k_far = eif_rbf_kernel(x1, x3, 2, 1.0f);
  TEST_ASSERT(k_far < 0.01f); // Far points have low kernel value

  return true;
}

// =============================================================================
// Logistic Regression Tests
// =============================================================================

bool test_binary_logistic(void) {
  eif_binary_logistic_t lr;
  eif_binary_logistic_init(&lr, 2);

  float weights[] = {2.0f, -2.0f};
  lr.weights = weights;
  lr.bias = 0.0f;

  // Positive case
  float feat_pos[] = {1.0f, 0.0f};
  float prob_pos = eif_binary_logistic_proba(&lr, feat_pos);
  TEST_ASSERT(prob_pos > 0.5f);

  // Negative case
  float feat_neg[] = {0.0f, 1.0f};
  float prob_neg = eif_binary_logistic_proba(&lr, feat_neg);
  TEST_ASSERT(prob_neg < 0.5f);

  return true;
}

bool test_softmax_regression(void) {
  eif_softmax_regression_t sr;
  eif_softmax_regression_init(&sr, 2, 3);

  // Identity-like weights
  float weights[] = {
      1.0f, 0.0f, // Class 0 prefers feature 0
      0.0f, 1.0f, // Class 1 prefers feature 1
      0.0f, 0.0f  // Class 2 neutral
  };
  float bias[] = {0.0f, 0.0f, 0.0f};
  sr.weights = weights;
  sr.bias = bias;

  float feat[] = {2.0f, 0.0f};
  float probs[3];
  eif_softmax_regression_proba(&sr, feat, probs);

  // Class 0 should have highest probability
  TEST_ASSERT(probs[0] > probs[1]);
  TEST_ASSERT(probs[0] > probs[2]);

  return true;
}

// =============================================================================
// Random Forest Tests
// =============================================================================

bool test_decision_tree(void) {
  eif_decision_tree_t tree;
  tree.num_nodes = 0;
  tree.root = 0;

  // Build simple tree: if x[0] <= 0.5 then class 0, else class 1
  int root = eif_dt_add_node(&tree, 0, 0.5f, false, -1);
  int left = eif_dt_add_node(&tree, -1, 0.0f, true, 0);
  int right = eif_dt_add_node(&tree, -1, 0.0f, true, 1);
  eif_dt_set_children(&tree, root, left, right);

  float low[] = {0.3f, 0.0f};
  float high[] = {0.7f, 0.0f};

  TEST_ASSERT_EQUAL_INT(0, eif_dt_predict(&tree, low));
  TEST_ASSERT_EQUAL_INT(1, eif_dt_predict(&tree, high));

  return true;
}

bool test_random_forest(void) {
  eif_random_forest_t rf;
  eif_rf_create_example(&rf, 2, 2);

  TEST_ASSERT_EQUAL_INT(3, rf.num_trees);
  TEST_ASSERT_EQUAL_INT(2, rf.num_features);

  // Test prediction
  float features[] = {0.8f, 0.8f}; // Should be class 1 (high values)
  int pred = eif_rf_predict(&rf, features);
  TEST_ASSERT_EQUAL_INT(1, pred);

  return true;
}

// =============================================================================
// Gradient Boosting Tests
// =============================================================================

bool test_gbm_tree(void) {
  eif_gbm_tree_t tree;
  tree.num_nodes = 0;
  tree.root = 0;

  int root = eif_gbm_add_node(&tree, 0, 0.5f, false, 0.0f);
  int left = eif_gbm_add_node(&tree, -1, 0.0f, true, -1.0f);
  int right = eif_gbm_add_node(&tree, -1, 0.0f, true, 1.0f);
  eif_gbm_set_children(&tree, root, left, right);

  float low[] = {0.3f};
  float high[] = {0.7f};

  TEST_ASSERT_EQUAL_FLOAT(-1.0f, eif_gbm_tree_predict(&tree, low), 0.001f);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, eif_gbm_tree_predict(&tree, high), 0.001f);

  return true;
}

bool test_gradient_boosting(void) {
  eif_gradient_boost_t gbm;
  eif_gbm_create_example(&gbm, 2);

  TEST_ASSERT_EQUAL_INT(3, gbm.num_trees);

  // Test prediction
  float high[] = {0.9f, 0.9f};
  float prob = eif_gbm_predict_proba_binary(&gbm, high);
  TEST_ASSERT(prob > 0.5f); // High values should give class 1

  return true;
}

BEGIN_TEST_SUITE(run_ml_classifier_tests)
RUN_TEST(test_linear_svm);
RUN_TEST(test_rbf_kernel);
RUN_TEST(test_binary_logistic);
RUN_TEST(test_softmax_regression);
RUN_TEST(test_decision_tree);
RUN_TEST(test_random_forest);
RUN_TEST(test_gbm_tree);
RUN_TEST(test_gradient_boosting);
END_TEST_SUITE()
