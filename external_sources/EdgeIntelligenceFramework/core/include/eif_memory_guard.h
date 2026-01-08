/**
 * @file eif_memory_guard.h
 * @brief EIF Memory Guard - Overflow Detection and Canary Protection
 *
 * Provides compile-time and runtime memory safety:
 * - Canary values for buffer overflow detection
 * - Stack guard for deep recursion protection
 * - Pool validation for memory corruption detection
 *
 * Usage:
 * @code
 * // Create guarded pool
 * EIF_GUARDED_POOL(my_pool, 4096);
 * eif_guard_pool_init(&my_pool);
 *
 * // Allocate with guard zone
 * float* data = eif_guard_alloc(&my_pool, 256 * sizeof(float));
 *
 * // Check for corruption
 * if (!eif_guard_pool_check(&my_pool)) {
 *     printf("Memory corruption detected!\n");
 * }
 * @endcode
 */

#ifndef EIF_MEMORY_GUARD_H
#define EIF_MEMORY_GUARD_H

#include "eif_memory.h"
#include "eif_status.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================

// Canary pattern (chosen to be unlikely in normal data)
#define EIF_CANARY_VALUE 0xDEADBEEF
#define EIF_CANARY_FILL 0xCD // Fill pattern for uninitialized memory
#define EIF_FREE_FILL 0xDD   // Fill pattern for freed memory

// Guard zone size (bytes before and after each allocation)
#ifndef EIF_GUARD_ZONE_SIZE
#define EIF_GUARD_ZONE_SIZE 8
#endif

// Maximum tracked allocations
#ifndef EIF_MAX_GUARD_ALLOCS
#define EIF_MAX_GUARD_ALLOCS 32
#endif

// =============================================================================
// Types
// =============================================================================

/**
 * @brief Guarded allocation record
 */
typedef struct {
  void *user_ptr;        /**< Pointer returned to user */
  size_t user_size;      /**< Size requested by user */
  uint32_t front_canary; /**< Canary before allocation */
  uint32_t back_canary;  /**< Canary after allocation */
  bool active;           /**< Is this slot in use */
} eif_guard_alloc_t;

/**
 * @brief Guarded memory pool
 */
typedef struct {
  eif_memory_pool_t pool;                         /**< Underlying pool */
  uint32_t header_canary;                         /**< Start canary */
  uint32_t footer_canary;                         /**< End canary */
  eif_guard_alloc_t allocs[EIF_MAX_GUARD_ALLOCS]; /**< Allocation tracking */
  int alloc_count;                                /**< Active allocations */
  int overflow_detected;                          /**< Overflow counter */
} eif_guard_pool_t;

// =============================================================================
// Macros
// =============================================================================

/**
 * @brief Declare a guarded pool with static buffer
 */
#define EIF_GUARDED_POOL(name, size)                                           \
  static uint8_t name##_buffer[size];                                          \
  static eif_guard_pool_t name

/**
 * @brief Compile-time size check
 */
#define EIF_STATIC_SIZE_CHECK(buffer, min_size)                                \
  _Static_assert(sizeof(buffer) >= (min_size), "Buffer " #buffer " too small")

// =============================================================================
// Pool API
// =============================================================================

/**
 * @brief Initialize guarded pool
 */
static inline eif_status_t eif_guard_pool_init(eif_guard_pool_t *gpool,
                                               void *buffer, size_t size) {
  if (!gpool || !buffer || size < 64) {
    return EIF_STATUS_INVALID_ARGUMENT;
  }

  // Initialize pool
  eif_status_t status = eif_memory_pool_init(&gpool->pool, buffer, size);
  if (status != EIF_STATUS_OK)
    return status;

  // Set canaries
  gpool->header_canary = EIF_CANARY_VALUE;
  gpool->footer_canary = EIF_CANARY_VALUE;

  // Clear allocation tracking
  memset(gpool->allocs, 0, sizeof(gpool->allocs));
  gpool->alloc_count = 0;
  gpool->overflow_detected = 0;

  // Fill buffer with pattern (helps detect uninitialized access)
#ifdef EIF_ENABLE_VALIDATION
  memset(buffer, EIF_CANARY_FILL, size);
#endif

  return EIF_STATUS_OK;
}

/**
 * @brief Initialize from declared pool macro
 */
#define eif_guard_pool_init_static(name)                                       \
  eif_guard_pool_init(&name, name##_buffer, sizeof(name##_buffer))

/**
 * @brief Check pool integrity
 */
static inline bool eif_guard_pool_check(const eif_guard_pool_t *gpool) {
  if (!gpool)
    return false;

  // Check pool canaries
  if (gpool->header_canary != EIF_CANARY_VALUE)
    return false;
  if (gpool->footer_canary != EIF_CANARY_VALUE)
    return false;

  // Check each allocation's canaries
  for (int i = 0; i < EIF_MAX_GUARD_ALLOCS; i++) {
    if (gpool->allocs[i].active) {
      if (gpool->allocs[i].front_canary != EIF_CANARY_VALUE)
        return false;
      if (gpool->allocs[i].back_canary != EIF_CANARY_VALUE)
        return false;

      // Check guard zones if validation enabled
#ifdef EIF_ENABLE_VALIDATION
      // Front guard zone check (before user pointer)
      uint8_t *front_guard =
          (uint8_t *)gpool->allocs[i].user_ptr - EIF_GUARD_ZONE_SIZE;
      for (int j = 0; j < EIF_GUARD_ZONE_SIZE; j++) {
        if (front_guard[j] != EIF_CANARY_FILL)
          return false;
      }

      // Back guard zone check (after user data)
      uint8_t *back_guard =
          (uint8_t *)gpool->allocs[i].user_ptr + gpool->allocs[i].user_size;
      for (int j = 0; j < EIF_GUARD_ZONE_SIZE; j++) {
        if (back_guard[j] != EIF_CANARY_FILL)
          return false;
      }
#endif
    }
  }

  return true;
}

// =============================================================================
// Allocation API
// =============================================================================

/**
 * @brief Allocate with guard zones
 */
static inline void *eif_guard_alloc(eif_guard_pool_t *gpool, size_t size) {
  if (!gpool || size == 0)
    return NULL;

  // Find free slot
  int slot = -1;
  for (int i = 0; i < EIF_MAX_GUARD_ALLOCS; i++) {
    if (!gpool->allocs[i].active) {
      slot = i;
      break;
    }
  }
  if (slot < 0)
    return NULL; // No free slots

  // Allocate with guard zones
  size_t total_size = size + (2 * EIF_GUARD_ZONE_SIZE);
  uint8_t *raw_ptr = (uint8_t *)eif_memory_alloc(&gpool->pool, total_size, 8);
  if (!raw_ptr)
    return NULL;

  // Fill guard zones
  memset(raw_ptr, EIF_CANARY_FILL, EIF_GUARD_ZONE_SIZE);
  memset(raw_ptr + EIF_GUARD_ZONE_SIZE + size, EIF_CANARY_FILL,
         EIF_GUARD_ZONE_SIZE);

  // User pointer is after front guard
  void *user_ptr = raw_ptr + EIF_GUARD_ZONE_SIZE;

  // Record allocation
  gpool->allocs[slot].user_ptr = user_ptr;
  gpool->allocs[slot].user_size = size;
  gpool->allocs[slot].front_canary = EIF_CANARY_VALUE;
  gpool->allocs[slot].back_canary = EIF_CANARY_VALUE;
  gpool->allocs[slot].active = true;
  gpool->alloc_count++;

  return user_ptr;
}

/**
 * @brief Allocate array with guard zones
 */
#define EIF_GUARD_ALLOC_ARRAY(gpool, T, count)                                 \
  ((T *)eif_guard_alloc(gpool, (count) * sizeof(T)))

// =============================================================================
// Stack Guard
// =============================================================================

/**
 * @brief Stack guard for detecting stack overflow
 * Place at function entry for deep recursion protection
 */
typedef struct {
  uint32_t canary;
  void *stack_base;
} eif_stack_guard_t;

#define EIF_STACK_GUARD_INIT(guard)                                            \
  do {                                                                         \
    volatile uint8_t _stack_var;                                               \
    (guard).canary = EIF_CANARY_VALUE;                                         \
    (guard).stack_base = (void *)&_stack_var;                                  \
  } while (0)

#define EIF_STACK_GUARD_CHECK(guard) ((guard).canary == EIF_CANARY_VALUE)

// =============================================================================
// Debug Helpers
// =============================================================================

#ifdef EIF_ENABLE_VALIDATION

/**
 * @brief Print pool status
 */
static inline void eif_guard_pool_print(const eif_guard_pool_t *gpool) {
  if (!gpool)
    return;

  printf("=== Guard Pool Status ===\n");
  printf("Header canary: %s\n",
         gpool->header_canary == EIF_CANARY_VALUE ? "OK" : "CORRUPTED");
  printf("Footer canary: %s\n",
         gpool->footer_canary == EIF_CANARY_VALUE ? "OK" : "CORRUPTED");
  printf("Active allocations: %d/%d\n", gpool->alloc_count,
         EIF_MAX_GUARD_ALLOCS);
  printf("Overflows detected: %d\n", gpool->overflow_detected);
  printf("Pool integrity: %s\n",
         eif_guard_pool_check(gpool) ? "OK" : "CORRUPTED");
}

#else
#define eif_guard_pool_print(gpool) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif // EIF_MEMORY_GUARD_H
