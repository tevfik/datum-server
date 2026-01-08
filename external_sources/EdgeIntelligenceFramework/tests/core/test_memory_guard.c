/**
 * @file test_memory_guard.c
 * @brief Unit tests for eif_memory_guard.h
 */

#include "../framework/eif_test_runner.h"
#include "eif_memory_guard.h"

// =============================================================================
// Pool Tests
// =============================================================================

bool test_guard_pool_init(void) {
  uint8_t buffer[1024];
  eif_guard_pool_t pool;

  eif_status_t status = eif_guard_pool_init(&pool, buffer, sizeof(buffer));

  TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
  TEST_ASSERT_EQUAL_INT(EIF_CANARY_VALUE, pool.header_canary);
  TEST_ASSERT_EQUAL_INT(EIF_CANARY_VALUE, pool.footer_canary);
  TEST_ASSERT_EQUAL_INT(0, pool.alloc_count);
  return true;
}

bool test_guard_pool_init_null(void) {
  eif_status_t status = eif_guard_pool_init(NULL, NULL, 0);
  TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, status);
  return true;
}

bool test_guard_pool_init_too_small(void) {
  uint8_t buffer[32]; // Too small
  eif_guard_pool_t pool;

  eif_status_t status = eif_guard_pool_init(&pool, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, status);
  return true;
}

bool test_guard_pool_check_valid(void) {
  uint8_t buffer[1024];
  eif_guard_pool_t pool;

  eif_guard_pool_init(&pool, buffer, sizeof(buffer));

  TEST_ASSERT_TRUE(eif_guard_pool_check(&pool));
  return true;
}

bool test_guard_pool_check_corrupted_header(void) {
  uint8_t buffer[1024];
  eif_guard_pool_t pool;

  eif_guard_pool_init(&pool, buffer, sizeof(buffer));

  // Corrupt header
  pool.header_canary = 0x12345678;

  TEST_ASSERT_TRUE(!eif_guard_pool_check(&pool));
  return true;
}

bool test_guard_pool_check_corrupted_footer(void) {
  uint8_t buffer[1024];
  eif_guard_pool_t pool;

  eif_guard_pool_init(&pool, buffer, sizeof(buffer));

  // Corrupt footer
  pool.footer_canary = 0x12345678;

  TEST_ASSERT_TRUE(!eif_guard_pool_check(&pool));
  return true;
}

// =============================================================================
// Allocation Tests
// =============================================================================

bool test_guard_alloc_basic(void) {
  uint8_t buffer[2048];
  eif_guard_pool_t pool;

  eif_guard_pool_init(&pool, buffer, sizeof(buffer));

  float *data = EIF_GUARD_ALLOC_ARRAY(&pool, float, 64);

  TEST_ASSERT_TRUE(data != NULL);
  TEST_ASSERT_EQUAL_INT(1, pool.alloc_count);

  // Should still be valid
  TEST_ASSERT_TRUE(eif_guard_pool_check(&pool));
  return true;
}

bool test_guard_alloc_multiple(void) {
  uint8_t buffer[4096];
  eif_guard_pool_t pool;

  eif_guard_pool_init(&pool, buffer, sizeof(buffer));

  void *ptrs[5];
  for (int i = 0; i < 5; i++) {
    ptrs[i] = eif_guard_alloc(&pool, 128);
    TEST_ASSERT_TRUE(ptrs[i] != NULL);
  }

  TEST_ASSERT_EQUAL_INT(5, pool.alloc_count);
  TEST_ASSERT_TRUE(eif_guard_pool_check(&pool));
  return true;
}

bool test_guard_alloc_null_pool(void) {
  void *ptr = eif_guard_alloc(NULL, 64);
  TEST_ASSERT_TRUE(ptr == NULL);
  return true;
}

bool test_guard_alloc_zero_size(void) {
  uint8_t buffer[1024];
  eif_guard_pool_t pool;

  eif_guard_pool_init(&pool, buffer, sizeof(buffer));

  void *ptr = eif_guard_alloc(&pool, 0);
  TEST_ASSERT_TRUE(ptr == NULL);
  return true;
}

// =============================================================================
// Stack Guard Tests
// =============================================================================

bool test_stack_guard_init(void) {
  eif_stack_guard_t guard;
  EIF_STACK_GUARD_INIT(guard);

  TEST_ASSERT_TRUE(EIF_STACK_GUARD_CHECK(guard));
  return true;
}

bool test_stack_guard_corruption(void) {
  eif_stack_guard_t guard;
  EIF_STACK_GUARD_INIT(guard);

  // Simulate corruption
  guard.canary = 0x12345678;

  TEST_ASSERT_TRUE(!EIF_STACK_GUARD_CHECK(guard));
  return true;
}

// =============================================================================
// Macro Tests
// =============================================================================

bool test_guarded_pool_macro(void) {
  // Use macro to declare pool
  EIF_GUARDED_POOL(test_pool, 2048);

  eif_status_t status = eif_guard_pool_init_static(test_pool);
  TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
  TEST_ASSERT_TRUE(eif_guard_pool_check(&test_pool));

  return true;
}

// =============================================================================
// Test Suite
// =============================================================================

BEGIN_TEST_SUITE(run_memory_guard_tests)
RUN_TEST(test_guard_pool_init);
RUN_TEST(test_guard_pool_init_null);
RUN_TEST(test_guard_pool_init_too_small);
RUN_TEST(test_guard_pool_check_valid);
RUN_TEST(test_guard_pool_check_corrupted_header);
RUN_TEST(test_guard_pool_check_corrupted_footer);
RUN_TEST(test_guard_alloc_basic);
RUN_TEST(test_guard_alloc_multiple);
RUN_TEST(test_guard_alloc_null_pool);
RUN_TEST(test_guard_alloc_zero_size);
RUN_TEST(test_stack_guard_init);
RUN_TEST(test_stack_guard_corruption);
RUN_TEST(test_guarded_pool_macro);
END_TEST_SUITE()
