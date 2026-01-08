/**
 * @file test_rf.c
 * @brief Random Forest Classifier Test
 */

#include "eif_memory.h"
#include "eif_ml_rf.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define TEST_POOL_SIZE (512 * 1024) // 512KB

// Iris dataset (simplified - 3 classes, 4 features)
static const float32_t iris_data[] = {
    // Setosa (class 0)
    5.1f, 3.5f, 1.4f, 0.2f, 4.9f, 3.0f, 1.4f, 0.2f, 4.7f, 3.2f, 1.3f, 0.2f,
    4.6f, 3.1f, 1.5f, 0.2f, 5.0f, 3.6f, 1.4f, 0.2f, 5.4f, 3.9f, 1.7f, 0.4f,
    4.6f, 3.4f, 1.4f, 0.3f, 5.0f, 3.4f, 1.5f, 0.2f, 4.4f, 2.9f, 1.4f, 0.2f,
    4.9f, 3.1f, 1.5f, 0.1f,
    // Versicolor (class 1)
    7.0f, 3.2f, 4.7f, 1.4f, 6.4f, 3.2f, 4.5f, 1.5f, 6.9f, 3.1f, 4.9f, 1.5f,
    5.5f, 2.3f, 4.0f, 1.3f, 6.5f, 2.8f, 4.6f, 1.5f, 5.7f, 2.8f, 4.5f, 1.3f,
    6.3f, 3.3f, 4.7f, 1.6f, 4.9f, 2.4f, 3.3f, 1.0f, 6.6f, 2.9f, 4.6f, 1.3f,
    5.2f, 2.7f, 3.9f, 1.4f,
    // Virginica (class 2)
    6.3f, 3.3f, 6.0f, 2.5f, 5.8f, 2.7f, 5.1f, 1.9f, 7.1f, 3.0f, 5.9f, 2.1f,
    6.3f, 2.9f, 5.6f, 1.8f, 6.5f, 3.0f, 5.8f, 2.2f, 7.6f, 3.0f, 6.6f, 2.1f,
    4.9f, 2.5f, 4.5f, 1.7f, 7.3f, 2.9f, 6.3f, 1.8f, 6.7f, 2.5f, 5.8f, 1.8f,
    7.2f, 3.6f, 6.1f, 2.5f,
};

static const uint16_t iris_labels[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Setosa
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // Versicolor
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // Virginica
};

#define NUM_SAMPLES 30
#define NUM_FEATURES 4
#define NUM_CLASSES 3
#define NUM_TRAIN 24
#define NUM_TEST 6

static void print_test_header(const char *test_name) {
  printf("\n=== %s ===\n", test_name);
}

static void print_test_result(const char *test_name, int passed) {
  printf("[%s] %s\n", passed ? "PASS" : "FAIL", test_name);
}

int run_rf_tests(void) {
  printf("EIF Random Forest Classifier Test\n");
  printf("==================================\n");

  // Initialize memory pool
  void *pool_memory = malloc(TEST_POOL_SIZE);
  if (!pool_memory) {
    printf("Failed to allocate memory pool\n");
    return 1;
  }

  eif_memory_pool_t pool;
  eif_status_t status = eif_memory_pool_init(&pool, pool_memory, TEST_POOL_SIZE);
  if (status != EIF_STATUS_OK) {
    printf("Failed to initialize memory pool\n");
    free(pool_memory);
    return 1;
  }

  // Test 1: Initialization
  print_test_header("Test 1: Random Forest Initialization");
  eif_rf_t rf;
  status = eif_rf_init(&rf, 10, 5, 2, 1, NUM_FEATURES, NUM_CLASSES, &pool);
  print_test_result("Initialization", status == EIF_STATUS_OK);
  if (status != EIF_STATUS_OK) {
    printf("Failed to initialize Random Forest: %d\n", status);
    free(pool_memory);
    return 1;
  }
  printf("  Trees: %d, Max Depth: %d, Features: %d, Classes: %d\n", rf.num_trees,
         rf.max_depth, rf.num_features, rf.num_classes);

  // Test 2: Training
  print_test_header("Test 2: Training on Iris Dataset");
  clock_t start = clock();
  status = eif_rf_fit(&rf, iris_data, iris_labels, NUM_TRAIN, &pool);
  clock_t end = clock();
  double train_time = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;

  print_test_result("Training", status == EIF_STATUS_OK);
  if (status != EIF_STATUS_OK) {
    printf("Failed to train Random Forest: %d\n", status);
    free(pool_memory);
    return 1;
  }
  printf("  Training time: %.2f ms\n", train_time);
  printf("  Memory used: %zu / %zu bytes (%.1f%%)\n", pool.used, pool.size,
         100.0 * pool.used / pool.size);

  // Test 3: Single Prediction
  print_test_header("Test 3: Single Sample Prediction");
  uint16_t pred;
  const float32_t test_sample[] = {5.1f, 3.5f, 1.4f, 0.2f}; // Setosa
  status = eif_rf_predict(&rf, test_sample, &pred);
  print_test_result("Single prediction", status == EIF_STATUS_OK && pred == 0);
  printf("  Input: [%.1f, %.1f, %.1f, %.1f]\n", test_sample[0], test_sample[1],
         test_sample[2], test_sample[3]);
  printf("  Predicted class: %d (expected: 0 - Setosa)\n", pred);

  // Test 4: Probability Prediction
  print_test_header("Test 4: Class Probability Prediction");
  float32_t probs[NUM_CLASSES];
  status = eif_rf_predict_proba(&rf, test_sample, probs);
  print_test_result("Probability prediction", status == EIF_STATUS_OK);
  printf("  Class probabilities:\n");
  for (int i = 0; i < NUM_CLASSES; i++) {
    printf("    Class %d: %.3f\n", i, probs[i]);
  }

  // Test 5: Batch Prediction
  print_test_header("Test 5: Batch Prediction on Test Set");
  const float32_t test_data[] = {
      4.9f, 3.1f, 1.5f, 0.1f, // Setosa
      5.4f, 3.9f, 1.7f, 0.4f, // Setosa
      6.4f, 3.2f, 4.5f, 1.5f, // Versicolor
      5.7f, 2.8f, 4.5f, 1.3f, // Versicolor
      6.3f, 3.3f, 6.0f, 2.5f, // Virginica
      7.2f, 3.6f, 6.1f, 2.5f, // Virginica
  };
  const uint16_t test_labels[] = {0, 0, 1, 1, 2, 2};

  uint16_t predictions[NUM_TEST];
  start = clock();
  status = eif_rf_predict_batch(&rf, test_data, NUM_TEST, predictions);
  end = clock();
  double pred_time = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;

  print_test_result("Batch prediction", status == EIF_STATUS_OK);
  printf("  Prediction time: %.2f ms (%.2f ms per sample)\n", pred_time,
         pred_time / NUM_TEST);

  // Calculate accuracy
  int correct = 0;
  printf("  Predictions:\n");
  for (int i = 0; i < NUM_TEST; i++) {
    printf("    Sample %d: predicted=%d, actual=%d %s\n", i, predictions[i],
           test_labels[i],
           predictions[i] == test_labels[i] ? "✓" : "✗");
    if (predictions[i] == test_labels[i]) {
      correct++;
    }
  }
  float32_t accuracy = (float32_t)correct / (float32_t)NUM_TEST;
  printf("  Accuracy: %.1f%% (%d/%d)\n", accuracy * 100.0f, correct, NUM_TEST);

  // Test 6: Different Classes
  print_test_header("Test 6: Testing All Three Classes");
  const float32_t class_samples[][NUM_FEATURES] = {
      {5.0f, 3.4f, 1.5f, 0.2f}, // Setosa
      {6.5f, 2.8f, 4.6f, 1.5f}, // Versicolor
      {7.1f, 3.0f, 5.9f, 2.1f}, // Virginica
  };
  const char *class_names[] = {"Setosa", "Versicolor", "Virginica"};

  int all_correct = 1;
  for (int c = 0; c < NUM_CLASSES; c++) {
    status = eif_rf_predict(&rf, class_samples[c], &pred);
    status = eif_rf_predict_proba(&rf, class_samples[c], probs);

    printf("  %s sample:\n", class_names[c]);
    printf("    Predicted: %s (class %d)\n", class_names[pred], pred);
    printf("    Confidence: %.1f%%\n", probs[pred] * 100.0f);
    printf("    Result: %s\n", pred == c ? "✓ CORRECT" : "✗ WRONG");

    if (pred != c)
      all_correct = 0;
  }
  print_test_result("Multi-class prediction", all_correct);

  // Summary
  printf("\n=================================\n");
  printf("Random Forest Test Summary\n");
  printf("=================================\n");
  printf("Configuration:\n");
  printf("  - Trees: %d\n", rf.num_trees);
  printf("  - Max Depth: %d\n", rf.max_depth);
  printf("  - Features: %d\n", rf.num_features);
  printf("  - Classes: %d\n", rf.num_classes);
  printf("  - Training samples: %d\n", NUM_TRAIN);
  printf("  - Test samples: %d\n", NUM_TEST);
  printf("\nPerformance:\n");
  printf("  - Training time: %.2f ms\n", train_time);
  printf("  - Avg prediction time: %.3f ms\n", pred_time / NUM_TEST);
  printf("  - Test accuracy: %.1f%%\n", accuracy * 100.0f);
  printf("  - Memory usage: %zu bytes\n", pool.used);
  printf("\n✓ All tests passed!\n\n");

  free(pool_memory);
  return 0;
}
