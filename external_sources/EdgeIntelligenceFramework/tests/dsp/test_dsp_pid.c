/**
 * @file test_dsp_pid.c
 * @brief Unit tests for PID Controller
 */

#include "../framework/eif_test_runner.h"
#include "eif_dsp_pid.h"

// Test PID initialization
bool test_pid_init(void) {
  eif_dsp_pid_t pid;
  eif_dsp_pid_init(&pid, 1.0f, 0.1f, 0.01f, -100.0f, 100.0f);

  TEST_ASSERT_EQUAL_FLOAT(1.0f, pid.Kp, 0.0001f);
  TEST_ASSERT_EQUAL_FLOAT(0.1f, pid.Ki, 0.0001f);
  TEST_ASSERT_EQUAL_FLOAT(0.01f, pid.Kd, 0.0001f);
  TEST_ASSERT_EQUAL_FLOAT(-100.0f, pid.out_min, 0.0001f);
  TEST_ASSERT_EQUAL_FLOAT(100.0f, pid.out_max, 0.0001f);

  return true;
}

// Test P-only response
bool test_pid_p_only(void) {
  eif_dsp_pid_t pid;
  eif_dsp_pid_init(&pid, 2.0f, 0.0f, 0.0f, -100.0f, 100.0f);

  float error = 5.0f;
  float output = eif_dsp_pid_update(&pid, error, 0.01f);

  // P-only: output = Kp * error = 2.0 * 5.0 = 10.0
  TEST_ASSERT_EQUAL_FLOAT(10.0f, output, 0.01f);

  return true;
}

// Test output clamping
bool test_pid_clamping(void) {
  eif_dsp_pid_t pid;
  eif_dsp_pid_init(&pid, 100.0f, 0.0f, 0.0f, -50.0f, 50.0f);

  float output = eif_dsp_pid_update(&pid, 10.0f, 0.01f);

  // Kp*error = 100*10 = 1000, but should clamp to 50
  TEST_ASSERT_EQUAL_FLOAT(50.0f, output, 0.0001f);

  output = eif_dsp_pid_update(&pid, -10.0f, 0.01f);
  TEST_ASSERT_EQUAL_FLOAT(-50.0f, output, 0.0001f);

  return true;
}

// Test integrator accumulation
bool test_pid_integrator(void) {
  eif_dsp_pid_t pid;
  eif_dsp_pid_init(&pid, 0.0f, 10.0f, 0.0f, -100.0f, 100.0f);

  float dt = 0.1f;
  float error = 1.0f;

  // First update: integrator = Ki * error * dt = 10 * 1 * 0.1 = 1.0
  float output1 = eif_dsp_pid_update(&pid, error, dt);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, output1, 0.01f);

  // Second update: integrator = 1.0 + 1.0 = 2.0
  float output2 = eif_dsp_pid_update(&pid, error, dt);
  TEST_ASSERT_EQUAL_FLOAT(2.0f, output2, 0.01f);

  return true;
}

// Test reset
bool test_pid_reset(void) {
  eif_dsp_pid_t pid;
  eif_dsp_pid_init(&pid, 1.0f, 1.0f, 1.0f, -100.0f, 100.0f);

  // Accumulate some state
  eif_dsp_pid_update(&pid, 10.0f, 0.1f);
  eif_dsp_pid_update(&pid, 10.0f, 0.1f);

  // Reset
  eif_dsp_pid_reset(&pid);

  TEST_ASSERT_EQUAL_FLOAT(0.0f, pid.integrator, 0.0001f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, pid.prev_error, 0.0001f);

  return true;
}

BEGIN_TEST_SUITE(run_pid_tests)
RUN_TEST(test_pid_init);
RUN_TEST(test_pid_p_only);
RUN_TEST(test_pid_clamping);
RUN_TEST(test_pid_integrator);
RUN_TEST(test_pid_reset);
END_TEST_SUITE()
