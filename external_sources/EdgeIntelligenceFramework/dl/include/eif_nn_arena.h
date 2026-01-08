/**
 * @file eif_nn_arena.h
 * @brief Tensor Arena Memory Management
 * 
 * Memory arena for activation tensors, scratch space, and persistent state.
 */

#ifndef EIF_NN_ARENA_H
#define EIF_NN_ARENA_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Tensor Arena
// ============================================================================

/**
 * @brief Tensor Arena - manages inference memory
 * 
 * Regions:
 * 1. Activation Buffer: Intermediate tensors (reused via liveness)
 * 2. Scratch Buffer: Temporary workspace (reset per layer)
 * 3. Persistent Buffer: RNN/LSTM states (across invocations)
 */
typedef struct {
    uint8_t* activation_base;
    size_t activation_size;
    
    uint8_t* scratch_base;
    size_t scratch_size;
    size_t scratch_used;
    
    uint8_t* persistent_base;
    size_t persistent_size;
} eif_tensor_arena_t;

// ============================================================================
// Arena Operations
// ============================================================================

/**
 * @brief Allocate from scratch buffer
 * 
 * @param arena Arena instance
 * @param size Bytes to allocate
 * @param alignment Alignment (power of 2)
 * @return Pointer to memory, or NULL if insufficient
 */
static inline void* eif_arena_scratch_alloc(eif_tensor_arena_t* arena, 
                                             size_t size, size_t alignment) {
    size_t offset = (arena->scratch_used + alignment - 1) & ~(alignment - 1);
    if (offset + size > arena->scratch_size) return NULL;
    void* ptr = arena->scratch_base + offset;
    arena->scratch_used = offset + size;
    return ptr;
}

/**
 * @brief Reset scratch buffer (called between layers)
 */
static inline void eif_arena_scratch_reset(eif_tensor_arena_t* arena) {
    arena->scratch_used = 0;
}

/**
 * @brief Get pointer to tensor data in arena
 */
static inline void* eif_arena_get_tensor(eif_tensor_arena_t* arena, size_t offset) {
    return arena->activation_base + offset;
}

#ifdef __cplusplus
}
#endif

#endif // EIF_NN_ARENA_H
