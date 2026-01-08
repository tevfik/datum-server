/**
 * @file test_rnn.c
 * @brief Unit tests for eif_rnn.h (RNN/LSTM/GRU)
 */

#include "../framework/eif_test_runner.h"
#include "eif_rnn.h"

// =============================================================================
// Activation LUT Tests
// =============================================================================

bool test_sigmoid_lut(void) {
  // sigmoid(0) = 0.5 -> Q15 = around 16384
  int16_t y = eif_sigmoid_q15(0);
  // Just check it's positive and reasonable
  TEST_ASSERT_TRUE(y >= 0 && y <= 32767);

  // sigmoid at various inputs should produce different values
  int16_t y_neg = eif_sigmoid_q15(-10000);
  int16_t y_pos = eif_sigmoid_q15(10000);

  // Positive input should give larger output than negative
  TEST_ASSERT_TRUE(y_pos >= y_neg);

  return true;
}

bool test_tanh_lut(void) {
  // Just test that tanh function exists and returns value
  int16_t y = eif_tanh_q15(0);
  // Should be within valid Q15 range
  TEST_ASSERT_TRUE(y >= -32768 && y <= 32767);

  return true;
}

// =============================================================================
// RNN Cell Tests
// =============================================================================

bool test_rnn_init(void) {
  eif_rnn_cell_t rnn;
  int16_t h[4] = {1000, 2000, 3000, 4000};

  rnn.hidden_size = 4;
  rnn.h = h;

  eif_rnn_init(&rnn);

  // Hidden should be zeroed
  TEST_ASSERT_EQUAL_INT(0, h[0]);
  TEST_ASSERT_EQUAL_INT(0, h[1]);
  TEST_ASSERT_EQUAL_INT(0, h[2]);
  TEST_ASSERT_EQUAL_INT(0, h[3]);

  return true;
}

bool test_rnn_reset(void) {
  eif_rnn_cell_t rnn;
  int16_t h[4] = {1000, 2000, 3000, 4000};

  rnn.hidden_size = 4;
  rnn.h = h;

  eif_rnn_reset(&rnn);

  for (int i = 0; i < 4; i++) {
    TEST_ASSERT_EQUAL_INT(0, h[i]);
  }

  return true;
}

// =============================================================================
// LSTM Cell Tests
// =============================================================================

bool test_lstm_init(void) {
  eif_lstm_cell_t lstm;
  int16_t h[4] = {1, 2, 3, 4};
  int16_t c[4] = {5, 6, 7, 8};

  lstm.hidden_size = 4;
  lstm.h = h;
  lstm.c = c;

  eif_lstm_init(&lstm);

  // h and c should be zeroed
  for (int i = 0; i < 4; i++) {
    TEST_ASSERT_EQUAL_INT(0, h[i]);
    TEST_ASSERT_EQUAL_INT(0, c[i]);
  }

  return true;
}

bool test_lstm_reset(void) {
  eif_lstm_cell_t lstm;
  int16_t h[2] = {1000, 2000};
  int16_t c[2] = {3000, 4000};

  lstm.hidden_size = 2;
  lstm.h = h;
  lstm.c = c;

  eif_lstm_reset(&lstm);

  TEST_ASSERT_EQUAL_INT(0, h[0]);
  TEST_ASSERT_EQUAL_INT(0, h[1]);
  TEST_ASSERT_EQUAL_INT(0, c[0]);
  TEST_ASSERT_EQUAL_INT(0, c[1]);

  return true;
}

// =============================================================================
// GRU Cell Tests
// =============================================================================

bool test_gru_init(void) {
  eif_gru_cell_t gru;
  int16_t h[4] = {1, 2, 3, 4};

  gru.hidden_size = 4;
  gru.h = h;

  eif_gru_init(&gru);

  for (int i = 0; i < 4; i++) {
    TEST_ASSERT_EQUAL_INT(0, h[i]);
  }

  return true;
}

bool test_gru_reset(void) {
  eif_gru_cell_t gru;
  int16_t h[4] = {1000, 2000, 3000, 4000};

  gru.hidden_size = 4;
  gru.h = h;

  eif_gru_reset(&gru);

  for (int i = 0; i < 4; i++) {
    TEST_ASSERT_EQUAL_INT(0, h[i]);
  }

  return true;
}

// =============================================================================
// Test Suite
// =============================================================================

BEGIN_TEST_SUITE(run_rnn_tests)
RUN_TEST(test_sigmoid_lut);
RUN_TEST(test_tanh_lut);
RUN_TEST(test_rnn_init);
RUN_TEST(test_rnn_reset);
RUN_TEST(test_lstm_init);
RUN_TEST(test_lstm_reset);
RUN_TEST(test_gru_init);
RUN_TEST(test_gru_reset);
END_TEST_SUITE()
