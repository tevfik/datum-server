/**
 * @file eif_nn_graph.h
 * @brief Graph Structure and Static Memory Planner
 * 
 * Enhanced graph representation with:
 * - Operator node with explicit I/O offsets (not pointers)
 * - Static memory planning at compile/load time
 * - Topological sort verification
 */

#ifndef EIF_NN_GRAPH_H
#define EIF_NN_GRAPH_H

#include "eif_nn_types.h"
#include "eif_nn_layers.h"
#include "eif_status.h"
#include "eif_memory.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Graph Node (Operator)
// ============================================================================

#define EIF_MAX_NODE_INPUTS  8
#define EIF_MAX_NODE_OUTPUTS 4

/**
 * @brief Graph node representing a single operation
 * 
 * All tensor references are indices into the model's tensor array.
 * This allows:
 * - Compact serialization
 * - Arena-based memory resolution at runtime
 * - No pointers in the graph structure
 */
typedef struct {
    // Operator type
    eif_layer_type_t op_type;
    
    // Input tensor indices (into model->tensors)
    int16_t input_indices[EIF_MAX_NODE_INPUTS];
    uint8_t num_inputs;
    
    // Output tensor indices
    int16_t output_indices[EIF_MAX_NODE_OUTPUTS];
    uint8_t num_outputs;
    
    // Operator-specific parameters (offset into params buffer)
    uint32_t params_offset;
    uint16_t params_size;
    
    // Execution hints
    uint8_t  is_inplace;        ///< Output can reuse input buffer
    uint8_t  requires_scratch;  ///< Needs scratch memory
    uint32_t scratch_bytes;     ///< Scratch requirement
    
} eif_graph_node_t;

// ============================================================================
// Tensor Descriptor (Enhanced)
// ============================================================================

/**
 * @brief Tensor with arena memory planning info
 */
typedef struct {
    // Type and shape
    eif_tensor_type_t dtype;
    int32_t shape[4];
    uint8_t num_dims;
    
    // Memory location
    union {
        struct {
            uint32_t arena_offset;   ///< Offset in activation arena
        } variable;
        struct {
            const void* data_ptr;    ///< Direct pointer to constant data
        } constant;
    };
    
    // Metadata
    uint32_t size_bytes;
    bool is_constant;            ///< True = weight/bias, False = activation
    
    // Liveness (for memory planning)
    int16_t first_use_node;      ///< First node that uses this tensor
    int16_t last_use_node;       ///< Last node that uses this tensor
    
} eif_graph_tensor_t;

// ============================================================================
// Model Graph
// ============================================================================

/**
 * @brief Complete model graph (immutable after loading)
 * 
 * Design:
 * - Nodes are topologically sorted (ready to execute in order)
 * - Tensors have pre-computed arena offsets
 * - No runtime memory allocation needed
 */
typedef struct {
    // Graph structure
    eif_graph_node_t* nodes;
    uint16_t num_nodes;
    
    // Tensor metadata
    eif_graph_tensor_t* tensors;
    uint16_t num_tensors;
    
    // I/O indices
    int16_t* input_tensors;
    uint8_t num_inputs;
    
    int16_t* output_tensors;
    uint8_t num_outputs;
    
    // Parameters buffer (shared, read-only)
    const uint8_t* params_buffer;
    uint32_t params_size;
    
    // Memory requirements (pre-computed)
    struct {
        uint32_t activation_arena_size;
        uint32_t scratch_buffer_size;
        uint32_t persistent_buffer_size;  ///< RNN states
    } memory;
    
    // Version and metadata
    uint32_t format_version;
    uint32_t checksum;
    
} eif_graph_model_t;

// ============================================================================
// Static Memory Planner
// ============================================================================

/**
 * @brief Memory planning result for one tensor
 */
typedef struct {
    uint16_t tensor_index;
    uint32_t arena_offset;
    uint32_t size_bytes;
} eif_memory_alloc_t;

/**
 * @brief Plan memory layout for all variable tensors
 * 
 * Uses liveness analysis + greedy best-fit:
 * 1. Compute first_use and last_use for each tensor
 * 2. Allocate in topological order
 * 3. Reuse freed blocks when lifetime ends
 * 
 * @param graph Model graph with nodes and tensors
 * @param allocations Output: Array of allocations (size = num_tensors)
 * @param total_arena_size Output: Required arena size
 * @return Status code
 */
eif_status_t eif_graph_plan_memory(
    const eif_graph_model_t* graph,
    eif_memory_alloc_t* allocations,
    uint32_t* total_arena_size
);

/**
 * @brief Verify graph is topologically sorted
 */
eif_status_t eif_graph_verify_topology(const eif_graph_model_t* graph);

/**
 * @brief Compute scratch buffer requirements
 */
uint32_t eif_graph_compute_scratch_size(const eif_graph_model_t* graph);

// ============================================================================
// Graph Builder (Optional, for runtime model construction)
// ============================================================================

typedef struct eif_graph_builder_t eif_graph_builder_t;

/**
 * @brief Create graph builder
 */
eif_graph_builder_t* eif_graph_builder_create(eif_memory_pool_t* pool);

/**
 * @brief Add tensor to graph
 * @return Tensor index
 */
int eif_graph_builder_add_tensor(
    eif_graph_builder_t* builder,
    const int32_t* shape,
    int num_dims,
    eif_tensor_type_t dtype,
    bool is_constant,
    const void* data
);

/**
 * @brief Add operation node
 * @return Node index, or -1 on error
 */
int eif_graph_builder_add_node(
    eif_graph_builder_t* builder,
    eif_layer_type_t op_type,
    const int16_t* input_indices,
    int num_inputs,
    const int16_t* output_indices,
    int num_outputs,
    const void* params,
    size_t params_size
);

/**
 * @brief Set graph inputs
 */
void eif_graph_builder_set_inputs(
    eif_graph_builder_t* builder,
    const int16_t* tensor_indices,
    int count
);

/**
 * @brief Set graph outputs
 */
void eif_graph_builder_set_outputs(
    eif_graph_builder_t* builder,
    const int16_t* tensor_indices,
    int count
);

/**
 * @brief Finalize graph (topological sort, memory planning)
 */
eif_status_t eif_graph_builder_finalize(
    eif_graph_builder_t* builder,
    eif_graph_model_t* model
);

#ifdef __cplusplus
}
#endif

#endif // EIF_NN_GRAPH_H
