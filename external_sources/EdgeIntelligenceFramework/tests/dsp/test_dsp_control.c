/**
 * @file test_dsp_control.c
 * @brief Unit tests for control system utilities
 */

#include "../framework/eif_test_runner.h"
#include "eif_dsp_control.h"
#include <math.h>

// Test Deadzone filter
bool test_deadzone(void) {
  eif_deadzone_t dz;
  eif_deadzone_init(&dz, 0.1f); // 10% deadzone

  // Inside deadzone should return 0
  TEST_ASSERT_EQUAL_FLOAT(0.0f, eif_deadzone_apply(&dz, 0.05f), 0.001f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, eif_deadzone_apply(&dz, -0.08f), 0.001f);

  // Outside deadzone should return scaled value
  float result = eif_deadzone_apply(&dz, 0.5f);
  TEST_ASSERT(result > 0.0f);
  TEST_ASSERT(result < 0.5f); // Scaled

  // Full input should give ~1.0
  result = eif_deadzone_apply(&dz, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, result, 0.01f);

  return true;
}

// Test Differentiator
bool test_differentiator(void) {
  eif_differentiator_t diff;
  eif_differentiator_init(&diff, 100.0f); // 100 Hz

  // First sample returns 0 (no previous)
  float result = eif_differentiator_update(&diff, 0.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, result, 0.001f);

  // Change of 1.0 in 0.01s = 100 units/sec
  result = eif_differentiator_update(&diff, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(100.0f, result, 0.01f);

  // No change = 0 derivative
  result = eif_differentiator_update(&diff, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, result, 0.001f);

  return true;
}

// Test Integrator with anti-windup
bool test_integrator(void) {
  eif_integrator_t integ;
  eif_integrator_init(&integ, 100.0f, -10.0f, 10.0f); // 100 Hz, limits ±10

  // Integrate constant 1.0 for several samples
  float result = 0;
  for (int i = 0; i < 100; i++) {
    result = eif_integrator_update(&integ, 1.0f);
  }

  // Should have accumulated ~1.0 (100 samples at 100Hz = 1 second of 1.0)
  TEST_ASSERT(result > 0.5f && result < 1.5f);

  // Continue integrating - should hit limit
  for (int i = 0; i < 2000; i++) {
    result = eif_integrator_update(&integ, 5.0f);
  }
  TEST_ASSERT_EQUAL_FLOAT(10.0f, result, 0.01f); // Clamped at max

  return true;
}

// Test Zero-crossing detector
bool test_zero_crossing(void) {
  eif_zero_cross_t zc;
  eif_zero_cross_init(&zc);

  // Start positive
  int result = eif_zero_cross_update(&zc, 1.0f);
  TEST_ASSERT_EQUAL_INT(0, result); // No crossing yet

  // Go negative (falling crossing)
  result = eif_zero_cross_update(&zc, -0.5f);
  TEST_ASSERT_EQUAL_INT(-1, result);

  // Go positive (rising crossing)
  result = eif_zero_cross_update(&zc, 0.5f);
  TEST_ASSERT_EQUAL_INT(1, result);

  TEST_ASSERT_EQUAL_INT(2, zc.crossing_count);

  return true;
}

// Test Peak detector
bool test_peak_detector(void) {
  eif_peak_detector_t pd;
  eif_peak_detector_init(&pd, 1.0f, 100.0f, 1000.0f); // 1ms attack, 100ms decay

  // Large input should quickly raise peak
  float result = 0;
  for (int i = 0; i < 10; i++) {
    result = eif_peak_detector_update(&pd, 1.0f);
  }
  TEST_ASSERT(result > 0.8f); // Should be close to 1.0

  // Zero input should slowly decay
  for (int i = 0; i < 100; i++) {
    result = eif_peak_detector_update(&pd, 0.0f);
  }
  TEST_ASSERT(result < 0.5f); // Should have decayed

  return true;
}

BEGIN_TEST_SUITE(run_control_tests)
RUN_TEST(test_deadzone);
RUN_TEST(test_differentiator);
RUN_TEST(test_integrator);
RUN_TEST(test_zero_crossing);
RUN_TEST(test_peak_detector);
END_TEST_SUITE()
