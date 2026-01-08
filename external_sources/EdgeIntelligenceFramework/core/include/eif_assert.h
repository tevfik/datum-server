/**
 * @file eif_assert.h
 * @brief EIF Assertion and Validation Macros
 *
 * Provides compile-time configurable validation for safety-critical
 * applications.
 *
 * Usage:
 *   Debug build:   cc -DEIF_ENABLE_VALIDATION ...
 *   Release build: cc ...  (validation stripped, zero overhead)
 *
 * Example:
 * @code
 * eif_status_t eif_conv2d(const float* input, ...) {
 *     EIF_VALIDATE_PTR(input);        // Stripped in release
 *     EIF_VALIDATE_RANGE(kernel, 1, 7);
 *     EIF_TRY(compute_internal(...)); // Propagate errors
 *     return EIF_STATUS_OK;
 * }
 * @endcode
 */

#ifndef EIF_ASSERT_H
#define EIF_ASSERT_H

#include "eif_status.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================

// Define EIF_ENABLE_VALIDATION for debug builds
// #define EIF_ENABLE_VALIDATION

// Define EIF_ENABLE_CRITICAL for always-on safety checks
#ifndef EIF_DISABLE_CRITICAL
#define EIF_ENABLE_CRITICAL 1
#endif

// =============================================================================
// Validation Macros (Optional - Debug Builds)
// =============================================================================

#ifdef EIF_ENABLE_VALIDATION

/**
 * @brief Validate a condition, return status if false
 */
#define EIF_VALIDATE(cond, status)                                             \
  do {                                                                         \
    if (!(cond))                                                               \
      return (status);                                                         \
  } while (0)

/**
 * @brief Validate pointer is not NULL
 */
#define EIF_VALIDATE_PTR(ptr)                                                  \
  EIF_VALIDATE((ptr) != NULL, EIF_STATUS_INVALID_ARGUMENT)

/**
 * @brief Validate value is in range [min, max]
 */
#define EIF_VALIDATE_RANGE(val, min, max)                                      \
  EIF_VALIDATE((val) >= (min) && (val) <= (max), EIF_STATUS_INVALID_ARGUMENT)

/**
 * @brief Validate size is positive
 */
#define EIF_VALIDATE_SIZE(size)                                                \
  EIF_VALIDATE((size) > 0, EIF_STATUS_INVALID_ARGUMENT)

/**
 * @brief Validate alignment (must be power of 2)
 */
#define EIF_VALIDATE_ALIGNMENT(align)                                          \
  EIF_VALIDATE(((align) > 0) && (((align) & ((align) - 1)) == 0),              \
               EIF_STATUS_INVALID_ARGUMENT)

/**
 * @brief Custom validation message (for debugging)
 */
#define EIF_VALIDATE_MSG(cond, status, msg)                                    \
  do {                                                                         \
    if (!(cond)) {                                                             \
      /* msg is ignored in minimal builds */                                   \
      (void)(msg);                                                             \
      return (status);                                                         \
    }                                                                          \
  } while (0)

#else // !EIF_ENABLE_VALIDATION - Release builds

#define EIF_VALIDATE(cond, status) ((void)0)
#define EIF_VALIDATE_PTR(ptr) ((void)0)
#define EIF_VALIDATE_RANGE(val, min, max) ((void)0)
#define EIF_VALIDATE_SIZE(size) ((void)0)
#define EIF_VALIDATE_ALIGNMENT(align) ((void)0)
#define EIF_VALIDATE_MSG(cond, status, msg) ((void)0)

#endif // EIF_ENABLE_VALIDATION

// =============================================================================
// Critical Checks (Always On - Minimal Set)
// =============================================================================

#if EIF_ENABLE_CRITICAL

/**
 * @brief Critical check - always enabled for safety
 * Use sparingly for truly essential validations
 */
#define EIF_CRITICAL_CHECK(cond, status)                                       \
  do {                                                                         \
    if (!(cond))                                                               \
      return (status);                                                         \
  } while (0)

/**
 * @brief Critical NULL check
 */
#define EIF_CRITICAL_PTR(ptr)                                                  \
  EIF_CRITICAL_CHECK((ptr) != NULL, EIF_STATUS_INVALID_ARGUMENT)

#else

#define EIF_CRITICAL_CHECK(cond, status) ((void)0)
#define EIF_CRITICAL_PTR(ptr) ((void)0)

#endif // EIF_ENABLE_CRITICAL

// =============================================================================
// Error Propagation
// =============================================================================

/**
 * @brief Try expression and propagate error
 *
 * Usage:
 *   EIF_TRY(eif_memory_alloc(...));
 *   EIF_TRY(eif_conv2d(...));
 */
#define EIF_TRY(expr)                                                          \
  do {                                                                         \
    eif_status_t _eif_status = (expr);                                         \
    if (_eif_status != EIF_STATUS_OK)                                          \
      return _eif_status;                                                      \
  } while (0)

/**
 * @brief Try with custom error handling
 */
#define EIF_TRY_OR(expr, cleanup)                                              \
  do {                                                                         \
    eif_status_t _eif_status = (expr);                                         \
    if (_eif_status != EIF_STATUS_OK) {                                        \
      cleanup;                                                                 \
      return _eif_status;                                                      \
    }                                                                          \
  } while (0)

// =============================================================================
// Compile-Time Assertions
// =============================================================================

/**
 * @brief Static assertion (compile-time check)
 */
#if __STDC_VERSION__ >= 201112L
#define EIF_STATIC_ASSERT(expr, msg) _Static_assert(expr, msg)
#else
#define EIF_STATIC_ASSERT(expr, msg)                                           \
  typedef char eif_static_assert_##__LINE__[(expr) ? 1 : -1]
#endif

/**
 * @brief Verify struct size at compile time
 */
#define EIF_ASSERT_SIZE(type, expected_size)                                   \
  EIF_STATIC_ASSERT(sizeof(type) == (expected_size),                           \
                    "Unexpected size for " #type)

/**
 * @brief Verify array has minimum size
 */
#define EIF_ASSERT_ARRAY_MIN(arr, min_elements)                                \
  EIF_STATIC_ASSERT(sizeof(arr) / sizeof((arr)[0]) >= (min_elements),          \
                    "Array " #arr " too small")

// =============================================================================
// Debug Helpers
// =============================================================================

#ifdef EIF_ENABLE_VALIDATION

/**
 * @brief Debug breakpoint (platform-specific)
 */
#if defined(__arm__) || defined(__ARM_ARCH)
#define EIF_BREAKPOINT() __asm__("BKPT #0")
#elif defined(__x86_64__) || defined(__i386__)
#define EIF_BREAKPOINT() __asm__("int $3")
#else
#define EIF_BREAKPOINT() ((void)0)
#endif

/**
 * @brief Assert with breakpoint
 */
#define EIF_ASSERT(cond)                                                       \
  do {                                                                         \
    if (!(cond)) {                                                             \
      EIF_BREAKPOINT();                                                        \
    }                                                                          \
  } while (0)

#else

#define EIF_BREAKPOINT() ((void)0)
#define EIF_ASSERT(cond) ((void)0)

#endif // EIF_ENABLE_VALIDATION

// =============================================================================
// Return Value Helpers
// =============================================================================

/**
 * @brief Mark function return value as must-use
 */
#if defined(__GNUC__) || defined(__clang__)
#define EIF_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define EIF_WARN_UNUSED_RESULT
#endif

/**
 * @brief Indicate function never returns
 */
#if defined(__GNUC__) || defined(__clang__)
#define EIF_NORETURN __attribute__((noreturn))
#else
#define EIF_NORETURN
#endif

#ifdef __cplusplus
}
#endif

#endif // EIF_ASSERT_H
