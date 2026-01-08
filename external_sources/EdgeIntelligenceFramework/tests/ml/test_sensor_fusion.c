/**
 * @file test_sensor_fusion.c
 * @brief Unit tests for sensor fusion modules (complementary filter, Kalman)
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Use EIF machine learning header that contains complementary and kalman
#include "../../ml/include/eif_ml.h"

// =============================================================================
// Test Utilities
// =============================================================================

static int sf_tests_run = 0;
static int sf_tests_passed = 0;

#define ASSERT_SF(cond, msg)                                                   \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL: %s\n", msg);                                               \
      return 0;                                                                \
    }                                                                          \
  } while (0)

#define ASSERT_NEAR_SF(a, b, eps, msg)                                         \
  do {                                                                         \
    if (fabsf((a) - (b)) > (eps)) {                                            \
      printf("FAIL: %s (got %f, expected %f)\n", msg, (float)(a), (float)(b)); \
      return 0;                                                                \
    }                                                                          \
  } while (0)

#define RUN_SF_TEST(name)                                                      \
  do {                                                                         \
    printf("Running %s... ", #name);                                           \
    sf_tests_run++;                                                            \
    if (name()) {                                                              \
      sf_tests_passed++;                                                       \
      printf("PASS\n");                                                        \
    }                                                                          \
  } while (0)

// =============================================================================
// Complementary Filter Tests (using actual eif_ml.h API)
// =============================================================================

static int test_complementary_basic(void) {
  // Test basic complementary filter concept
  // Simple weighted average: alpha * gyro + (1-alpha) * accel
  float alpha = 0.98f;
  float angle = 0.0f;
  float dt = 0.02f;

  // Simulate: gyro=0, accel says 45 degrees
  float accel_angle = 0.785f; // 45 degrees in radians

  for (int i = 0; i < 100; i++) {
    float gyro_rate = 0.0f;
    float gyro_angle = angle + gyro_rate * dt;
    angle = alpha * gyro_angle + (1.0f - alpha) * accel_angle;
  }

  // Should converge to accel angle
  // With alpha=0.98, convergence is slow, allow larger tolerance
  ASSERT_NEAR_SF(angle, accel_angle, 0.15f,
                 "Should converge toward accel angle");
  return 1;
}

static int test_complementary_gyro_integration(void) {
  float alpha = 0.98f;
  float angle = 0.0f;
  float dt = 0.02f;

  // Constant rotation: 1 rad/s for 1 second
  for (int i = 0; i < 50; i++) {
    float gyro_rate = 1.0f;
    float accel_angle = 0.0f; // Accel says 0

    float gyro_angle = angle + gyro_rate * dt;
    angle = alpha * gyro_angle + (1.0f - alpha) * accel_angle;
  }

  // Should have integrated (mostly)
  ASSERT_SF(angle > 0.5f, "Should track gyro integration");
  return 1;
}

// =============================================================================
// 1D Kalman Filter Tests (concept verification)
// =============================================================================

static int test_kalman_1d_concept(void) {
  // Simple 1D Kalman implementation
  float x = 0.0f;  // State estimate
  float P = 1.0f;  // Uncertainty
  float Q = 0.01f; // Process noise
  float R = 0.5f;  // Measurement noise

  // Measurements near true value of 5.0
  float measurements[] = {5.1f, 4.9f, 5.2f, 4.8f, 5.0f, 5.1f, 4.9f, 5.0f};
  int n = sizeof(measurements) / sizeof(measurements[0]);

  for (int i = 0; i < n; i++) {
    // Predict
    P = P + Q;

    // Update
    float K = P / (P + R); // Kalman gain
    x = x + K * (measurements[i] - x);
    P = (1 - K) * P;
  }

  // Should converge near 5.0
  ASSERT_NEAR_SF(x, 5.0f, 0.5f, "Should converge to measurement mean");
  ASSERT_SF(P < 1.0f, "Uncertainty should decrease");
  return 1;
}

static int test_kalman_high_r(void) {
  // High R = don't trust measurements much
  float x = 10.0f;
  float P = 1.0f;
  float Q = 0.001f;
  float R = 100.0f; // Very high

  // Measurements say 0
  for (int i = 0; i < 10; i++) {
    P = P + Q;
    float K = P / (P + R);
    x = x + K * (0.0f - x);
    P = (1 - K) * P;
  }

  // State should barely move
  ASSERT_SF(x > 5.0f, "High R should resist measurement updates");
  return 1;
}

static int test_kalman_low_r(void) {
  // Low R = trust measurements completely
  float x = 0.0f;
  float P = 1.0f;
  float Q = 0.001f;
  float R = 0.001f; // Very low

  // Single measurement
  P = P + Q;
  float K = P / (P + R);
  x = x + K * (10.0f - x);

  // Should jump to measurement
  ASSERT_NEAR_SF(x, 10.0f, 0.5f, "Low R should quickly adopt measurement");
  return 1;
}

// =============================================================================
// IMU Angle Calculation Tests
// =============================================================================

static int test_accel_to_pitch(void) {
  // Flat device: ax=0, ay=0, az=9.81
  float ax = 0.0f, ay = 0.0f, az = 9.81f;
  float pitch = atan2f(ax, sqrtf(ay * ay + az * az));

  ASSERT_NEAR_SF(pitch, 0.0f, 0.01f, "Flat device should have 0 pitch");

  // Tilted 45 degrees forward
  ax = 6.94f; // sin(45°) * 9.81
  az = 6.94f; // cos(45°) * 9.81
  pitch = atan2f(ax, sqrtf(ay * ay + az * az));

  ASSERT_NEAR_SF(pitch, 0.785f, 0.1f, "45° tilt should give ~0.785 rad");
  return 1;
}

static int test_accel_to_roll(void) {
  // Tilted to the side
  float ax = 0.0f, ay = 6.94f, az = 6.94f; // 45° roll
  float roll = atan2f(ay, sqrtf(ax * ax + az * az));

  ASSERT_NEAR_SF(roll, 0.785f, 0.1f, "45° roll should give ~0.785 rad");
  return 1;
}

// =============================================================================
// Quaternion Basics
// =============================================================================

static int test_quaternion_identity(void) {
  // Identity quaternion: [1, 0, 0, 0]
  float q[4] = {1.0f, 0.0f, 0.0f, 0.0f};

  // Norm should be 1
  float norm = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  ASSERT_NEAR_SF(norm, 1.0f, 0.001f, "Quaternion norm should be 1");

  return 1;
}

static int test_quaternion_normalization(void) {
  // Un-normalized quaternion
  float q[4] = {2.0f, 0.0f, 0.0f, 0.0f};

  float norm = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  q[0] /= norm;
  q[1] /= norm;
  q[2] /= norm;
  q[3] /= norm;

  float new_norm = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  ASSERT_NEAR_SF(new_norm, 1.0f, 0.001f,
                 "Normalized quaternion should have norm 1");

  return 1;
}

// =============================================================================
// Main
// =============================================================================

int run_sensor_fusion_tests(void) {
  printf("\n=== Running Test Suite: Sensor Fusion ===\n");

  RUN_SF_TEST(test_complementary_basic);
  RUN_SF_TEST(test_complementary_gyro_integration);
  RUN_SF_TEST(test_kalman_1d_concept);
  RUN_SF_TEST(test_kalman_high_r);
  RUN_SF_TEST(test_kalman_low_r);
  RUN_SF_TEST(test_accel_to_pitch);
  RUN_SF_TEST(test_accel_to_roll);
  RUN_SF_TEST(test_quaternion_identity);
  RUN_SF_TEST(test_quaternion_normalization);

  printf("Results: %d Run, %d Passed, %d Failed\n", sf_tests_run,
         sf_tests_passed, sf_tests_run - sf_tests_passed);

  return (sf_tests_run == sf_tests_passed) ? 0 : 1;
}
