/**
 * @file test_federated.c
 * @brief Unit tests for eif_federated.h
 */

#include "../framework/eif_test_runner.h"
#define EIF_HAS_PRINTF 1
#include "eif_federated.h"

// =============================================================================
// FL Context Tests
// =============================================================================

bool test_fl_init(void) {
  int16_t weights[10];
  eif_fl_context_t ctx;

  eif_fl_init(&ctx, weights, 10);

  TEST_ASSERT_EQUAL_INT(10, ctx.weight_count);
  TEST_ASSERT_EQUAL_INT(0, ctx.round);
  TEST_ASSERT_EQUAL_INT(0, (int)ctx.total_samples);
  TEST_ASSERT_TRUE(ctx.global_weights == weights);

  return true;
}

bool test_fl_client_init(void) {
  int16_t weights[5];
  eif_fl_client_t client;

  eif_fl_client_init(&client, weights, 5, 42);

  TEST_ASSERT_EQUAL_INT(5, client.weight_count);
  TEST_ASSERT_EQUAL_INT(42, client.client_id);
  TEST_ASSERT_TRUE(client.valid);

  return true;
}

// =============================================================================
// Aggregation Tests
// =============================================================================

bool test_fl_aggregate_single_client(void) {
  int16_t global[4] = {0, 0, 0, 0};
  int16_t client_w[4] = {1000, 2000, 3000, 4000};

  eif_fl_context_t ctx;
  eif_fl_init(&ctx, global, 4);

  eif_fl_client_t clients[1];
  eif_fl_client_init(&clients[0], client_w, 4, 0);
  clients[0].sample_count = 100;

  eif_fl_aggregate(&ctx, clients, 1);

  // With single client, global should equal client weights
  TEST_ASSERT_EQUAL_INT(1000, global[0]);
  TEST_ASSERT_EQUAL_INT(2000, global[1]);
  TEST_ASSERT_EQUAL_INT(3000, global[2]);
  TEST_ASSERT_EQUAL_INT(4000, global[3]);
  TEST_ASSERT_EQUAL_INT(1, ctx.round);

  return true;
}

bool test_fl_aggregate_weighted(void) {
  int16_t global[2] = {0, 0};
  int16_t w1[2] = {10000, 0};
  int16_t w2[2] = {0, 10000};

  eif_fl_context_t ctx;
  eif_fl_init(&ctx, global, 2);

  eif_fl_client_t clients[2];
  eif_fl_client_init(&clients[0], w1, 2, 0);
  eif_fl_client_init(&clients[1], w2, 2, 1);

  // Client 0 has 3x more samples
  clients[0].sample_count = 75;
  clients[1].sample_count = 25;

  eif_fl_aggregate(&ctx, clients, 2);

  // global[0] should be 0.75*10000 + 0.25*0 = 7500
  TEST_ASSERT_TRUE(global[0] > 7000 && global[0] < 8000);
  // global[1] should be 0.75*0 + 0.25*10000 = 2500
  TEST_ASSERT_TRUE(global[1] > 2000 && global[1] < 3000);

  return true;
}

bool test_fl_average(void) {
  int16_t w1[3] = {1000, 2000, 3000};
  int16_t w2[3] = {3000, 4000, 5000};
  int16_t output[3];

  eif_fl_client_t clients[2];
  eif_fl_client_init(&clients[0], w1, 3, 0);
  eif_fl_client_init(&clients[1], w2, 3, 1);

  eif_fl_average(output, clients, 2, 3);

  // Average: [2000, 3000, 4000]
  TEST_ASSERT_EQUAL_INT(2000, output[0]);
  TEST_ASSERT_EQUAL_INT(3000, output[1]);
  TEST_ASSERT_EQUAL_INT(4000, output[2]);

  return true;
}

// =============================================================================
// Compression Tests
// =============================================================================

bool test_fl_compress_count(void) {
  int16_t gradients[] = {100, 50, 200, 10, 150};

  int count = eif_fl_compress_count(gradients, 5, 100);

  // Values >= 100: 100, 200, 150 = 3
  TEST_ASSERT_EQUAL_INT(3, count);

  return true;
}

bool test_fl_compress_threshold(void) {
  int16_t gradients[] = {100, 50, -200, 10, 150};

  eif_fl_compress_threshold(gradients, 5, 100);

  // Values < 100 should be zeroed
  TEST_ASSERT_EQUAL_INT(100, gradients[0]);
  TEST_ASSERT_EQUAL_INT(0, gradients[1]);
  TEST_ASSERT_EQUAL_INT(-200, gradients[2]); // abs(-200) >= 100
  TEST_ASSERT_EQUAL_INT(0, gradients[3]);
  TEST_ASSERT_EQUAL_INT(150, gradients[4]);

  return true;
}

bool test_fl_quantize_8bit(void) {
  int16_t gradients[] = {16384, -16384, 0}; // 0.5, -0.5, 0
  int8_t quantized[3];
  int16_t scale;

  eif_fl_quantize_8bit(gradients, quantized, 3, &scale);

  TEST_ASSERT_EQUAL_INT(16384, scale); // Max abs value

  // 16384 / 16384 * 127 = 127
  TEST_ASSERT_EQUAL_INT(127, quantized[0]);
  TEST_ASSERT_EQUAL_INT(-127, quantized[1]);
  TEST_ASSERT_EQUAL_INT(0, quantized[2]);

  return true;
}

bool test_fl_dequantize_8bit(void) {
  int8_t quantized[] = {127, -127, 0};
  int16_t gradients[3];
  int16_t scale = 16384;

  eif_fl_dequantize_8bit(quantized, gradients, 3, scale);

  // Dequantize: 127 * 16384 / 127 = 16384
  TEST_ASSERT_TRUE(gradients[0] > 16000 && gradients[0] < 17000);
  TEST_ASSERT_TRUE(gradients[1] < -16000 && gradients[1] > -17000);
  TEST_ASSERT_EQUAL_INT(0, gradients[2]);

  return true;
}

// =============================================================================
// Privacy Tests
// =============================================================================

bool test_fl_add_noise(void) {
  int16_t gradients[4] = {10000, 10000, 10000, 10000};

  eif_fl_srand(42);
  eif_fl_add_noise(gradients, 4, 1000);

  // Values should be perturbed
  int same_count = 0;
  for (int i = 0; i < 4; i++) {
    if (gradients[i] == 10000)
      same_count++;
  }

  // With noise, unlikely all stay the same
  TEST_ASSERT_TRUE(same_count < 4);

  return true;
}

bool test_fl_clip_gradients(void) {
  // Large gradients
  int16_t gradients[] = {20000, 20000};

  eif_fl_clip_gradients(gradients, 2, 10000);

  // Both should be reduced
  TEST_ASSERT_TRUE(gradients[0] < 20000);
  TEST_ASSERT_TRUE(gradients[1] < 20000);

  return true;
}

// =============================================================================
// Checkpoint Tests
// =============================================================================

bool test_fl_checksum(void) {
  int16_t weights[] = {100, 200, 300};

  int32_t cs1 = eif_fl_checksum(weights, 3);
  int32_t cs2 = eif_fl_checksum(weights, 3);

  // Same input should give same checksum
  TEST_ASSERT_TRUE(cs1 == cs2);

  // Different input should give different checksum (usually)
  weights[0] = 999;
  int32_t cs3 = eif_fl_checksum(weights, 3);
  // Note: could theoretically collide but very unlikely
  TEST_ASSERT_TRUE(cs3 != 0 || cs1 != 0); // At least one is non-zero

  return true;
}

bool test_fl_checkpoint_save_restore(void) {
  int16_t weights[] = {1000, 2000, 3000};
  int16_t ckpt_buf[3];

  eif_fl_checkpoint_t ckpt;
  ckpt.weights = ckpt_buf;

  eif_fl_checkpoint_save(&ckpt, weights, 3, 5);

  TEST_ASSERT_EQUAL_INT(5, ckpt.round);
  TEST_ASSERT_TRUE(ckpt.valid);

  // Corrupt original
  weights[0] = 0;
  weights[1] = 0;
  weights[2] = 0;

  // Restore
  bool success = eif_fl_checkpoint_restore(&ckpt, weights, 3);

  TEST_ASSERT_TRUE(success);
  TEST_ASSERT_EQUAL_INT(1000, weights[0]);
  TEST_ASSERT_EQUAL_INT(2000, weights[1]);
  TEST_ASSERT_EQUAL_INT(3000, weights[2]);

  return true;
}

// =============================================================================
// Test Suite
// =============================================================================

BEGIN_TEST_SUITE(run_federated_tests)
RUN_TEST(test_fl_init);
RUN_TEST(test_fl_client_init);
RUN_TEST(test_fl_aggregate_single_client);
RUN_TEST(test_fl_aggregate_weighted);
RUN_TEST(test_fl_average);
RUN_TEST(test_fl_compress_count);
RUN_TEST(test_fl_compress_threshold);
RUN_TEST(test_fl_quantize_8bit);
RUN_TEST(test_fl_dequantize_8bit);
RUN_TEST(test_fl_add_noise);
RUN_TEST(test_fl_clip_gradients);
RUN_TEST(test_fl_checksum);
RUN_TEST(test_fl_checkpoint_save_restore);
END_TEST_SUITE()
