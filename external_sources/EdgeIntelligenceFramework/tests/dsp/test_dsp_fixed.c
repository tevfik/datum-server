#include "eif_dsp_agc_fixed.h"
#include "eif_dsp_beamformer_fixed.h"
#include "eif_dsp_pid_fixed.h"
#include "eif_dsp_resample_fixed.h"
#include "eif_dsp_smooth_fixed.h"
#include "eif_test_runner.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// Helper to check Q15 within range
static bool eif_test_q15_within(int tolerance, q15_t expected, q15_t actual) {
  if (abs(expected - actual) <= tolerance)
    return true;
  printf(ANSI_COLOR_RED "FAIL" ANSI_COLOR_RESET
                        " Expected %d, Got %d (Tol %d)\n",
         expected, actual, tolerance);
  return false;
}

#define TEST_ASSERT_Q15_WITHIN(tol, exp, act)                                  \
  TEST_ASSERT_TRUE(eif_test_q15_within((tol), (exp), (act)))

// ===================================
// PID Tests
// ===================================
bool test_pid_q15_basic(void) {
  eif_pid_q15_t pid;
  // Kp=1.0 (32767), Ki=0, Kd=0
  eif_pid_q15_init(&pid, 32767, 0, 0, -32767, 32767);

  // Error = 1000. Output should be 1000.
  q15_t out = eif_pid_q15_update(&pid, 1000);
  // (1.0 * 1000) = 1000.
  TEST_ASSERT_Q15_WITHIN(1, 1000, out);

  // Error = -500. Output -500.
  out = eif_pid_q15_update(&pid, -500);
  TEST_ASSERT_Q15_WITHIN(1, -500, out);
  return true;
}

bool test_pid_q15_integral(void) {
  eif_pid_q15_t pid;
  // Ki=1.0.
  eif_pid_q15_init(&pid, 0, 32767, 0, -32767, 32767);

  // Update 1: Error 100. Int += 100. Out = 100.
  eif_pid_q15_update(&pid, 100);

  // Update 2: Error 100. Int += 100 (Total 200). Out = 200.
  q15_t out = eif_pid_q15_update(&pid, 100);

  TEST_ASSERT_Q15_WITHIN(2, 200, out);
  return true;
}

// ===================================
// AGC Tests
// ===================================
bool test_agc_q15_limiting(void) {
  eif_agc_q15_t agc;
  // Target = 0.5 (16384). MaxGain = 4.0 (8192 in Q4.11) to allow boost.
  eif_agc_q15_init(&agc, 16384, 8192);

#define AGC_TEST_LEN 100
  q15_t input_long[AGC_TEST_LEN];
  q15_t output_long[AGC_TEST_LEN];

  // Constant input 0.1 (3276). Target 0.5.
  for (int i = 0; i < AGC_TEST_LEN; i++)
    input_long[i] = 3276;

  // Speed up adaptation for test
  agc.decay = 3276; // 0.1 rate (fast)

  eif_agc_q15_process(&agc, input_long, output_long, AGC_TEST_LEN);

  // Gain starts at 1.0. Gain boosts signal toward Target (16384).
  // Last output should be significantly > Input (3276).
  TEST_ASSERT_TRUE(output_long[AGC_TEST_LEN - 1] >
                   input_long[AGC_TEST_LEN - 1]);
  return true;
}

// ===================================
// Smoothing Tests
// ===================================
bool test_median_q15(void) {
  eif_median_q15_t mf;
  eif_median_q15_init(&mf, 5);

  eif_median_q15_update(&mf, 10);
  eif_median_q15_update(&mf, 20);
  eif_median_q15_update(&mf, 100);
  eif_median_q15_update(&mf, 20);
  q15_t out = eif_median_q15_update(&mf, 10);

  // Sorted: 10, 10, 20, 20, 100. Median 20.
  TEST_ASSERT_EQUAL_INT(20, out);
  return true;
}

bool test_ma_q15(void) {
  eif_ma_q15_t ma;
  eif_ma_q15_init(&ma, 4);

  eif_ma_q15_update(&ma, 100);
  eif_ma_q15_update(&ma, 100);
  eif_ma_q15_update(&ma, 200);
  q15_t out = eif_ma_q15_update(&ma, 200);

  // (100+100+200+200)/4 = 150.
  TEST_ASSERT_EQUAL_INT(150, out);
  return true;
}

// ===================================
// Resample Tests
// ===================================
bool test_resample_q15_linear(void) {
  q15_t input[2] = {100, 200}; // Slope 100
  q15_t output[3];             // Upsample 2 -> 3

  eif_resample_linear_q15(input, 2, output, 3);

  TEST_ASSERT_Q15_WITHIN(1, 100, output[0]);
  TEST_ASSERT_Q15_WITHIN(1, 150, output[1]);
  TEST_ASSERT_Q15_WITHIN(1, 200, output[2]);
  return true;
}

// ===================================
// Beamformer Tests
// ===================================
bool test_beamformer_q15(void) {
  eif_beamformer_q15_t bf;
  // 0 delay
  eif_beamformer_q15_init(&bf, 0.1f, 16000, 0);

  q15_t input[4] = {100, 100, 200, 200}; // L, R, L, R
  q15_t output[2];

  eif_beamformer_q15_process_stereo(&bf, input, output, 2);

  TEST_ASSERT_EQUAL_INT(100, output[0]);
  TEST_ASSERT_EQUAL_INT(200, output[1]);
  return true;
}

BEGIN_TEST_SUITE(test_dsp_fixed_suite)
RUN_TEST(test_pid_q15_basic);
RUN_TEST(test_pid_q15_integral);
RUN_TEST(test_agc_q15_limiting);
RUN_TEST(test_median_q15);
RUN_TEST(test_ma_q15);
RUN_TEST(test_resample_q15_linear);
RUN_TEST(test_beamformer_q15);
END_TEST_SUITE()

int main(void) { return test_dsp_fixed_suite(); }
