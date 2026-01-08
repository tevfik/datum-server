/**
 * @file test_dsp_smooth.c
 * @brief Unit tests for smoothing filters (EMA, Median, MA)
 */

#include "../framework/eif_test_runner.h"
#include "eif_dsp_smooth.h"
#include <math.h>

// Test EMA initialization
bool test_ema_init(void) {
  eif_ema_t ema;
  eif_ema_init(&ema, 0.3f);

  TEST_ASSERT_EQUAL_FLOAT(0.3f, ema.alpha, 0.0001f);
  TEST_ASSERT(ema.initialized == false);

  return true;
}

// Test EMA smoothing behavior
bool test_ema_update(void) {
  eif_ema_t ema;
  eif_ema_init(&ema, 0.5f);

  // First sample sets output directly
  float out = eif_ema_update(&ema, 10.0f);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, out, 0.001f);

  // Second sample: 0.5 * 0 + 0.5 * 10 = 5
  out = eif_ema_update(&ema, 0.0f);
  TEST_ASSERT_EQUAL_FLOAT(5.0f, out, 0.001f);

  // Third sample: 0.5 * 10 + 0.5 * 5 = 7.5
  out = eif_ema_update(&ema, 10.0f);
  TEST_ASSERT_EQUAL_FLOAT(7.5f, out, 0.001f);

  return true;
}

// Test Median filter removes spikes
bool test_median_spike_removal(void) {
  eif_median_t mf;
  eif_median_init(&mf, 3); // 3-sample window

  // Normal values with a spike
  float out;
  out = eif_median_update(&mf, 5.0f);   // [5] -> median 5
  out = eif_median_update(&mf, 100.0f); // [5, 100] -> median 100 or 5
  out = eif_median_update(&mf, 6.0f);   // [5, 100, 6] -> median 6

  // After spike, median should be normal value
  TEST_ASSERT(out <= 10.0f); // Should be 6, not 100

  // Continue with normal
  out = eif_median_update(&mf, 7.0f); // [100, 6, 7] -> median 7
  TEST_ASSERT(out <= 10.0f);

  return true;
}

// Test Moving Average convergence
bool test_ma_convergence(void) {
  eif_ma_t ma;
  eif_ma_init(&ma, 4);

  // Add 4 samples of value 10
  for (int i = 0; i < 4; i++) {
    eif_ma_update(&ma, 10.0f);
  }

  float out = eif_ma_update(&ma, 10.0f);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, out, 0.001f);

  // Average of [10, 10, 10, 20] = 12.5
  out = eif_ma_update(&ma, 20.0f);
  TEST_ASSERT_EQUAL_FLOAT(12.5f, out, 0.001f);

  return true;
}

// Test MA reset
bool test_ma_reset(void) {
  eif_ma_t ma;
  eif_ma_init(&ma, 4);

  eif_ma_update(&ma, 10.0f);
  eif_ma_update(&ma, 20.0f);

  eif_ma_reset(&ma);
  TEST_ASSERT_EQUAL_INT(0, ma.count);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, ma.sum, 0.001f);

  return true;
}

// Test Rate Limiter
bool test_rate_limiter(void) {
  eif_rate_limiter_t rl;
  eif_rate_limiter_init(&rl, 2.0f); // Max 2 per sample

  // First sample sets directly
  float out = eif_rate_limiter_update(&rl, 10.0f);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, out, 0.001f);

  // Try to jump to 20, but limited to +2
  out = eif_rate_limiter_update(&rl, 20.0f);
  TEST_ASSERT_EQUAL_FLOAT(12.0f, out, 0.001f);

  // Another step toward 20
  out = eif_rate_limiter_update(&rl, 20.0f);
  TEST_ASSERT_EQUAL_FLOAT(14.0f, out, 0.001f);

  // Try to jump down to 0, but limited to -2
  out = eif_rate_limiter_update(&rl, 0.0f);
  TEST_ASSERT_EQUAL_FLOAT(12.0f, out, 0.001f);

  return true;
}

// Test Hysteresis (Schmitt Trigger)
bool test_hysteresis(void) {
  eif_hysteresis_t hyst;
  eif_hysteresis_init(&hyst, 3.0f, 7.0f); // Low < 3, High > 7

  // Start LOW
  TEST_ASSERT(hyst.state == false);

  // Input 5 - between thresholds, stay LOW
  bool out = eif_hysteresis_update(&hyst, 5.0f);
  TEST_ASSERT(out == false);

  // Input 8 - above high threshold, switch to HIGH
  out = eif_hysteresis_update(&hyst, 8.0f);
  TEST_ASSERT(out == true);

  // Input 5 - between thresholds, stay HIGH
  out = eif_hysteresis_update(&hyst, 5.0f);
  TEST_ASSERT(out == true);

  // Input 2 - below low threshold, switch to LOW
  out = eif_hysteresis_update(&hyst, 2.0f);
  TEST_ASSERT(out == false);

  return true;
}

// Test Debounce
bool test_debounce(void) {
  eif_debounce_t db;
  eif_debounce_init(&db, 3); // Need 3 consecutive samples

  // Start with stable LOW
  TEST_ASSERT(db.stable_state == false);

  // Single HIGH doesn't change stable state
  bool out = eif_debounce_update(&db, true);
  TEST_ASSERT(out == false);

  // Second HIGH still not enough
  out = eif_debounce_update(&db, true);
  TEST_ASSERT(out == false);

  // Third HIGH - now triggers
  out = eif_debounce_update(&db, true);
  TEST_ASSERT(out == true);

  // Single LOW doesn't change back
  out = eif_debounce_update(&db, false);
  TEST_ASSERT(out == true);

  // Three LOWs to change back
  eif_debounce_update(&db, false);
  out = eif_debounce_update(&db, false);
  TEST_ASSERT(out == false);

  return true;
}

BEGIN_TEST_SUITE(run_smooth_tests)
RUN_TEST(test_ema_init);
RUN_TEST(test_ema_update);
RUN_TEST(test_median_spike_removal);
RUN_TEST(test_ma_convergence);
RUN_TEST(test_ma_reset);
RUN_TEST(test_rate_limiter);
RUN_TEST(test_hysteresis);
RUN_TEST(test_debounce);
END_TEST_SUITE()
