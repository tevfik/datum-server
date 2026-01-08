/**
 * @file test_simd.c
 * @brief Unit tests for eif_hal_simd.h SIMD functions
 */

#include "../framework/eif_test_runner.h"
#include "eif_hal_simd.h"
#include <math.h>

// =============================================================================
// Helper for float comparison
// =============================================================================

#define FLOAT_TOLERANCE 1e-4f

static bool float_equal(float32_t a, float32_t b) {
  return fabsf(a - b) < FLOAT_TOLERANCE;
}

// =============================================================================
// Tests
// =============================================================================

bool test_simd_platform_name(void) {
  const char *name = eif_simd_get_platform();
  TEST_ASSERT_TRUE(name != NULL);
  TEST_ASSERT_TRUE(name[0] != '\0');
  return true;
}

bool test_simd_dot_f32_basic(void) {
  float32_t a[] = {1.0f, 2.0f, 3.0f, 4.0f};
  float32_t b[] = {1.0f, 1.0f, 1.0f, 1.0f};

  float32_t result = eif_simd_dot_f32(a, b, 4);

  // 1+2+3+4 = 10
  TEST_ASSERT_TRUE(float_equal(result, 10.0f));
  return true;
}

bool test_simd_dot_f32_identity(void) {
  float32_t a[] = {3.0f, 4.0f};
  float32_t b[] = {3.0f, 4.0f};

  float32_t result = eif_simd_dot_f32(a, b, 2);

  // 9 + 16 = 25 (3^2 + 4^2)
  TEST_ASSERT_TRUE(float_equal(result, 25.0f));
  return true;
}

bool test_simd_dot_f32_large(void) {
  float32_t a[100];
  float32_t b[100];

  for (int i = 0; i < 100; i++) {
    a[i] = 1.0f;
    b[i] = 2.0f;
  }

  float32_t result = eif_simd_dot_f32(a, b, 100);

  // 100 * 2 = 200
  TEST_ASSERT_TRUE(float_equal(result, 200.0f));
  return true;
}

bool test_simd_dot_q7_basic(void) {
  q7_t a[] = {1, 2, 3, 4, 5, 6, 7, 8};
  q7_t b[] = {1, 1, 1, 1, 1, 1, 1, 1};

  int32_t result = eif_simd_dot_q7(a, b, 8);

  // 1+2+3+4+5+6+7+8 = 36
  TEST_ASSERT_EQUAL_INT(36, result);
  return true;
}

bool test_simd_relu_f32(void) {
  float32_t data[] = {-2.0f, -1.0f, 0.0f, 1.0f, 2.0f};

  eif_simd_relu_f32(data, 5);

  TEST_ASSERT_TRUE(float_equal(data[0], 0.0f));
  TEST_ASSERT_TRUE(float_equal(data[1], 0.0f));
  TEST_ASSERT_TRUE(float_equal(data[2], 0.0f));
  TEST_ASSERT_TRUE(float_equal(data[3], 1.0f));
  TEST_ASSERT_TRUE(float_equal(data[4], 2.0f));
  return true;
}

bool test_simd_add_f32(void) {
  float32_t a[] = {1.0f, 2.0f, 3.0f, 4.0f};
  float32_t b[] = {4.0f, 3.0f, 2.0f, 1.0f};
  float32_t c[4];

  eif_simd_add_f32(a, b, c, 4);

  for (int i = 0; i < 4; i++) {
    TEST_ASSERT_TRUE(float_equal(c[i], 5.0f));
  }
  return true;
}

bool test_simd_scale_f32(void) {
  float32_t a[] = {1.0f, 2.0f, 3.0f, 4.0f};
  float32_t b[4];

  eif_simd_scale_f32(a, 2.0f, b, 4);

  TEST_ASSERT_TRUE(float_equal(b[0], 2.0f));
  TEST_ASSERT_TRUE(float_equal(b[1], 4.0f));
  TEST_ASSERT_TRUE(float_equal(b[2], 6.0f));
  TEST_ASSERT_TRUE(float_equal(b[3], 8.0f));
  return true;
}

bool test_simd_conv2d_pixel(void) {
  float32_t patch[] = {1.0f, 1.0f, 1.0f, 1.0f};
  float32_t filter[] = {0.25f, 0.25f, 0.25f, 0.25f};
  float32_t bias = 0.5f;

  float32_t result = eif_simd_conv2d_pixel_f32(patch, filter, bias, 4);

  // (1*0.25 + 1*0.25 + 1*0.25 + 1*0.25) + 0.5 = 1.5
  TEST_ASSERT_TRUE(float_equal(result, 1.5f));
  return true;
}

bool test_simd_matvec_f32(void) {
  // 2x3 matrix
  float32_t A[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
  float32_t x[] = {1.0f, 1.0f, 1.0f};
  float32_t y[2];

  eif_simd_matvec_f32(A, x, y, 2, 3);

  // y[0] = 1+2+3 = 6
  // y[1] = 4+5+6 = 15
  TEST_ASSERT_TRUE(float_equal(y[0], 6.0f));
  TEST_ASSERT_TRUE(float_equal(y[1], 15.0f));
  return true;
}

// =============================================================================
// Test Suite
// =============================================================================

BEGIN_TEST_SUITE(run_simd_tests)
RUN_TEST(test_simd_platform_name);
RUN_TEST(test_simd_dot_f32_basic);
RUN_TEST(test_simd_dot_f32_identity);
RUN_TEST(test_simd_dot_f32_large);
RUN_TEST(test_simd_dot_q7_basic);
RUN_TEST(test_simd_relu_f32);
RUN_TEST(test_simd_add_f32);
RUN_TEST(test_simd_scale_f32);
RUN_TEST(test_simd_conv2d_pixel);
RUN_TEST(test_simd_matvec_f32);
END_TEST_SUITE()
