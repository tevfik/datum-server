/**
 * @file test_dsp_iir.c
 * @brief Unit tests for IIR Bi-Quad filters
 */

#include "../framework/eif_test_runner.h"
#include "eif_dsp_iir.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Test Bi-Quad Filter initialization
bool test_iir_init(void) {
  eif_dsp_iir_t filter;

  // Standard Bi-Quad coefficients for a simple LPF
  eif_dsp_iir_init(&filter, 0.2929f, 0.5858f, 0.2929f, 0.0f, 0.1716f);

  // Verify coefficients were set
  TEST_ASSERT_EQUAL_FLOAT(0.2929f, filter.b0, 0.0001f);
  TEST_ASSERT_EQUAL_FLOAT(0.5858f, filter.b1, 0.0001f);

  return true;
}

// Test filter reset
bool test_iir_reset(void) {
  eif_dsp_iir_t filter;
  eif_dsp_iir_init(&filter, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f);

  // Process some data to populate state
  eif_dsp_iir_update(&filter, 1.0f);
  eif_dsp_iir_update(&filter, 0.5f);

  // Reset and verify state is cleared
  eif_dsp_iir_reset(&filter);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, filter.state[0], 0.0001f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, filter.state[1], 0.0001f);

  return true;
}

// Test DC gain with passthrough coefficients
bool test_iir_dc_gain(void) {
  eif_dsp_iir_t filter;

  // Unity gain passthrough: y[n] = x[n]
  eif_dsp_iir_init(&filter, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);

  float output = eif_dsp_iir_update(&filter, 0.5f);
  TEST_ASSERT_EQUAL_FLOAT(0.5f, output, 0.001f);

  return true;
}

// Test block processing
bool test_iir_block_process(void) {
  eif_dsp_iir_t filter;

  // Simple averaging filter: y[n] = 0.5*x[n] + 0.5*x[n-1]
  eif_dsp_iir_init(&filter, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f);

  float input[] = {1.0f, 1.0f, 1.0f, 1.0f};
  float output[4] = {0};

  eif_dsp_iir_process(&filter, input, output, 4);

  // After transient: output should approach 1.0
  TEST_ASSERT(output[3] > 0.9f);

  return true;
}

BEGIN_TEST_SUITE(run_iir_tests)
RUN_TEST(test_iir_init);
RUN_TEST(test_iir_reset);
RUN_TEST(test_iir_dc_gain);
RUN_TEST(test_iir_block_process);
END_TEST_SUITE()
