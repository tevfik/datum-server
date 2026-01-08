/**
 * @file test_fixed_point.c
 * @brief Unit tests for Q15 fixed-point arithmetic concepts
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// =============================================================================
// Test Utilities
// =============================================================================

static int fp_tests_run = 0;
static int fp_tests_passed = 0;

#define ASSERT_FP(cond, msg)                                                   \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL: %s\n", msg);                                               \
      return 0;                                                                \
    }                                                                          \
  } while (0)

#define ASSERT_FP_NEAR(a, b, eps, msg)                                         \
  do {                                                                         \
    if (abs((a) - (b)) > (eps)) {                                              \
      printf("FAIL: %s (got %d, expected %d)\n", msg, (a), (b));               \
      return 0;                                                                \
    }                                                                          \
  } while (0)

#define RUN_FP_TEST(name)                                                      \
  do {                                                                         \
    printf("Running %s... ", #name);                                           \
    fp_tests_run++;                                                            \
    if (name()) {                                                              \
      fp_tests_passed++;                                                       \
      printf("PASS\n");                                                        \
    }                                                                          \
  } while (0)

// =============================================================================
// Q15 Helper Functions (inline implementation for testing)
// =============================================================================

static inline int16_t float_to_q15(float f) {
  if (f >= 1.0f)
    return 32767;
  if (f <= -1.0f)
    return -32768;
  return (int16_t)(f * 32768.0f);
}

static inline float q15_to_float(int16_t q) { return (float)q / 32768.0f; }

static inline int16_t q15_mul(int16_t a, int16_t b) {
  int32_t temp = (int32_t)a * b;
  return (int16_t)(temp >> 15);
}

// =============================================================================
// Q15 Conversion Tests
// =============================================================================

static int test_float_to_q15_basic(void) {
  int16_t q;

  q = float_to_q15(0.5f);
  ASSERT_FP_NEAR(q, 16384, 10, "0.5 should convert to ~16384");

  q = float_to_q15(-0.5f);
  ASSERT_FP_NEAR(q, -16384, 10, "-0.5 should convert to ~-16384");

  q = float_to_q15(0.0f);
  ASSERT_FP_NEAR(q, 0, 1, "0.0 should convert to 0");

  q = float_to_q15(0.25f);
  ASSERT_FP_NEAR(q, 8192, 10, "0.25 should convert to ~8192");

  return 1;
}

static int test_q15_to_float_basic(void) {
  float f;

  f = q15_to_float(16384);
  ASSERT_FP(fabsf(f - 0.5f) < 0.001f, "16384 should convert to ~0.5");

  f = q15_to_float(-16384);
  ASSERT_FP(fabsf(f - (-0.5f)) < 0.001f, "-16384 should convert to ~-0.5");

  f = q15_to_float(0);
  ASSERT_FP(fabsf(f) < 0.001f, "0 should convert to 0.0");

  return 1;
}

static int test_q15_roundtrip(void) {
  float values[] = {0.0f, 0.1f, 0.5f, 0.9f, -0.5f, -0.9f};
  int n = sizeof(values) / sizeof(values[0]);

  for (int i = 0; i < n; i++) {
    int16_t q = float_to_q15(values[i]);
    float back = q15_to_float(q);
    ASSERT_FP(fabsf(back - values[i]) < 0.001f,
              "Roundtrip should preserve value");
  }

  return 1;
}

// =============================================================================
// Q15 Arithmetic Tests
// =============================================================================

static int test_q15_multiply_basic(void) {
  // 0.5 * 0.5 = 0.25
  int16_t a = float_to_q15(0.5f);
  int16_t b = float_to_q15(0.5f);
  int16_t result = q15_mul(a, b);

  ASSERT_FP_NEAR(result, 8192, 100, "0.5 * 0.5 should be ~0.25 (8192)");
  return 1;
}

static int test_q15_multiply_negative(void) {
  // 0.5 * (-0.5) = -0.25
  int16_t a = float_to_q15(0.5f);
  int16_t b = float_to_q15(-0.5f);
  int16_t result = q15_mul(a, b);

  ASSERT_FP_NEAR(result, -8192, 100, "0.5 * -0.5 should be ~-0.25");
  return 1;
}

static int test_q15_saturation(void) {
  int16_t q;

  q = float_to_q15(1.5f);
  ASSERT_FP(q == 32767, "Values > 1.0 should saturate to max");

  q = float_to_q15(-1.5f);
  ASSERT_FP(q == -32768, "Values < -1.0 should saturate to min");

  return 1;
}

// =============================================================================
// Q15 FIR Filter Concept Tests
// =============================================================================

static int test_fir_ma_concept(void) {
  // Simple 4-tap moving average in Q15
  int16_t coeffs[4] = {8192, 8192, 8192, 8192}; // 4 * 0.25 = 1.0
  int16_t buffer[4] = {0};
  int idx = 0;

  // Process some samples
  int16_t input = float_to_q15(0.5f);
  for (int i = 0; i < 10; i++) {
    buffer[idx] = input;
    idx = (idx + 1) % 4;

    // Compute output
    int32_t acc = 0;
    for (int j = 0; j < 4; j++) {
      acc += (int32_t)coeffs[j] * buffer[j];
    }
    int16_t output = (int16_t)(acc >> 15);

    if (i >= 3) { // After filling buffer
      float out_f = q15_to_float(output);
      ASSERT_FP(fabsf(out_f - 0.5f) < 0.1f, "DC gain should be ~1");
    }
  }

  return 1;
}

static int test_fir_accumulator_no_overflow(void) {
  // Test that using 64-bit accumulator prevents overflow for large sums
  int16_t max_val = 32767;
  int64_t acc = 0; // Use 64-bit for large accumulations

  for (int i = 0; i < 100; i++) {
    acc += (int64_t)max_val * max_val;
  }

  // 64-bit can handle 100 * 32767^2 = ~107 billion
  ASSERT_FP(acc > 0, "64-bit accumulator should handle large sums");
  return 1;
}

// =============================================================================
// Edge Cases
// =============================================================================

static int test_q15_extreme_values(void) {
  int16_t max_q15 = 32767;
  int16_t min_q15 = -32768;

  float max_f = q15_to_float(max_q15);
  float min_f = q15_to_float(min_q15);

  ASSERT_FP(max_f > 0.99f, "Max Q15 should be near 1.0");
  ASSERT_FP(min_f < -0.99f, "Min Q15 should be near -1.0");

  return 1;
}

static int test_q15_multiply_by_one(void) {
  int16_t one = 32767;  // Close to 1.0
  int16_t half = 16384; // 0.5

  int16_t result = q15_mul(one, half);

  // Result should be close to 0.5 (but slightly less due to 32767 < 1.0)
  ASSERT_FP(result > 16000 && result < 17000,
            "Multiply by ~1 should preserve value");
  return 1;
}

static int test_q15_multiply_by_zero(void) {
  int16_t zero = 0;
  int16_t any = 16384;

  int16_t result = q15_mul(zero, any);
  ASSERT_FP(result == 0, "Multiply by 0 should give 0");

  return 1;
}

// =============================================================================
// Main
// =============================================================================

int run_fixed_point_tests(void) {
  printf("\n=== Running Test Suite: Fixed Point (Q15) ===\n");

  RUN_FP_TEST(test_float_to_q15_basic);
  RUN_FP_TEST(test_q15_to_float_basic);
  RUN_FP_TEST(test_q15_roundtrip);
  RUN_FP_TEST(test_q15_multiply_basic);
  RUN_FP_TEST(test_q15_multiply_negative);
  RUN_FP_TEST(test_q15_saturation);
  RUN_FP_TEST(test_fir_ma_concept);
  RUN_FP_TEST(test_fir_accumulator_no_overflow);
  RUN_FP_TEST(test_q15_extreme_values);
  RUN_FP_TEST(test_q15_multiply_by_one);
  RUN_FP_TEST(test_q15_multiply_by_zero);

  printf("Results: %d Run, %d Passed, %d Failed\n", fp_tests_run,
         fp_tests_passed, fp_tests_run - fp_tests_passed);

  return (fp_tests_run == fp_tests_passed) ? 0 : 1;
}
