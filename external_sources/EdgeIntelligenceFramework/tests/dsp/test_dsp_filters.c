/**
 * @file test_dsp_filters.c
 * @brief Unit tests for FIR and Biquad filters
 */

#include "../framework/eif_test_runner.h"
#include "eif_dsp_biquad.h"
#include "eif_dsp_fir.h"
#include "eif_dsp_filter.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =============================================================================
// FIR Filter Tests
// =============================================================================

// Test FIR initialization
bool test_fir_init(void) {
  eif_fir_t fir;
  float coeffs[] = {0.25f, 0.5f, 0.25f}; // Simple averaging

  TEST_ASSERT(eif_fir_init(&fir, coeffs, 3) == true);
  TEST_ASSERT_EQUAL_INT(3, fir.order);
  TEST_ASSERT_EQUAL_FLOAT(0.5f, fir.coeffs[1], 0.001f);

  return true;
}

// Test FIR lowpass design
bool test_fir_lowpass_design(void) {
  eif_fir_t fir;

  // Design 21-tap lowpass with 0.2 normalized cutoff
  TEST_ASSERT(eif_fir_design_lowpass(&fir, 0.2f, 21, EIF_WINDOW_HAMMING) ==
              true);
  TEST_ASSERT_EQUAL_INT(21, fir.order);

  // Coefficients should sum to ~1 (unity gain at DC)
  float sum = 0.0f;
  for (int i = 0; i < fir.order; i++) {
    sum += fir.coeffs[i];
  }
  TEST_ASSERT_EQUAL_FLOAT(1.0f, sum, 0.01f);

  return true;
}

// Test FIR filtering
bool test_fir_process(void) {
  eif_fir_t fir;
  float coeffs[] = {0.25f, 0.5f, 0.25f};
  eif_fir_init(&fir, coeffs, 3);

  // Process DC signal - should pass through
  for (int i = 0; i < 10; i++) {
    eif_fir_process(&fir, 1.0f);
  }
  float out = eif_fir_process(&fir, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, out, 0.01f);

  return true;
}

// =============================================================================
// Biquad Filter Tests
// =============================================================================

// Test biquad lowpass
bool test_biquad_lowpass(void) {
  eif_biquad_t bq;

  // 1kHz lowpass at 44.1kHz, Q=0.707
  eif_biquad_lowpass(&bq, 1000.0f, 44100.0f, 0.707f);

  // Process DC - should pass through
  float out = 0.0f;
  for (int i = 0; i < 100; i++) {
    out = eif_biquad_process(&bq, 1.0f);
  }
  TEST_ASSERT_EQUAL_FLOAT(1.0f, out, 0.01f);

  return true;
}

// Test biquad peaking EQ
bool test_biquad_peaking(void) {
  eif_biquad_t bq;

  // 1kHz peaking EQ, +6dB gain
  eif_biquad_peaking(&bq, 1000.0f, 44100.0f, 1.0f, 6.0f);

  // State variables should be initialized
  TEST_ASSERT_EQUAL_FLOAT(0.0f, bq.z1, 0.001f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, bq.z2, 0.001f);

  return true;
}

// Test biquad cascade
bool test_biquad_cascade(void) {
  eif_biquad_cascade_t cascade;

  // 4th order Butterworth lowpass
  eif_biquad_butter4_lowpass(&cascade, 1000.0f, 44100.0f);

  TEST_ASSERT_EQUAL_INT(2, cascade.num_stages);

  // Process DC - should pass through
  float out = 0.0f;
  for (int i = 0; i < 100; i++) {
    out = eif_biquad_cascade_process(&cascade, 1.0f);
  }
  TEST_ASSERT_EQUAL_FLOAT(1.0f, out, 0.02f);

  return true;
}

// Test biquad high frequency attenuation
bool test_biquad_attenuation(void) {
  eif_biquad_t bq;

  // Very low cutoff lowpass (100Hz at 44.1kHz)
  eif_biquad_lowpass(&bq, 100.0f, 44100.0f, 0.707f);

  // Feed high frequency signal (10kHz sine)
  float max_out = 0.0f;
  for (int i = 0; i < 1000; i++) {
    float input = sinf(2.0f * M_PI * 10000.0f * i / 44100.0f);
    float out = eif_biquad_process(&bq, input);
    if (fabsf(out) > max_out)
      max_out = fabsf(out);
  }

  // High frequency should be significantly attenuated
  TEST_ASSERT(max_out < 0.1f);

  return true;
}

// Test direct IIR function
bool test_iir_direct_f32(void) {
    // Identity filter: b0=1, others=0
    // Coeffs: [b0, b1, b2, a1, a2]
    float32_t coeffs[] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float32_t state[] = {0.0f, 0.0f};
    
    float32_t input[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float32_t output[4];
    
    eif_status_t status = eif_dsp_iir_f32(input, output, 4, coeffs, state, 1);
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    for(int i=0; i<4; i++) {
        TEST_ASSERT_EQUAL_FLOAT(input[i], output[i], 0.0001f);
    }
    
    // Test invalid args
    status = eif_dsp_iir_f32(NULL, output, 4, coeffs, state, 1);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, status);
    
    return true;
}

BEGIN_TEST_SUITE(run_filter_tests)
RUN_TEST(test_fir_init);
RUN_TEST(test_fir_lowpass_design);
RUN_TEST(test_fir_process);
RUN_TEST(test_biquad_lowpass);
RUN_TEST(test_biquad_peaking);
RUN_TEST(test_biquad_cascade);
RUN_TEST(test_biquad_attenuation);
RUN_TEST(test_iir_direct_f32);
END_TEST_SUITE()
