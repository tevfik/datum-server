/**
 * @file eif_nn_inference.h
 * @brief Neural Network Inference Context and API
 * 
 * Context management and inference execution.
 */

#ifndef EIF_NN_INFERENCE_H
#define EIF_NN_INFERENCE_H

#include "eif_nn_model.h"
#include "eif_nn_arena.h"
#include "eif_status.h"
#include "eif_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Inference Context
// ============================================================================

// Forward declaration
struct eif_neural_context_t;

/**
 * @brief Custom Operator Function Pointer
 */
typedef eif_status_t (*eif_custom_op_func_t)(
    struct eif_neural_context_t* ctx,
    const eif_layer_node_t* node,
    void** inputs,
    void** outputs
);

#define EIF_MAX_CUSTOM_OPS 16

typedef struct {
    uint32_t op_ids[EIF_MAX_CUSTOM_OPS];
    eif_custom_op_func_t funcs[EIF_MAX_CUSTOM_OPS];
    int count;
} eif_custom_op_registry_t;

/**
 * @brief Inference context (thread-safe, per-instance)
 * 
 * Model can be shared across contexts. Each context has its own arena.
 */
typedef struct eif_neural_context_t {
    const eif_model_t* model;
    eif_tensor_arena_t arena;
    void** tensor_data;  ///< Runtime pointers to tensor data
    eif_custom_op_registry_t custom_ops;
} eif_neural_context_t;

// ============================================================================
// Inference API
// ============================================================================

/**
 * @brief Initialize inference context
 * 
 * @param ctx Context to initialize
 * @param model Model to use (can be shared)
 * @param pool Memory pool for allocations
 * @return Status code
 */
eif_status_t eif_neural_init(eif_neural_context_t* ctx, 
                              const eif_model_t* model, 
                              eif_memory_pool_t* pool);

/**
 * @brief Run inference (forward pass)
 */
eif_status_t eif_neural_invoke(eif_neural_context_t* ctx);

/**
 * @brief Set input tensor data
 */
eif_status_t eif_neural_set_input(eif_neural_context_t* ctx, int index, 
                                   const void* data, size_t size);

/**
 * @brief Get output tensor data
 */
eif_status_t eif_neural_get_output(eif_neural_context_t* ctx, int index, 
                                    void* data, size_t size);

/**
 * @brief Get pointer to input tensor (zero-copy)
 */
void* eif_neural_get_input_ptr(eif_neural_context_t* ctx, int index);

/**
 * @brief Get pointer to output tensor (zero-copy)
 */
const void* eif_neural_get_output_ptr(eif_neural_context_t* ctx, int index);

/**
 * @brief Reset persistent state (RNN/LSTM hidden states)
 */
eif_status_t eif_neural_reset_state(eif_neural_context_t* ctx);

/**
 * @brief Register a custom operator
 * 
 * @param ctx Context to register with
 * @param op_id Unique ID for the custom operator (must match model)
 * @param func Function pointer to implementation
 * @return EIF_STATUS_OK or error
 */
eif_status_t eif_neural_register_custom_op(
    eif_neural_context_t* ctx,
    uint32_t op_id,
    eif_custom_op_func_t func
);

#ifdef __cplusplus
}
#endif

#endif // EIF_NN_INFERENCE_H
