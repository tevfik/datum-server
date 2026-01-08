/**
 * @file test_predictive_maintenance.c
 * @brief Unit tests for predictive maintenance concepts
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// =============================================================================
// Test Utilities
// =============================================================================

static int pm_tests_run = 0;
static int pm_tests_passed = 0;

#define ASSERT_PM(cond, msg)                                                   \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL: %s\n", msg);                                               \
      return 0;                                                                \
    }                                                                          \
  } while (0)

#define ASSERT_NEAR_PM(a, b, eps, msg)                                         \
  do {                                                                         \
    if (fabsf((a) - (b)) > (eps)) {                                            \
      printf("FAIL: %s (got %f, expected %f)\n", msg, (float)(a), (float)(b)); \
      return 0;                                                                \
    }                                                                          \
  } while (0)

#define RUN_PM_TEST(name)                                                      \
  do {                                                                         \
    printf("Running %s... ", #name);                                           \
    pm_tests_run++;                                                            \
    if (name()) {                                                              \
      pm_tests_passed++;                                                       \
      printf("PASS\n");                                                        \
    }                                                                          \
  } while (0)

// =============================================================================
// Z-Score Anomaly Detection Tests
// =============================================================================

static int test_z_score_calculation(void) {
  // Z-score = (value - mean) / std_dev
  float mean = 100.0f;
  float std_dev = 5.0f;
  float value = 115.0f;

  float z = (value - mean) / std_dev;

  ASSERT_NEAR_PM(z, 3.0f, 0.01f, "Z-score should be 3.0");
  return 1;
}

static int test_z_score_anomaly_detection(void) {
  float threshold = 3.0f;
  float mean = 100.0f;
  float std_dev = 5.0f;

  // Normal value
  float normal = 105.0f;
  float z_normal = fabsf((normal - mean) / std_dev);
  ASSERT_PM(z_normal < threshold, "Normal value should not be anomaly");

  // Anomaly
  float anomaly = 120.0f;
  float z_anomaly = fabsf((anomaly - mean) / std_dev);
  ASSERT_PM(z_anomaly > threshold, "Large deviation should be anomaly");

  return 1;
}

// =============================================================================
// Running Statistics Tests
// =============================================================================

static int test_running_mean(void) {
  // Exponential moving average for running mean
  float alpha = 0.1f;
  float mean = 0.0f;

  float values[] = {10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f};
  int n = sizeof(values) / sizeof(values[0]);

  for (int i = 0; i < n; i++) {
    mean = alpha * values[i] + (1.0f - alpha) * mean;
  }

  // Should converge toward 10
  ASSERT_PM(mean > 5.0f, "Running mean should approach value");
  return 1;
}

static int test_running_variance(void) {
  // Welford's online algorithm for variance
  int n = 0;
  float mean = 0.0f;
  float M2 = 0.0f;

  float values[] = {2.0f, 4.0f, 4.0f, 4.0f, 5.0f, 5.0f, 7.0f, 9.0f};
  int count = sizeof(values) / sizeof(values[0]);

  for (int i = 0; i < count; i++) {
    n++;
    float delta = values[i] - mean;
    mean += delta / n;
    float delta2 = values[i] - mean;
    M2 += delta * delta2;
  }

  float variance = M2 / n;
  float std_dev = sqrtf(variance);

  ASSERT_NEAR_PM(mean, 5.0f, 0.1f, "Mean should be 5");
  ASSERT_PM(std_dev > 1.0f && std_dev < 3.0f, "Std dev should be reasonable");
  return 1;
}

// =============================================================================
// RMS and Energy Tests
// =============================================================================

static int test_rms_calculation(void) {
  // RMS of sine wave = 1/sqrt(2)
  float signal[100];
  for (int i = 0; i < 100; i++) {
    signal[i] = sinf(2.0f * 3.14159f * (float)i / 100.0f);
  }

  float sum_sq = 0.0f;
  for (int i = 0; i < 100; i++) {
    sum_sq += signal[i] * signal[i];
  }
  float rms = sqrtf(sum_sq / 100.0f);

  ASSERT_NEAR_PM(rms, 0.707f, 0.05f, "RMS of sine should be ~0.707");
  return 1;
}

static int test_signal_energy(void) {
  float signal[] = {1.0f, 2.0f, 3.0f, 2.0f, 1.0f};
  int n = 5;

  float energy = 0.0f;
  for (int i = 0; i < n; i++) {
    energy += signal[i] * signal[i];
  }

  // Energy = 1 + 4 + 9 + 4 + 1 = 19
  ASSERT_NEAR_PM(energy, 19.0f, 0.01f, "Energy should be 19");
  return 1;
}

// =============================================================================
// Kurtosis Tests
// =============================================================================

static int test_kurtosis_uniform(void) {
  // Uniform distribution has kurtosis < 3
  float uniform[100];
  for (int i = 0; i < 100; i++) {
    uniform[i] = (float)i / 99.0f; // 0 to 1
  }

  // Calculate mean
  float mean = 0.0f;
  for (int i = 0; i < 100; i++) {
    mean += uniform[i];
  }
  mean /= 100.0f;

  // Calculate moments
  float sum2 = 0.0f, sum4 = 0.0f;
  for (int i = 0; i < 100; i++) {
    float d = uniform[i] - mean;
    sum2 += d * d;
    sum4 += d * d * d * d;
  }
  float var = sum2 / 100.0f;
  float kurt = (sum4 / 100.0f) / (var * var);

  // Uniform has kurtosis ~1.8
  ASSERT_PM(kurt < 3.0f, "Uniform distribution should have kurtosis < 3");
  return 1;
}

// =============================================================================
// Health Index Tests
// =============================================================================

static int test_health_index_calculation(void) {
  // Simple health index: 1 - (current_rms / max_rms)
  float baseline_rms = 1.0f;
  float max_degradation = 5.0f;

  float current_rms = 1.0f; // Normal
  float health = 1.0f - ((current_rms - baseline_rms) / max_degradation);
  if (health > 1.0f)
    health = 1.0f;
  if (health < 0.0f)
    health = 0.0f;

  ASSERT_NEAR_PM(health, 1.0f, 0.01f, "Normal operation should be health=1");

  current_rms = 3.0f; // Degraded
  health = 1.0f - ((current_rms - baseline_rms) / max_degradation);
  ASSERT_PM(health < 1.0f && health > 0.0f, "Degraded should reduce health");

  return 1;
}

static int test_rul_linear_degradation(void) {
  // Simple RUL estimation: if degrading at rate r, time to threshold
  float current_health = 0.7f;
  float threshold = 0.3f;
  float degradation_rate = 0.01f; // Per time unit

  int rul = (int)((current_health - threshold) / degradation_rate);

  ASSERT_PM(rul > 0, "RUL should be positive");
  ASSERT_PM(rul == 40, "RUL should be 40 time units");

  return 1;
}

// =============================================================================
// Crest Factor Tests
// =============================================================================

static int test_crest_factor(void) {
  // Crest factor = peak / rms
  // Sine wave: peak = 1, rms = 0.707, crest factor = 1.414

  float peak = 1.0f;
  float rms = 0.707f;
  float crest = peak / rms;

  ASSERT_NEAR_PM(crest, 1.414f, 0.01f, "Sine crest factor should be ~1.414");

  return 1;
}

// =============================================================================
// Main
// =============================================================================

int run_predictive_maintenance_tests(void) {
  printf("\n=== Running Test Suite: Predictive Maintenance ===\n");

  RUN_PM_TEST(test_z_score_calculation);
  RUN_PM_TEST(test_z_score_anomaly_detection);
  RUN_PM_TEST(test_running_mean);
  RUN_PM_TEST(test_running_variance);
  RUN_PM_TEST(test_rms_calculation);
  RUN_PM_TEST(test_signal_energy);
  RUN_PM_TEST(test_kurtosis_uniform);
  RUN_PM_TEST(test_health_index_calculation);
  RUN_PM_TEST(test_rul_linear_degradation);
  RUN_PM_TEST(test_crest_factor);

  printf("Results: %d Run, %d Passed, %d Failed\n", pm_tests_run,
         pm_tests_passed, pm_tests_run - pm_tests_passed);

  return (pm_tests_run == pm_tests_passed) ? 0 : 1;
}
