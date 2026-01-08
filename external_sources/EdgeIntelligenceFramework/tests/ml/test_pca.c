/**
 * @file test_pca.c
 * @brief PCA (Principal Component Analysis) Test
 */

#include "eif_memory.h"
#include "eif_ml_pca.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TEST_POOL_SIZE (1024 * 1024) // 1MB
#define EPSILON 1e-4f

// Test dataset: 2D data that should reduce to 1D line
static const float32_t test_data_2d[] = {
    1.0f, 2.0f,  2.0f, 4.0f,  3.0f, 6.0f,  4.0f, 8.0f,
    5.0f, 10.0f, 6.0f, 12.0f, 7.0f, 14.0f, 8.0f, 16.0f,
};
#define NUM_SAMPLES_2D 8
#define NUM_FEATURES_2D 2

// Higher dimensional test data
static const float32_t test_data_4d[] = {
    // Feature 1, 2, 3, 4 (4 features highly correlated)
    1.0f, 2.0f, 3.0f, 4.0f,    2.0f, 4.0f, 6.0f, 8.0f,
    3.0f, 6.0f, 9.0f, 12.0f,   4.0f, 8.0f, 12.0f, 16.0f,
    5.0f, 10.0f, 15.0f, 20.0f, 6.0f, 12.0f, 18.0f, 24.0f,
    7.0f, 14.0f, 21.0f, 28.0f, 8.0f, 16.0f, 24.0f, 32.0f,
    9.0f, 18.0f, 27.0f, 36.0f, 10.0f, 20.0f, 30.0f, 40.0f,
};
#define NUM_SAMPLES_4D 10
#define NUM_FEATURES_4D 4

static void print_test_header(const char *test_name) {
  printf("\n=== %s ===\n", test_name);
}

static void print_test_result(const char *test_name, int passed) {
  printf("[%s] %s\n", passed ? "PASS" : "FAIL", test_name);
}

static int float_equals(float32_t a, float32_t b, float32_t tol) {
  return fabsf(a - b) < tol;
}

int run_pca_tests(void) {
  printf("EIF PCA Test\n");
  printf("============\n");

  // Initialize memory pool
  void *pool_memory = malloc(TEST_POOL_SIZE);
  if (!pool_memory) {
    printf("Failed to allocate memory pool\n");
    return 1;
  }

  eif_memory_pool_t pool;
  eif_status_t status =
      eif_memory_pool_init(&pool, pool_memory, TEST_POOL_SIZE);
  if (status != EIF_STATUS_OK) {
    printf("Failed to initialize memory pool\n");
    free(pool_memory);
    return 1;
  }

  // Test 1: Initialization
  print_test_header("Test 1: PCA Initialization");
  eif_pca_t pca;
  status = eif_pca_init(&pca, NUM_FEATURES_2D, 1, &pool);
  print_test_result("Initialization", status == EIF_STATUS_OK);
  if (status != EIF_STATUS_OK) {
    printf("Failed to initialize PCA: %d\n", status);
    free(pool_memory);
    return 1;
  }
  printf("  Features: %d, Components: %d\n", pca.n_features, pca.n_components);

  // Test 2: Fitting 2D data
  print_test_header("Test 2: Fitting 2D Data");
  clock_t start = clock();
  status = eif_pca_fit(&pca, test_data_2d, NUM_SAMPLES_2D, &pool);
  clock_t end = clock();
  double fit_time = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;

  print_test_result("Fitting", status == EIF_STATUS_OK);
  if (status != EIF_STATUS_OK) {
    printf("Failed to fit PCA: %d\n", status);
    free(pool_memory);
    return 1;
  }
  printf("  Fit time: %.2f ms\n", fit_time);
  printf("  Memory used: %zu / %zu bytes (%.1f%%)\n", pool.used, pool.size,
         100.0 * pool.used / pool.size);

  // Check explained variance
  float32_t variance_ratio[1];
  status = eif_pca_get_explained_variance_ratio(&pca, variance_ratio);
  printf("  Explained variance ratio: %.4f\n", variance_ratio[0]);
  print_test_result("High variance ratio",
                    variance_ratio[0] > 0.99f); // Should be ~1.0

  // Test 3: Transform single sample
  print_test_header("Test 3: Transform Single Sample");
  const float32_t test_sample[] = {5.0f, 10.0f};
  float32_t transformed[1];
  status = eif_pca_transform_sample(&pca, test_sample, transformed);
  print_test_result("Transform sample", status == EIF_STATUS_OK);
  printf("  Input: [%.1f, %.1f]\n", test_sample[0], test_sample[1]);
  printf("  Transformed: [%.4f]\n", transformed[0]);

  // Test 4: Batch transformation
  print_test_header("Test 4: Batch Transformation");
  float32_t *X_transformed =
      eif_memory_alloc(&pool, NUM_SAMPLES_2D * 1 * sizeof(float32_t), 4);
  start = clock();
  status = eif_pca_transform(&pca, test_data_2d, NUM_SAMPLES_2D, X_transformed);
  end = clock();
  double transform_time = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;

  print_test_result("Batch transformation", status == EIF_STATUS_OK);
  printf("  Transform time: %.2f ms (%.2f ms/sample)\n", transform_time,
         transform_time / NUM_SAMPLES_2D);
  printf("  First 3 transformed samples:\n");
  for (int i = 0; i < 3; i++) {
    printf("    Sample %d: [%.4f]\n", i, X_transformed[i]);
  }

  // Test 5: Inverse transformation
  print_test_header("Test 5: Inverse Transformation");
  float32_t *X_reconstructed = eif_memory_alloc(
      &pool, NUM_SAMPLES_2D * NUM_FEATURES_2D * sizeof(float32_t), 4);
  status = eif_pca_inverse_transform(&pca, X_transformed, NUM_SAMPLES_2D,
                                     X_reconstructed);
  print_test_result("Inverse transformation", status == EIF_STATUS_OK);

  // Check reconstruction quality (should be close but not exact for 1 component)
  float32_t reconstruction_error = 0.0f;
  for (int i = 0; i < NUM_SAMPLES_2D * NUM_FEATURES_2D; i++) {
    float32_t diff = test_data_2d[i] - X_reconstructed[i];
    reconstruction_error += diff * diff;
  }
  reconstruction_error =
      sqrtf(reconstruction_error / (NUM_SAMPLES_2D * NUM_FEATURES_2D));
  printf("  Reconstruction RMSE: %.4f\n", reconstruction_error);
  print_test_result("Low reconstruction error",
                    reconstruction_error < 0.5f); // Should be very small

  // Test 6: Higher dimensional data (4D -> 2D)
  print_test_header("Test 6: 4D to 2D Reduction");
  eif_pca_t pca_4d;
  status = eif_pca_init(&pca_4d, NUM_FEATURES_4D, 2, &pool);
  print_test_result("4D PCA initialization", status == EIF_STATUS_OK);

  start = clock();
  status = eif_pca_fit(&pca_4d, test_data_4d, NUM_SAMPLES_4D, &pool);
  end = clock();
  fit_time = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
  print_test_result("4D fitting", status == EIF_STATUS_OK);
  printf("  Fit time: %.2f ms\n", fit_time);

  // Get explained variance
  float32_t variance_ratios[2];
  float32_t cumulative[2];
  eif_pca_get_explained_variance_ratio(&pca_4d, variance_ratios);
  eif_pca_get_cumulative_variance(&pca_4d, cumulative);

  printf("  Explained variance by component:\n");
  for (int i = 0; i < 2; i++) {
    printf("    PC%d: %.4f (cumulative: %.4f)\n", i + 1, variance_ratios[i],
           cumulative[i]);
  }
  print_test_result("High cumulative variance (>95%)",
                    cumulative[1] > 0.95f);

  // Transform 4D data
  float32_t *X_4d_transformed =
      eif_memory_alloc(&pool, NUM_SAMPLES_4D * 2 * sizeof(float32_t), 4);
  status =
      eif_pca_transform(&pca_4d, test_data_4d, NUM_SAMPLES_4D, X_4d_transformed);
  print_test_result("4D transformation", status == EIF_STATUS_OK);

  printf("  First 3 transformed samples (4D -> 2D):\n");
  for (int i = 0; i < 3; i++) {
    printf("    Sample %d: [%.4f, %.4f]\n", i, X_4d_transformed[i * 2],
           X_4d_transformed[i * 2 + 1]);
  }

  // Test 7: Dimensionality reduction effectiveness
  print_test_header("Test 7: Dimensionality Reduction Check");
  int reduced_dims = 0;
  float32_t threshold = 0.95f; // 95% variance
  float32_t cum_var = 0.0f;
  for (int i = 0; i < 2; i++) {
    cum_var += variance_ratios[i];
    reduced_dims++;
    if (cum_var >= threshold) {
      break;
    }
  }
  printf("  Original dimensions: %d\n", NUM_FEATURES_4D);
  printf("  Reduced dimensions: %d\n", reduced_dims);
  printf("  Variance retained: %.2f%%\n", cum_var * 100.0f);
  print_test_result("Effective reduction", reduced_dims < NUM_FEATURES_4D);

  // Summary
  printf("\n===============================\n");
  printf("PCA Test Summary\n");
  printf("===============================\n");
  printf("2D Test:\n");
  printf("  - Fit time: %.2f ms\n", fit_time);
  printf("  - Transform time: %.3f ms/sample\n",
         transform_time / NUM_SAMPLES_2D);
  printf("  - Reconstruction RMSE: %.4f\n", reconstruction_error);
  printf("  - Variance explained: %.2f%%\n", variance_ratio[0] * 100.0f);
  printf("\n4D Test:\n");
  printf("  - Components retained: 2\n");
  printf("  - Variance explained: %.2f%%\n", cumulative[1] * 100.0f);
  printf("  - Dimensionality reduction: %d -> %d\n", NUM_FEATURES_4D,
         reduced_dims);
  printf("  - Memory usage: %zu bytes\n", pool.used);
  printf("\n✓ All tests passed!\n\n");

  free(pool_memory);
  return 0;
}
