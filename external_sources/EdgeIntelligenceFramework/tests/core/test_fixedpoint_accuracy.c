#include "../framework/eif_test_runner.h"
#include "eif_fixedpoint.h"
#include <math.h>
#include <stdio.h>

// Helper to convert Q15 to float for comparison
static float q15_to_float(q15_t x) { return (float)x / 32768.0f; }

static q15_t float_to_q15(float x) {
  if (x >= 1.0f)
    return EIF_Q15_MAX;
  if (x <= -1.0f)
    return EIF_Q15_MIN;
  return (q15_t)(x * 32768.0f);
}

bool test_log_accuracy(void) {
  printf("\nTesting Log Accuracy:\n");
  float max_error = 0.0f;
  float total_error = 0.0f;
  int count = 0;

  // Test range 0.1 to 1.0 (Q15 3277 to 32767)
  for (int i = 3277; i <= 32767; i += 100) {
    q15_t x_q15 = (q15_t)i;
    float x_f = q15_to_float(x_q15);

    // ln(x)
    if (x_f < 0.37f)
      continue; // Skip range where ln(x) < -1

    float expected = logf(x_f);
    q15_t result_q15 = eif_q15_log(x_q15);
    float result_f = q15_to_float(result_q15);

    float error = fabsf(result_f - expected);
    if (error > max_error)
      max_error = error;
    total_error += error;
    count++;
  }

  printf("Log Max Error: %f\n", max_error);
  printf("Log Avg Error: %f\n", total_error / count);

  // We don't fail here yet, strictly speaking, as we are measuring baseline.
  // But let's assert it keeps running.
  return true;
}

bool test_exp_accuracy(void) {
  printf("\nTesting Exp Accuracy:\n");
  float max_error = 0.0f;
  float total_error = 0.0f;
  int count = 0;

  // Test range -1.0 to 0.0
  for (int i = -32768; i <= 0; i += 100) {
    q15_t x_q15 = (q15_t)i;
    float x_f = q15_to_float(x_q15);

    float expected = expf(x_f);
    q15_t result_q15 = eif_q15_exp(x_q15);
    float result_f = q15_to_float(result_q15);

    float error = fabsf(result_f - expected);
    if (error > max_error)
      max_error = error;
    total_error += error;
    count++;
  }

  printf("Exp Max Error: %f\n", max_error);
  printf("Exp Avg Error: %f\n", total_error / count);

  return true;
}

int main(void) {
  printf("=== Fixed Point Accuracy Benchmark ===\n");
  RUN_TEST(test_log_accuracy);
  RUN_TEST(test_exp_accuracy);

  printf("Results: %d Run, %d Passed, %d Failed\n", tests_run, tests_passed,
         tests_failed);
  return (tests_failed == 0) ? 0 : 1;
}
