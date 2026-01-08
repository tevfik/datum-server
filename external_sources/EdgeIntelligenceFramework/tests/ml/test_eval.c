/**
 * @file test_eval.c
 * @brief Unit tests for eif_eval.h (Evaluation metrics)
 */

#include "../framework/eif_test_runner.h"
#define EIF_HAS_PRINTF 1
#include "eif_eval.h"

// =============================================================================
// Evaluation Tests
// =============================================================================

bool test_eval_init(void) {
  eif_eval_t eval;
  eif_eval_init(&eval, 5);

  TEST_ASSERT_EQUAL_INT(5, eval.num_classes);
  TEST_ASSERT_EQUAL_INT(0, (int)eval.total_samples);
  TEST_ASSERT_EQUAL_INT(0, (int)eval.correct_samples);

  return true;
}

bool test_eval_argmax(void) {
  int16_t output[] = {100, 500, 200, 800, 300};

  int idx = eif_eval_argmax(output, 5);

  TEST_ASSERT_EQUAL_INT(3, idx); // 800 is max at index 3

  return true;
}

bool test_eval_update_correct(void) {
  eif_eval_t eval;
  eif_eval_init(&eval, 3);

  // Prediction: class 1 has highest score
  int16_t output[] = {1000, 20000, 5000};

  eif_eval_update(&eval, output, 1); // True label is 1

  TEST_ASSERT_EQUAL_INT(1, (int)eval.total_samples);
  TEST_ASSERT_EQUAL_INT(1, (int)eval.correct_samples);
  TEST_ASSERT_EQUAL_INT(1, (int)eval.true_positives[1]);

  return true;
}

bool test_eval_update_incorrect(void) {
  eif_eval_t eval;
  eif_eval_init(&eval, 3);

  // Prediction: class 1, but true label is 2
  int16_t output[] = {1000, 20000, 5000};

  eif_eval_update(&eval, output, 2);

  TEST_ASSERT_EQUAL_INT(1, (int)eval.total_samples);
  TEST_ASSERT_EQUAL_INT(0, (int)eval.correct_samples);
  TEST_ASSERT_EQUAL_INT(1, (int)eval.false_positives[1]);
  TEST_ASSERT_EQUAL_INT(1, (int)eval.false_negatives[2]);

  return true;
}

bool test_eval_accuracy(void) {
  eif_eval_t eval;
  eif_eval_init(&eval, 2);

  // 3 correct, 1 incorrect
  int16_t correct[] = {20000, 1000};
  int16_t wrong[] = {1000, 20000};

  eif_eval_update(&eval, correct, 0);
  eif_eval_update(&eval, correct, 0);
  eif_eval_update(&eval, correct, 0);
  eif_eval_update(&eval, wrong, 0); // Predicted 1, actual 0

  float acc = eif_eval_accuracy(&eval);

  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.75f, acc);

  return true;
}

bool test_eval_precision(void) {
  eif_eval_t eval;
  eif_eval_init(&eval, 2);

  // Class 0: 2 TP, 1 FP
  int16_t pred_0[] = {20000, 1000};
  int16_t pred_1[] = {1000, 20000};

  eif_eval_update(&eval, pred_0, 0); // TP for 0
  eif_eval_update(&eval, pred_0, 0); // TP for 0
  eif_eval_update(&eval, pred_0, 1); // FP for 0, FN for 1

  float precision = eif_eval_precision(&eval, 0);

  // Precision = TP / (TP + FP) = 2 / 3 = 0.667
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.667f, precision);

  return true;
}

bool test_eval_recall(void) {
  eif_eval_t eval;
  eif_eval_init(&eval, 2);

  int16_t pred_0[] = {20000, 1000};
  int16_t pred_1[] = {1000, 20000};

  // Class 0: 2 TP, 1 FN
  eif_eval_update(&eval, pred_0, 0); // TP
  eif_eval_update(&eval, pred_0, 0); // TP
  eif_eval_update(&eval, pred_1, 0); // FN (predicted 1, actual 0)

  float recall = eif_eval_recall(&eval, 0);

  // Recall = TP / (TP + FN) = 2 / 3 = 0.667
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.667f, recall);

  return true;
}

bool test_eval_f1(void) {
  eif_eval_t eval;
  eif_eval_init(&eval, 2);

  // Perfect predictions for class 0
  int16_t pred_0[] = {20000, 1000};

  eif_eval_update(&eval, pred_0, 0);
  eif_eval_update(&eval, pred_0, 0);

  float f1 = eif_eval_f1(&eval, 0);

  // Perfect precision and recall -> F1 = 1.0
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, f1);

  return true;
}

bool test_eval_confusion_matrix(void) {
  eif_eval_t eval;
  eif_eval_init(&eval, 2);

  int16_t pred_0[] = {20000, 1000};
  int16_t pred_1[] = {1000, 20000};

  eif_eval_update(&eval, pred_0, 0); // confusion[0][0]++
  eif_eval_update(&eval, pred_1, 1); // confusion[1][1]++
  eif_eval_update(&eval, pred_0, 1); // confusion[0][1]++

  TEST_ASSERT_EQUAL_INT(1, (int)eval.confusion[0][0]);
  TEST_ASSERT_EQUAL_INT(1, (int)eval.confusion[1][1]);
  TEST_ASSERT_EQUAL_INT(1, (int)eval.confusion[0][1]);
  TEST_ASSERT_EQUAL_INT(0, (int)eval.confusion[1][0]);

  return true;
}

bool test_eval_timing(void) {
  eif_eval_t eval;
  eif_eval_init(&eval, 2);

  eif_eval_update_timing(&eval, 100);
  eif_eval_update_timing(&eval, 200);
  eif_eval_update_timing(&eval, 150);

  TEST_ASSERT_EQUAL_INT(100, (int)eval.min_inference_us);
  TEST_ASSERT_EQUAL_INT(200, (int)eval.max_inference_us);
  TEST_ASSERT_EQUAL_INT(450, (int)eval.total_inference_us);

  return true;
}

// =============================================================================
// Profiler Tests
// =============================================================================

bool test_profiler_init(void) {
  eif_profiler_t prof;
  eif_profiler_init(&prof);

  TEST_ASSERT_EQUAL_INT(0, prof.num_layers);
  TEST_ASSERT_EQUAL_INT(0, (int)prof.total_inference_us);

  return true;
}

bool test_profiler_add_layer(void) {
  eif_profiler_t prof;
  eif_profiler_init(&prof);

  int idx = eif_profiler_add_layer(&prof, "conv1");

  TEST_ASSERT_EQUAL_INT(0, idx);
  TEST_ASSERT_EQUAL_INT(1, prof.num_layers);

  return true;
}

bool test_profiler_record(void) {
  eif_profiler_t prof;
  eif_profiler_init(&prof);

  int idx = eif_profiler_add_layer(&prof, "dense");

  eif_profiler_record(&prof, idx, 50);
  eif_profiler_record(&prof, idx, 100);
  eif_profiler_record(&prof, idx, 75);

  TEST_ASSERT_EQUAL_INT(3, (int)prof.layers[idx].call_count);
  TEST_ASSERT_EQUAL_INT(225, (int)prof.layers[idx].total_us);
  TEST_ASSERT_EQUAL_INT(50, (int)prof.layers[idx].min_us);
  TEST_ASSERT_EQUAL_INT(100, (int)prof.layers[idx].max_us);

  return true;
}

// =============================================================================
// Test Suite
// =============================================================================

BEGIN_TEST_SUITE(run_eval_tests)
RUN_TEST(test_eval_init);
RUN_TEST(test_eval_argmax);
RUN_TEST(test_eval_update_correct);
RUN_TEST(test_eval_update_incorrect);
RUN_TEST(test_eval_accuracy);
RUN_TEST(test_eval_precision);
RUN_TEST(test_eval_recall);
RUN_TEST(test_eval_f1);
RUN_TEST(test_eval_confusion_matrix);
RUN_TEST(test_eval_timing);
RUN_TEST(test_profiler_init);
RUN_TEST(test_profiler_add_layer);
RUN_TEST(test_profiler_record);
END_TEST_SUITE()
