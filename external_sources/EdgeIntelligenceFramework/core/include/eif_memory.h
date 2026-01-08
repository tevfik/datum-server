#ifndef EIF_MEMORY_H
#define EIF_MEMORY_H

/**
 * @file eif_memory.h
 * @brief EIF Memory Management
 * 
 * Provides:
 * - Static memory pool allocation (no heap)
 * - Aligned allocations
 * - Scratch buffer support for temporary allocations
 * - Memory usage tracking and statistics
 * 
 * Typical usage:
 * @code
 * static uint8_t buffer[4096];
 * eif_memory_pool_t pool;
 * eif_memory_pool_init(&pool, buffer, sizeof(buffer));
 * 
 * float* data = eif_memory_alloc(&pool, 100 * sizeof(float), sizeof(float));
 * 
 * // Use scratch for temporary allocations
 * eif_memory_mark_t mark = eif_memory_mark(&pool);
 * float* temp = eif_memory_alloc(&pool, 50 * sizeof(float), sizeof(float));
 * // ... use temp ...
 * eif_memory_restore(&pool, mark);  // Free temp
 * @endcode
 */

#include "eif_types.h"
#include "eif_status.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Types
// =============================================================================

/**
 * @brief Memory pool structure
 * 
 * Memory Layout:
 * [base_addr] -> [used bytes] -> [available bytes] -> [end]
 */
typedef struct {
    uint8_t* base_addr;     /**< Base address of pool */
    size_t size;            /**< Total size in bytes */
    size_t used;            /**< Currently used bytes */
    size_t peak;            /**< Peak usage (watermark) */
    uint32_t alloc_count;   /**< Number of allocations */
} eif_memory_pool_t;

/**
 * @brief Memory mark for scratch buffer support
 */
typedef struct {
    size_t used_at_mark;    /**< Bytes used when marked */
} eif_memory_mark_t;

/**
 * @brief Memory statistics
 */
typedef struct {
    size_t total_size;      /**< Total pool size */
    size_t used;            /**< Current usage */
    size_t available;       /**< Available bytes */
    size_t peak;            /**< Peak usage */
    uint32_t alloc_count;   /**< Allocation count */
    float utilization;      /**< Usage percentage (0.0-1.0) */
} eif_memory_stats_t;

// =============================================================================
// Core Pool API
// =============================================================================

/**
 * @brief Initialize a memory pool
 * @param pool Pool to initialize
 * @param buffer Static buffer to use
 * @param size Size of buffer in bytes
 * @return EIF_STATUS_OK on success
 */
eif_status_t eif_memory_pool_init(eif_memory_pool_t* pool, void* buffer, size_t size);

/**
 * @brief Allocate memory from the pool
 * @param pool Pool to allocate from
 * @param size Size in bytes to allocate
 * @param alignment Memory alignment (must be power of 2)
 * @return Pointer to allocated memory, or NULL if failed
 */
void* eif_memory_alloc(eif_memory_pool_t* pool, size_t size, size_t alignment);

/**
 * @brief Allocate and zero-initialize memory
 * @param pool Pool to allocate from
 * @param count Number of elements
 * @param size Size of each element
 * @param alignment Memory alignment
 * @return Pointer to zeroed memory, or NULL if failed
 */
void* eif_memory_calloc(eif_memory_pool_t* pool, size_t count, size_t size, size_t alignment);

/**
 * @brief Reset the pool (free all allocations)
 * @param pool Pool to reset
 */
void eif_memory_reset(eif_memory_pool_t* pool);

/**
 * @brief Get remaining available space
 * @param pool Pool to query
 * @return Available bytes
 */
size_t eif_memory_available(eif_memory_pool_t* pool);

// =============================================================================
// Scratch Buffer API (for temporary allocations)
// =============================================================================

/**
 * @brief Mark current pool position for later restore
 * @param pool Pool to mark
 * @return Mark that can be restored later
 */
eif_memory_mark_t eif_memory_mark(eif_memory_pool_t* pool);

/**
 * @brief Restore pool to a previously marked position
 * @param pool Pool to restore
 * @param mark Mark from eif_memory_mark()
 * 
 * All allocations made after the mark are freed.
 */
void eif_memory_restore(eif_memory_pool_t* pool, eif_memory_mark_t mark);

// =============================================================================
// Statistics API
// =============================================================================

/**
 * @brief Get memory pool statistics
 * @param pool Pool to query
 * @param stats Output statistics
 */
void eif_memory_get_stats(const eif_memory_pool_t* pool, eif_memory_stats_t* stats);

/**
 * @brief Get peak memory usage (watermark)
 * @param pool Pool to query
 * @return Peak usage in bytes
 */
size_t eif_memory_peak(const eif_memory_pool_t* pool);

/**
 * @brief Reset peak usage counter
 * @param pool Pool to reset
 */
void eif_memory_reset_peak(eif_memory_pool_t* pool);

// =============================================================================
// Utility Macros
// =============================================================================

/**
 * @brief Allocate array of type T with proper alignment
 */
#define EIF_ALLOC_ARRAY(pool, T, count) \
    ((T*)eif_memory_alloc(pool, (count) * sizeof(T), _Alignof(T)))

/**
 * @brief Allocate single instance of type T
 */
#define EIF_ALLOC(pool, T) \
    ((T*)eif_memory_alloc(pool, sizeof(T), _Alignof(T)))

/**
 * @brief Begin scratch scope
 */
#define EIF_SCRATCH_BEGIN(pool, mark) \
    eif_memory_mark_t mark = eif_memory_mark(pool)

/**
 * @brief End scratch scope (free temporary allocations)
 */
#define EIF_SCRATCH_END(pool, mark) \
    eif_memory_restore(pool, mark)

#ifdef __cplusplus
}
#endif

#endif // EIF_MEMORY_H
