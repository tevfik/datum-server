#ifndef EIF_TEST_RUNNER_H
#define EIF_TEST_RUNNER_H

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// Colors
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_RESET "\x1b[0m"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition)                                                 \
  do {                                                                         \
    if (!(condition)) {                                                        \
      printf(ANSI_COLOR_RED "FAIL" ANSI_COLOR_RESET " at %s:%d\n", __FILE__,   \
             __LINE__);                                                        \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define TEST_ASSERT_EQUAL_FLOAT(expected, actual, epsilon)                     \
  do {                                                                         \
    if (fabs((expected) - (actual)) > (epsilon)) {                             \
      printf(ANSI_COLOR_RED "FAIL" ANSI_COLOR_RESET                            \
                            " at %s:%d: Expected %f, Got %f\n",                \
             __FILE__, __LINE__, (float)(expected), (float)(actual));          \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define TEST_ASSERT_FLOAT_WITHIN(epsilon, expected, actual)                    \
  TEST_ASSERT_EQUAL_FLOAT(expected, actual, epsilon)

#define TEST_ASSERT_TRUE(condition) TEST_ASSERT(condition)

#define TEST_ASSERT_EQUAL_INT(expected, actual)                                \
  do {                                                                         \
    if ((expected) != (actual)) {                                              \
      printf(ANSI_COLOR_RED "FAIL" ANSI_COLOR_RESET                            \
                            " at %s:%d: Expected %d, Got %d\n",                \
             __FILE__, __LINE__, (int)(expected), (int)(actual));              \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define TEST_ASSERT_EQUAL_STRING(expected, actual)                             \
  do {                                                                         \
    const char *exp = (expected);                                              \
    const char *act = (actual);                                                \
    if (strcmp(exp, act) != 0) {                                               \
      printf(ANSI_COLOR_RED "FAIL" ANSI_COLOR_RESET                            \
                            " at %s:%d: Expected '%s', Got '%s'\n",            \
             __FILE__, __LINE__, exp, act);                                    \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define TEST_ASSERT_NOT_NULL(pointer)                                          \
  do {                                                                         \
    if ((pointer) == NULL) {                                                   \
      printf(ANSI_COLOR_RED "FAIL" ANSI_COLOR_RESET                            \
                            " at %s:%d: Expected not NULL\n",                  \
             __FILE__, __LINE__);                                              \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define RUN_TEST(test_func)                                                    \
  do {                                                                         \
    printf("Running %s... ", #test_func);                                      \
    tests_run++;                                                               \
    if (test_func()) {                                                         \
      printf(ANSI_COLOR_GREEN "PASS" ANSI_COLOR_RESET "\n");                   \
      tests_passed++;                                                          \
    } else {                                                                   \
      tests_failed++;                                                          \
    }                                                                          \
  } while (0)

#define BEGIN_TEST_SUITE(name)                                                 \
  int name() {                                                                 \
    printf("=== Running Test Suite: %s ===\n", #name);

#define END_TEST_SUITE()                                                       \
  printf("Results: %d Run, %d Passed, %d Failed\n", tests_run, tests_passed,   \
         tests_failed);                                                        \
  return (tests_failed == 0) ? 0 : 1;                                          \
  }

typedef bool (*test_func_t)(void);

#endif // EIF_TEST_RUNNER_H
