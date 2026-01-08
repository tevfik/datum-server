/**
 * @file eif_memory.c
 * @brief EIF Memory Management Implementation
 */

#include "eif_memory.h"
#include <string.h>

// =============================================================================
// Core Pool API
// =============================================================================

eif_status_t eif_memory_pool_init(eif_memory_pool_t* pool, void* buffer, size_t size) {
    if (pool == NULL || buffer == NULL || size == 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    pool->base_addr = (uint8_t*)buffer;
    pool->size = size;
    pool->used = 0;
    pool->peak = 0;
    pool->alloc_count = 0;
    return EIF_STATUS_OK;
}

void* eif_memory_alloc(eif_memory_pool_t* pool, size_t size, size_t alignment) {
    if (pool == NULL || size == 0) {
        return NULL;
    }

    // Ensure alignment is at least 1 and power of 2
    if (alignment == 0) alignment = 1;

    uintptr_t current_addr = (uintptr_t)(pool->base_addr + pool->used);
    uintptr_t aligned_addr = (current_addr + (alignment - 1)) & ~(alignment - 1);
    size_t padding = aligned_addr - current_addr;

    if (pool->used + padding + size > pool->size) {
        return NULL; // Out of memory
    }

    pool->used += padding + size;
    pool->alloc_count++;
    
    // Track peak usage
    if (pool->used > pool->peak) {
        pool->peak = pool->used;
    }
    
    return (void*)aligned_addr;
}

void* eif_memory_calloc(eif_memory_pool_t* pool, size_t count, size_t size, size_t alignment) {
    size_t total_size = count * size;
    void* ptr = eif_memory_alloc(pool, total_size, alignment);
    if (ptr != NULL) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

void eif_memory_reset(eif_memory_pool_t* pool) {
    if (pool != NULL) {
        pool->used = 0;
        pool->alloc_count = 0;
        // Don't reset peak - keep it for analysis
    }
}

size_t eif_memory_available(eif_memory_pool_t* pool) {
    if (pool == NULL) {
        return 0;
    }
    return pool->size - pool->used;
}

// =============================================================================
// Scratch Buffer API
// =============================================================================

eif_memory_mark_t eif_memory_mark(eif_memory_pool_t* pool) {
    eif_memory_mark_t mark = {0};
    if (pool != NULL) {
        mark.used_at_mark = pool->used;
    }
    return mark;
}

void eif_memory_restore(eif_memory_pool_t* pool, eif_memory_mark_t mark) {
    if (pool != NULL && mark.used_at_mark <= pool->used) {
        pool->used = mark.used_at_mark;
        // Alloc count is not restored - still tracks total allocations
    }
}

// =============================================================================
// Statistics API
// =============================================================================

void eif_memory_get_stats(const eif_memory_pool_t* pool, eif_memory_stats_t* stats) {
    if (pool == NULL || stats == NULL) {
        return;
    }
    
    stats->total_size = pool->size;
    stats->used = pool->used;
    stats->available = pool->size - pool->used;
    stats->peak = pool->peak;
    stats->alloc_count = pool->alloc_count;
    stats->utilization = (pool->size > 0) ? (float)pool->used / pool->size : 0.0f;
}

size_t eif_memory_peak(const eif_memory_pool_t* pool) {
    if (pool == NULL) {
        return 0;
    }
    return pool->peak;
}

void eif_memory_reset_peak(eif_memory_pool_t* pool) {
    if (pool != NULL) {
        pool->peak = pool->used;
    }
}
