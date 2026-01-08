/**
 * @file eif_nn_model.h
 * @brief Neural Network Model Structure
 * 
 * Model graph structure and metadata.
 */

#ifndef EIF_NN_MODEL_H
#define EIF_NN_MODEL_H

#include "eif_nn_types.h"
#include "eif_nn_layers.h"
#include "eif_memory.h"
#include "eif_status.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Graph Node
// ============================================================================

/**
 * @brief Graph node representing a single operation
 */
typedef struct {
    eif_layer_type_t type;
    int* input_indices;    ///< Indices into tensors array
    int num_inputs;
    int* output_indices;   ///< Indices into tensors array
    int num_outputs;
    void* params;          ///< Layer-specific parameters
} eif_layer_node_t;

// ============================================================================
// Model Structure
// ============================================================================

/**
 * @brief Neural network model (immutable, shareable)
 */
typedef struct {
    eif_layer_node_t* nodes;   ///< Topologically sorted operations
    int num_nodes;
    
    eif_tensor_t* tensors;     ///< All tensors
    int num_tensors;
    
    int* input_tensor_indices;
    int num_inputs;
    
    int* output_tensor_indices;
    int num_outputs;
    
    // Memory requirements
    size_t activation_arena_size;
    size_t scratch_size;
    size_t persistent_size;
} eif_model_t;

// ============================================================================
// Model Metadata (Legacy compatibility)
// ============================================================================

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint16_t num_layers;
    uint32_t state_size_bytes;
    const eif_layer_t* layers;
    size_t arena_size;
} eif_model_meta_t;

// ============================================================================
// Serialization
// ============================================================================

eif_status_t eif_model_deserialize(eif_model_t* model, const uint8_t* buffer, 
                                    size_t buffer_size, eif_memory_pool_t* pool);

#ifdef __cplusplus
}
#endif

#endif // EIF_NN_MODEL_H
