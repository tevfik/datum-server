/**
 * @file test_logging.c
 * @brief Unit tests for EU AI Act Logging Compliance
 */

#include "../framework/eif_test_runner.h"
#include "eif_logging.h"
#include <string.h>

// Mock callback state
static char last_message[256];
static eif_log_level_t last_level;
static int callback_count = 0;

// Mock callback function
static void mock_log_callback(eif_log_level_t level, const char *message) {
  last_level = level;
  strncpy(last_message, message, sizeof(last_message) - 1);
  callback_count++;
}

// Reset state
static void setup(void) {
  memset(last_message, 0, sizeof(last_message));
  last_level = (eif_log_level_t)-1;
  callback_count = 0;
  eif_log_init(mock_log_callback, EIF_LOG_TRACE);
}

// Tests

bool test_log_basic(void) {
  setup();
  eif_log(EIF_LOG_INFO, "Test message %d", 123);

  TEST_ASSERT_EQUAL_INT(1, callback_count);
  TEST_ASSERT_EQUAL_INT(EIF_LOG_INFO, last_level);
  TEST_ASSERT_EQUAL_STRING("Test message 123", last_message);
  return true;
}

bool test_log_level_filtering(void) {
  setup();
  eif_log_set_level(EIF_LOG_WARN);

  // Should pass
  eif_log(EIF_LOG_ERROR, "Error msg");
  TEST_ASSERT_EQUAL_INT(1, callback_count);

  // Should be ignored
  eif_log(EIF_LOG_INFO, "Info msg");
  TEST_ASSERT_EQUAL_INT(1, callback_count);
  return true;
}

bool test_log_macros(void) {
  setup();
  EIF_LOG_ERROR("Fatal error");

  TEST_ASSERT_EQUAL_INT(1, callback_count);
  // Macro adds prefix "[ERR]  "
  TEST_ASSERT_TRUE(strstr(last_message, "[ERR]") != NULL);
  TEST_ASSERT_TRUE(strstr(last_message, "Fatal error") != NULL);
  return true;
}

bool test_log_no_init(void) {
  // Reset internal state to NULL via re-init with NULL
  eif_log_init(NULL, EIF_LOG_INFO);
  callback_count = 0;

  eif_log(EIF_LOG_INFO, "Should not crash");
  TEST_ASSERT_EQUAL_INT(0, callback_count);
  return true;
}

// Test suite registration
BEGIN_TEST_SUITE(run_logging_tests)
RUN_TEST(test_log_basic);
RUN_TEST(test_log_level_filtering);
RUN_TEST(test_log_macros);
RUN_TEST(test_log_no_init);
END_TEST_SUITE()
