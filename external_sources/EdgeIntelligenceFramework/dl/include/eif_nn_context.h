/**
 * @file eif_nn_context.h
 * @brief Thread-Safe Neural Context with RTOS Support
 * 
 * Enhanced context management with:
 * - Explicit mutex for multi-threaded access
 * - Operator dispatch table
 * - Execution statistics
 */

#ifndef EIF_NN_CONTEXT_H
#define EIF_NN_CONTEXT_H

#include "eif_nn_model.h"
#include "eif_nn_arena.h"
#include "eif_status.h"
#include "eif_memory.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// RTOS Abstraction (User-Provided)
// ============================================================================

/**
 * @brief Mutex handle type (opaque, platform-specific)
 * 
 * User provides implementation via eif_mutex_* functions.
 */
typedef void* eif_mutex_t;

// User must implement these for their RTOS:
// void eif_mutex_lock(eif_mutex_t mutex);
// void eif_mutex_unlock(eif_mutex_t mutex);
// eif_mutex_t eif_mutex_create(void);

// ============================================================================
// Execution Statistics
// ============================================================================

typedef struct {
    uint32_t invoke_count;           ///< Total invocations
    uint32_t last_invoke_cycles;     ///< Cycles for last invoke
    uint32_t total_cycles;           ///< Total cycles
    uint32_t peak_scratch_bytes;     ///< Peak scratch usage
} eif_exec_stats_t;

// ============================================================================
// Operator Function Signature
// ============================================================================

struct eif_dl_context_t;  // Forward declaration

/**
 * @brief Operator execution function pointer
 */
typedef eif_status_t (*eif_op_func_t)(
    struct eif_dl_context_t* ctx,
    const eif_layer_node_t* node,
    void** inputs,
    void** outputs
);

// ============================================================================
// Operator Registry
// ============================================================================

#define EIF_MAX_OP_TYPES 64

typedef struct {
    eif_op_func_t funcs[EIF_MAX_OP_TYPES];
    uint8_t registered_count;
} eif_op_registry_t;

// ============================================================================
// Deep Learning Context (Thread-Safe)
// ============================================================================

/**
 * @brief DL Inference Context
 * 
 * Thread Safety:
 * - Model is immutable and can be shared across contexts
 * - Each thread should have its own context
 * - For shared context access, use the mutex
 * 
 * Memory:
 * - No internal allocations during inference
 * - All memory from provided arena
 */
typedef struct eif_dl_context_t {
    // Model reference (immutable, shareable)
    const eif_model_t* model;
    
    // Memory arena (per-context)
    eif_tensor_arena_t arena;
    
    // Runtime tensor pointers (indices → addresses)
    void** tensor_data;
    int num_tensors;
    
    // Operator dispatch table
    const eif_op_registry_t* op_registry;
    
    // Threading
    eif_mutex_t mutex;              ///< Optional mutex for shared context
    bool mutex_enabled;
    
    // Execution state
    size_t persistent_offset;       ///< Current offset in persistent buffer
    
    // Statistics
    eif_exec_stats_t stats;
    
    // User data (optional)
    void* user_data;
    
} eif_dl_context_t;

// ============================================================================
// Context API
// ============================================================================

/**
 * @brief Initialize DL context with model and memory
 * 
 * @param ctx Context to initialize (caller provides storage)
 * @param model Model to execute (immutable, can be shared)
 * @param pool Memory pool for arena allocation
 * @param registry Operator registry (can be shared, or NULL for defaults)
 * @return Status code
 */
eif_status_t eif_dl_context_init(
    eif_dl_context_t* ctx,
    const eif_model_t* model,
    eif_memory_pool_t* pool,
    const eif_op_registry_t* registry
);

/**
 * @brief Enable mutex for thread-safe shared context
 * 
 * @param ctx Context
 * @param mutex Platform mutex handle
 */
void eif_dl_context_set_mutex(eif_dl_context_t* ctx, eif_mutex_t mutex);

/**
 * @brief Get memory requirements before allocation
 */
typedef struct {
    size_t activation_bytes;
    size_t scratch_bytes;
    size_t persistent_bytes;
    size_t tensor_ptr_bytes;
    size_t total_bytes;
} eif_dl_memory_req_t;

eif_status_t eif_dl_get_memory_requirements(
    const eif_model_t* model,
    eif_dl_memory_req_t* req
);

// ============================================================================
// Invoke API
// ============================================================================

/**
 * @brief Execute graph inference
 * 
 * Thread Safety:
 * - Safe if each thread has own context
 * - For shared context, enable mutex via eif_dl_context_set_mutex()
 * 
 * Memory:
 * - No allocations during execution
 * - Uses only pre-allocated arena
 * 
 * @param ctx Inference context
 * @return Status code
 */
eif_status_t eif_dl_invoke(eif_dl_context_t* ctx);

/**
 * @brief Invoke with explicit input/output buffers
 * 
 * Convenience wrapper that copies data.
 */
eif_status_t eif_dl_invoke_io(
    eif_dl_context_t* ctx,
    const void* input,
    size_t input_size,
    void* output,
    size_t output_size
);

// ============================================================================
// Tensor Access (Zero-Copy)
// ============================================================================

/**
 * @brief Get pointer to input tensor buffer
 */
static inline void* eif_dl_input_ptr(eif_dl_context_t* ctx, int index) {
    if (!ctx || !ctx->model || index < 0 || index >= ctx->model->num_inputs)
        return NULL;
    return ctx->tensor_data[ctx->model->input_tensor_indices[index]];
}

/**
 * @brief Get pointer to output tensor buffer
 */
static inline const void* eif_dl_output_ptr(eif_dl_context_t* ctx, int index) {
    if (!ctx || !ctx->model || index < 0 || index >= ctx->model->num_outputs)
        return NULL;
    return ctx->tensor_data[ctx->model->output_tensor_indices[index]];
}

/**
 * @brief Get tensor size in bytes
 */
static inline size_t eif_dl_tensor_size(eif_dl_context_t* ctx, int tensor_idx) {
    if (!ctx || !ctx->model || tensor_idx < 0 || tensor_idx >= ctx->model->num_tensors)
        return 0;
    return ctx->model->tensors[tensor_idx].size_bytes;
}

// ============================================================================
// State Management
// ============================================================================

/**
 * @brief Reset RNN/LSTM hidden states
 */
eif_status_t eif_dl_reset_state(eif_dl_context_t* ctx);

/**
 * @brief Reset execution statistics
 */
static inline void eif_dl_reset_stats(eif_dl_context_t* ctx) {
    if (ctx) {
        ctx->stats.invoke_count = 0;
        ctx->stats.total_cycles = 0;
        ctx->stats.peak_scratch_bytes = 0;
    }
}

#ifdef __cplusplus
}
#endif

#endif // EIF_NN_CONTEXT_H
