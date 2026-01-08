/**
 * @file test_assert.c
 * @brief Unit tests for eif_assert.h validation macros
 */

#include "../framework/eif_test_runner.h"
#include "eif_assert.h"
#include "eif_status.h"

// =============================================================================
// Test Helper Functions (using validation macros)
// =============================================================================

static eif_status_t func_with_ptr_validation(const void *ptr) {
  EIF_CRITICAL_PTR(ptr);
  return EIF_STATUS_OK;
}

static eif_status_t func_with_range_check(int val) {
  EIF_CRITICAL_CHECK(val >= 0 && val <= 100, EIF_STATUS_INVALID_ARGUMENT);
  return EIF_STATUS_OK;
}

static eif_status_t inner_func(bool should_fail) {
  if (should_fail)
    return EIF_STATUS_ERROR;
  return EIF_STATUS_OK;
}

static eif_status_t outer_func_with_try(bool should_fail) {
  EIF_TRY(inner_func(should_fail));
  return EIF_STATUS_OK;
}

// =============================================================================
// Tests
// =============================================================================

bool test_critical_ptr_null(void) {
  eif_status_t result = func_with_ptr_validation(NULL);
  TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, result);
  return true;
}

bool test_critical_ptr_valid(void) {
  int value = 42;
  eif_status_t result = func_with_ptr_validation(&value);
  TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, result);
  return true;
}

bool test_critical_check_valid(void) {
  TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, func_with_range_check(50));
  TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, func_with_range_check(0));
  TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, func_with_range_check(100));
  return true;
}

bool test_critical_check_invalid(void) {
  TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, func_with_range_check(-1));
  TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT,
                        func_with_range_check(101));
  return true;
}

bool test_try_propagates_success(void) {
  eif_status_t result = outer_func_with_try(false);
  TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, result);
  return true;
}

bool test_try_propagates_error(void) {
  eif_status_t result = outer_func_with_try(true);
  TEST_ASSERT_EQUAL_INT(EIF_STATUS_ERROR, result);
  return true;
}

bool test_static_assert_compiles(void) {
  // If this compiles, static assert works
  EIF_STATIC_ASSERT(sizeof(int) >= 2, "int_too_small");
  EIF_STATIC_ASSERT(sizeof(void *) >= 4, "pointer_too_small");
  return true;
}

// =============================================================================
// Test Suite
// =============================================================================

BEGIN_TEST_SUITE(run_assert_tests)
RUN_TEST(test_critical_ptr_null);
RUN_TEST(test_critical_ptr_valid);
RUN_TEST(test_critical_check_valid);
RUN_TEST(test_critical_check_invalid);
RUN_TEST(test_try_propagates_success);
RUN_TEST(test_try_propagates_error);
RUN_TEST(test_static_assert_compiles);
END_TEST_SUITE()
