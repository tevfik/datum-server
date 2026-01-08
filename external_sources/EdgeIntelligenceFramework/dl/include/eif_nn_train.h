/**
 * @file eif_nn_train.h
 * @brief Neural Network Training Support
 * 
 * Backpropagation, optimizers, and weight updates.
 */

#ifndef EIF_NN_TRAIN_H
#define EIF_NN_TRAIN_H

#include "eif_nn_inference.h"
#include "eif_status.h"
#include "eif_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Optimizer Types
// ============================================================================

typedef enum {
    EIF_OPTIMIZER_SGD = 0,
    EIF_OPTIMIZER_ADAM,
    EIF_OPTIMIZER_RMSPROP
} eif_optimizer_type_t;

typedef struct {
    eif_optimizer_type_t type;
    float32_t learning_rate;
    float32_t momentum;      ///< For SGD with momentum
    float32_t weight_decay;  ///< L2 regularization
    float32_t beta1;         ///< Adam: first moment decay
    float32_t beta2;         ///< Adam: second moment decay
    float32_t epsilon;       ///< Adam: numerical stability
} eif_optimizer_t;

// ============================================================================
// Training Context
// ============================================================================

/**
 * @brief Training context (extends inference context)
 */
typedef struct {
    eif_neural_context_t* ctx;
    void** tensor_gradients;  ///< Gradients for each tensor
    eif_optimizer_t optimizer;
    
    // Optimizer state (for momentum/Adam)
    void** moment1;           ///< First moment estimates
    void** moment2;           ///< Second moment estimates (Adam)
    int step;                 ///< Training step counter
} eif_neural_train_ctx_t;

// ============================================================================
// Training API
// ============================================================================

/**
 * @brief Initialize training context
 */
eif_status_t eif_neural_train_init(eif_neural_train_ctx_t* train_ctx, 
                                    eif_neural_context_t* ctx, 
                                    const eif_optimizer_t* opt, 
                                    eif_memory_pool_t* pool);

/**
 * @brief Compute gradients (backward pass)
 * 
 * @param train_ctx Training context
 * @param target Target output for loss computation
 * @return Status code
 */
eif_status_t eif_neural_backward(eif_neural_train_ctx_t* train_ctx, 
                                  const float32_t* target);

/**
 * @brief Update weights using optimizer
 */
eif_status_t eif_neural_update(eif_neural_train_ctx_t* train_ctx);

/**
 * @brief Zero all gradients
 */
eif_status_t eif_neural_zero_grad(eif_neural_train_ctx_t* train_ctx);

#ifdef __cplusplus
}
#endif

#endif // EIF_NN_TRAIN_H
