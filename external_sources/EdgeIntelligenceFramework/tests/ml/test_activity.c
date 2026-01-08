/**
 * @file test_activity.c
 * @brief Unit tests for activity recognition concepts
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// Test Utilities
// =============================================================================

static int act_tests_run = 0;
static int act_tests_passed = 0;

#define ASSERT_ACT(cond, msg)                                                  \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL: %s\n", msg);                                               \
      return 0;                                                                \
    }                                                                          \
  } while (0)

#define RUN_ACT_TEST(name)                                                     \
  do {                                                                         \
    printf("Running %s... ", #name);                                           \
    act_tests_run++;                                                           \
    if (name()) {                                                              \
      act_tests_passed++;                                                      \
      printf("PASS\n");                                                        \
    }                                                                          \
  } while (0)

// =============================================================================
// Helper Structures
// =============================================================================

typedef struct {
  float x, y, z;
} accel_sample_t;

typedef struct {
  float mean_x, mean_y, mean_z;
  float std_x, std_y, std_z;
  float magnitude_mean, magnitude_std;
  float sma;
} activity_features_t;

// =============================================================================
// Feature Extraction Functions
// =============================================================================

static void extract_features(accel_sample_t *samples, int n,
                             activity_features_t *f) {
  // Calculate means
  f->mean_x = 0;
  f->mean_y = 0;
  f->mean_z = 0;
  float sum_mag = 0;

  for (int i = 0; i < n; i++) {
    f->mean_x += samples[i].x;
    f->mean_y += samples[i].y;
    f->mean_z += samples[i].z;
    float mag =
        sqrtf(samples[i].x * samples[i].x + samples[i].y * samples[i].y +
              samples[i].z * samples[i].z);
    sum_mag += mag;
  }
  f->mean_x /= n;
  f->mean_y /= n;
  f->mean_z /= n;
  f->magnitude_mean = sum_mag / n;

  // Calculate std devs
  float var_x = 0, var_y = 0, var_z = 0, var_mag = 0;
  float sma = 0;

  for (int i = 0; i < n; i++) {
    var_x += (samples[i].x - f->mean_x) * (samples[i].x - f->mean_x);
    var_y += (samples[i].y - f->mean_y) * (samples[i].y - f->mean_y);
    var_z += (samples[i].z - f->mean_z) * (samples[i].z - f->mean_z);

    float mag =
        sqrtf(samples[i].x * samples[i].x + samples[i].y * samples[i].y +
              samples[i].z * samples[i].z);
    var_mag += (mag - f->magnitude_mean) * (mag - f->magnitude_mean);

    sma += fabsf(samples[i].x) + fabsf(samples[i].y) + fabsf(samples[i].z);
  }

  f->std_x = sqrtf(var_x / n);
  f->std_y = sqrtf(var_y / n);
  f->std_z = sqrtf(var_z / n);
  f->magnitude_std = sqrtf(var_mag / n);
  f->sma = sma / n;
}

// =============================================================================
// Tests
// =============================================================================

static int test_stationary_features(void) {
  // Simulate stationary data
  accel_sample_t samples[64];
  srand(42);

  for (int i = 0; i < 64; i++) {
    float noise = ((float)(rand() % 100) / 1000.0f - 0.05f);
    samples[i].x = 0.0f + noise * 0.1f;
    samples[i].y = 0.0f + noise * 0.1f;
    samples[i].z = 9.81f + noise * 0.2f;
  }

  activity_features_t f;
  extract_features(samples, 64, &f);

  ASSERT_ACT(f.magnitude_std < 0.5f,
             "Stationary should have low magnitude_std");
  ASSERT_ACT(fabsf(f.mean_z - 9.81f) < 0.5f, "Mean Z should be near gravity");

  return 1;
}

static int test_walking_features(void) {
  // Simulate walking data (periodic motion)
  accel_sample_t samples[64];

  for (int i = 0; i < 64; i++) {
    float t = (float)i / 50.0f;
    samples[i].x = 0.5f * sinf(2.0f * 3.14159f * 2.0f * t);
    samples[i].y = 0.3f * sinf(2.0f * 3.14159f * 4.0f * t);
    samples[i].z = 9.81f + 1.0f * fabsf(sinf(2.0f * 3.14159f * 4.0f * t));
  }

  activity_features_t f;
  extract_features(samples, 64, &f);

  ASSERT_ACT(f.magnitude_std > 0.1f, "Walking should have noticeable motion");

  return 1;
}

static int test_running_features(void) {
  // Simulate running data (higher amplitude)
  accel_sample_t samples[64];

  for (int i = 0; i < 64; i++) {
    float t = (float)i / 50.0f;
    samples[i].x = 1.5f * sinf(2.0f * 3.14159f * 3.0f * t);
    samples[i].y = 1.0f * sinf(2.0f * 3.14159f * 6.0f * t);
    samples[i].z = 9.81f + 3.0f * fabsf(sinf(2.0f * 3.14159f * 6.0f * t));
  }

  activity_features_t f;
  extract_features(samples, 64, &f);

  // Running generates motion but exact values depend on simulation
  ASSERT_ACT(f.magnitude_std > 0.5f, "Running should have measurable motion");

  return 1;
}

static int test_classify_by_magnitude_std(void) {
  // Simple rule-based classification by magnitude_std
  float mag_std_stationary = 0.1f;
  float mag_std_walking = 1.5f;
  float mag_std_running = 4.0f;

  // Inline classification logic
  int class_stationary = (mag_std_stationary < 0.5f)   ? 0
                         : (mag_std_stationary > 3.0f) ? 2
                                                       : 1;
  int class_walking = (mag_std_walking < 0.5f)   ? 0
                      : (mag_std_walking > 3.0f) ? 2
                                                 : 1;
  int class_running = (mag_std_running < 0.5f)   ? 0
                      : (mag_std_running > 3.0f) ? 2
                                                 : 1;

  ASSERT_ACT(class_stationary == 0, "Low std should be stationary");
  ASSERT_ACT(class_walking == 1, "Medium std should be walking");
  ASSERT_ACT(class_running == 2, "High std should be running");

  return 1;
}

static int test_zero_input(void) {
  accel_sample_t samples[64];
  memset(samples, 0, sizeof(samples));

  activity_features_t f;
  extract_features(samples, 64, &f);

  ASSERT_ACT(f.mean_x == 0.0f, "Mean X should be 0");
  ASSERT_ACT(f.mean_y == 0.0f, "Mean Y should be 0");
  ASSERT_ACT(f.magnitude_mean == 0.0f, "Magnitude should be 0");

  return 1;
}

static int test_constant_input(void) {
  accel_sample_t samples[64];
  for (int i = 0; i < 64; i++) {
    samples[i].x = 1.0f;
    samples[i].y = 2.0f;
    samples[i].z = 9.81f;
  }

  activity_features_t f;
  extract_features(samples, 64, &f);

  ASSERT_ACT(f.std_x < 0.001f, "Constant input should have ~0 std");
  ASSERT_ACT(fabsf(f.mean_x - 1.0f) < 0.001f, "Mean should match constant");

  return 1;
}

static int test_window_concept(void) {
  // Test windowing concept
  int window_size = 64;
  int hop_size = 16;

  int total_samples = 200;
  int classifications = 0;

  for (int i = 0; i <= total_samples - window_size; i += hop_size) {
    classifications++;
  }

  ASSERT_ACT(classifications > 0, "Should produce classifications");
  ASSERT_ACT(classifications == (200 - 64) / 16 + 1,
             "Should match expected count");

  return 1;
}

static int test_sma_calculation(void) {
  accel_sample_t samples[4] = {{1.0f, 2.0f, 3.0f},
                               {2.0f, 3.0f, 4.0f},
                               {1.0f, 2.0f, 3.0f},
                               {2.0f, 3.0f, 4.0f}};

  activity_features_t f;
  extract_features(samples, 4, &f);

  // SMA = mean(|x| + |y| + |z|) = mean(6, 9, 6, 9) = 7.5
  ASSERT_ACT(fabsf(f.sma - 7.5f) < 0.1f, "SMA should be 7.5");

  return 1;
}

// =============================================================================
// Main
// =============================================================================

int run_activity_tests(void) {
  printf("\n=== Running Test Suite: Activity Recognition ===\n");

  srand(42);

  RUN_ACT_TEST(test_stationary_features);
  RUN_ACT_TEST(test_walking_features);
  RUN_ACT_TEST(test_running_features);
  RUN_ACT_TEST(test_classify_by_magnitude_std);
  RUN_ACT_TEST(test_zero_input);
  RUN_ACT_TEST(test_constant_input);
  RUN_ACT_TEST(test_window_concept);
  RUN_ACT_TEST(test_sma_calculation);

  printf("Results: %d Run, %d Passed, %d Failed\n", act_tests_run,
         act_tests_passed, act_tests_run - act_tests_passed);

  return (act_tests_run == act_tests_passed) ? 0 : 1;
}
