/**
 * @file eif_dl_context.c
 * @brief Thread-Safe Graph Execution Engine
 * 
 * Implementation of:
 * - Context initialization with memory planning
 * - Graph execution with operator dispatch
 * - Thread-safe invoke with optional mutex
 */

#include "eif_nn_context.h"
#include "eif_nn_layers.h"
#include "eif_dl_internal.h"
#include <string.h>
#include <float.h>

// ============================================================================
// Default Mutex Stubs (No RTOS)
// ============================================================================

#ifndef EIF_HAS_RTOS

static void eif_mutex_lock_stub(eif_mutex_t m) { (void)m; }
static void eif_mutex_unlock_stub(eif_mutex_t m) { (void)m; }

#define eif_mutex_lock  eif_mutex_lock_stub
#define eif_mutex_unlock eif_mutex_unlock_stub

#endif

// ============================================================================
// Forward Declarations: Built-in Operators
// ============================================================================

static eif_status_t op_relu(eif_dl_context_t* ctx, const eif_layer_node_t* node, void** in, void** out);
static eif_status_t op_dense(eif_dl_context_t* ctx, const eif_layer_node_t* node, void** in, void** out);
static eif_status_t op_conv2d(eif_dl_context_t* ctx, const eif_layer_node_t* node, void** in, void** out);
static eif_status_t op_add(eif_dl_context_t* ctx, const eif_layer_node_t* node, void** in, void** out);
static eif_status_t op_softmax(eif_dl_context_t* ctx, const eif_layer_node_t* node, void** in, void** out);

// ============================================================================
// Default Operator Registry
// ============================================================================

static eif_op_registry_t s_default_registry = {0};

static void init_default_registry(void) {
    static bool initialized = false;
    if (initialized) return;
    
    s_default_registry.funcs[EIF_LAYER_RELU] = op_relu;
    s_default_registry.funcs[EIF_LAYER_DENSE] = op_dense;
    s_default_registry.funcs[EIF_LAYER_CONV2D] = op_conv2d;
    s_default_registry.funcs[EIF_LAYER_ADD] = op_add;
    s_default_registry.funcs[EIF_LAYER_SOFTMAX] = op_softmax;
    // ... more operators registered here
    
    initialized = true;
}

// ============================================================================
// Memory Requirements Calculation
// ============================================================================

eif_status_t eif_dl_get_memory_requirements(
    const eif_model_t* model,
    eif_dl_memory_req_t* req
) {
    if (!model || !req) return EIF_STATUS_INVALID_ARGUMENT;
    
    memset(req, 0, sizeof(eif_dl_memory_req_t));
    
    // Activation arena size (pre-computed by model loader)
    req->activation_bytes = model->activation_arena_size;
    
    // Scratch buffer (use model hint or default)
    req->scratch_bytes = model->scratch_size > 0 ? model->scratch_size : 4096;
    
    // Persistent buffer for RNN states
    req->persistent_bytes = model->persistent_size;
    
    // Tensor pointer array
    req->tensor_ptr_bytes = model->num_tensors * sizeof(void*);
    
    req->total_bytes = req->activation_bytes + req->scratch_bytes + 
                       req->persistent_bytes + req->tensor_ptr_bytes;
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Context Initialization
// ============================================================================

eif_status_t eif_dl_context_init(
    eif_dl_context_t* ctx,
    const eif_model_t* model,
    eif_memory_pool_t* pool,
    const eif_op_registry_t* registry
) {
    if (!ctx || !model || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    memset(ctx, 0, sizeof(eif_dl_context_t));
    ctx->model = model;
    ctx->num_tensors = model->num_tensors;
    
    // Initialize default registry if not provided
    if (!registry) {
        init_default_registry();
        registry = &s_default_registry;
    }
    ctx->op_registry = registry;
    
    // ========================================================================
    // Step 1: Liveness Analysis for Activation Memory Planning
    // ========================================================================
    
    int* first_use = (int*)eif_memory_alloc(pool, model->num_tensors * sizeof(int), 4);
    int* last_use = (int*)eif_memory_alloc(pool, model->num_tensors * sizeof(int), 4);
    size_t* offsets = (size_t*)eif_memory_alloc(pool, model->num_tensors * sizeof(size_t), 4);
    
    if (!first_use || !last_use || !offsets) return EIF_STATUS_OUT_OF_MEMORY;
    
    for (int i = 0; i < model->num_tensors; i++) {
        first_use[i] = -1;
        last_use[i] = -1;
        offsets[i] = 0;
    }
    
    // Mark graph inputs as live from the start
    for (int i = 0; i < model->num_inputs; i++) {
        int idx = model->input_tensor_indices[i];
        first_use[idx] = 0;
        if (last_use[idx] < 0) last_use[idx] = 0;
    }
    
    // Mark graph outputs as live until the end
    for (int i = 0; i < model->num_outputs; i++) {
        int idx = model->output_tensor_indices[i];
        last_use[idx] = model->num_nodes;
    }
    
    // Compute liveness for each tensor based on node usage
    for (int i = 0; i < model->num_nodes; i++) {
        const eif_layer_node_t* node = &model->nodes[i];
        
        for (int j = 0; j < node->num_inputs; j++) {
            int idx = node->input_indices[j];
            if (last_use[idx] < i) last_use[idx] = i;
        }
        
        for (int j = 0; j < node->num_outputs; j++) {
            int idx = node->output_indices[j];
            if (first_use[idx] == -1) first_use[idx] = i;
        }
    }
    
    // ========================================================================
    // Step 2: Greedy Best-Fit Memory Allocation
    // ========================================================================
    
    #define MAX_FREE_BLOCKS 64
    struct { size_t offset; size_t size; } free_blocks[MAX_FREE_BLOCKS];
    int num_free = 0;
    size_t activation_arena_size = 0;
    
    for (int step = 0; step <= model->num_nodes; step++) {
        // Release tensors whose lifetime ended
        if (step > 0) {
            for (int t = 0; t < model->num_tensors; t++) {
                if (model->tensors[t].is_variable && last_use[t] == step - 1) {
                    if (num_free < MAX_FREE_BLOCKS) {
                        free_blocks[num_free].offset = offsets[t];
                        free_blocks[num_free].size = (model->tensors[t].size_bytes + 15) & ~15;
                        num_free++;
                    }
                }
            }
        }
        
        // Allocate tensors that start at this step
        for (int t = 0; t < model->num_tensors; t++) {
            if (model->tensors[t].is_variable && first_use[t] == step) {
                size_t size = (model->tensors[t].size_bytes + 15) & ~15;
                
                // Find best-fit block
                int best_idx = -1;
                size_t min_diff = (size_t)-1;
                
                for (int k = 0; k < num_free; k++) {
                    if (free_blocks[k].size >= size) {
                        size_t diff = free_blocks[k].size - size;
                        if (diff < min_diff) {
                            min_diff = diff;
                            best_idx = k;
                        }
                    }
                }
                
                if (best_idx != -1) {
                    offsets[t] = free_blocks[best_idx].offset;
                    free_blocks[best_idx].offset += size;
                    free_blocks[best_idx].size -= size;
                    if (free_blocks[best_idx].size == 0) {
                        free_blocks[best_idx] = free_blocks[num_free - 1];
                        num_free--;
                    }
                } else {
                    offsets[t] = activation_arena_size;
                    activation_arena_size += size;
                }
            }
        }
    }
    #undef MAX_FREE_BLOCKS
    
    // ========================================================================
    // Step 3: Allocate Arena Memory
    // ========================================================================
    
    ctx->arena.activation_size = activation_arena_size;
    if (activation_arena_size > 0) {
        ctx->arena.activation_base = (uint8_t*)eif_memory_alloc(pool, activation_arena_size, 16);
        if (!ctx->arena.activation_base) return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    ctx->arena.scratch_size = model->scratch_size > 0 ? model->scratch_size : 4096;
    ctx->arena.scratch_base = (uint8_t*)eif_memory_alloc(pool, ctx->arena.scratch_size, 16);
    if (!ctx->arena.scratch_base) return EIF_STATUS_OUT_OF_MEMORY;
    ctx->arena.scratch_used = 0;
    
    ctx->arena.persistent_size = model->persistent_size;
    if (model->persistent_size > 0) {
        ctx->arena.persistent_base = (uint8_t*)eif_memory_alloc(pool, model->persistent_size, 16);
        if (!ctx->arena.persistent_base) return EIF_STATUS_OUT_OF_MEMORY;
        memset(ctx->arena.persistent_base, 0, model->persistent_size);
    }
    
    // ========================================================================
    // Step 4: Resolve Tensor Data Pointers
    // ========================================================================
    
    ctx->tensor_data = (void**)eif_memory_alloc(pool, model->num_tensors * sizeof(void*), 4);
    if (!ctx->tensor_data) return EIF_STATUS_OUT_OF_MEMORY;
    
    for (int i = 0; i < model->num_tensors; i++) {
        if (model->tensors[i].is_variable) {
            ctx->tensor_data[i] = ctx->arena.activation_base + offsets[i];
        } else {
            ctx->tensor_data[i] = model->tensors[i].data;
        }
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Thread Safety
// ============================================================================

void eif_dl_context_set_mutex(eif_dl_context_t* ctx, eif_mutex_t mutex) {
    if (ctx) {
        ctx->mutex = mutex;
        ctx->mutex_enabled = (mutex != NULL);
    }
}

// ============================================================================
// Graph Execution Engine
// ============================================================================

eif_status_t eif_dl_invoke(eif_dl_context_t* ctx) {
    if (!ctx || !ctx->model) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Thread safety: acquire mutex if enabled
    if (ctx->mutex_enabled && ctx->mutex) {
        eif_mutex_lock(ctx->mutex);
    }
    
    const eif_model_t* model = ctx->model;
    eif_status_t status = EIF_STATUS_OK;
    
    // Reset persistent offset for RNN state tracking
    ctx->persistent_offset = 0;
    
    // ========================================================================
    // Execute Graph Nodes in Topological Order
    // ========================================================================
    
    for (int i = 0; i < model->num_nodes && status == EIF_STATUS_OK; i++) {
        const eif_layer_node_t* node = &model->nodes[i];
        
        // Reset scratch buffer before each layer
        eif_arena_scratch_reset(&ctx->arena);
        
        // Gather input pointers
        void* inputs[8];
        for (int j = 0; j < node->num_inputs && j < 8; j++) {
            inputs[j] = ctx->tensor_data[node->input_indices[j]];
        }
        
        // Gather output pointers
        void* outputs[4];
        for (int j = 0; j < node->num_outputs && j < 4; j++) {
            outputs[j] = ctx->tensor_data[node->output_indices[j]];
        }
        
        // Dispatch to operator
        if (ctx->op_registry && node->type < EIF_MAX_OP_TYPES) {
            eif_op_func_t op_func = ctx->op_registry->funcs[node->type];
            if (op_func) {
                status = op_func(ctx, node, inputs, outputs);
            } else {
                // Fallback to built-in dispatch
                status = eif_dl_execute_node(ctx, node, inputs, outputs);
            }
        } else {
            status = eif_dl_execute_node(ctx, node, inputs, outputs);
        }
        
        // Update statistics
        if (ctx->arena.scratch_used > ctx->stats.peak_scratch_bytes) {
            ctx->stats.peak_scratch_bytes = ctx->arena.scratch_used;
        }
    }
    
    ctx->stats.invoke_count++;
    
    // Thread safety: release mutex
    if (ctx->mutex_enabled && ctx->mutex) {
        eif_mutex_unlock(ctx->mutex);
    }
    
    return status;
}

// ============================================================================
// Built-in Node Execution (Fallback)
// ============================================================================

eif_status_t eif_dl_execute_node(
    eif_dl_context_t* ctx,
    const eif_layer_node_t* node,
    void** inputs,
    void** outputs
) {
    const eif_model_t* model = ctx->model;
    const eif_tensor_t* in_tensor = NULL;
    const eif_tensor_t* out_tensor = NULL;
    
    if (node->num_inputs > 0) {
        in_tensor = &model->tensors[node->input_indices[0]];
    }
    if (node->num_outputs > 0) {
        out_tensor = &model->tensors[node->output_indices[0]];
    }
    
    switch (node->type) {
        case EIF_LAYER_RELU: {
            int size = in_tensor->size_bytes / sizeof(float32_t);
            eif_layer_relu((float32_t*)inputs[0], (float32_t*)outputs[0], size);
            break;
        }
        
        case EIF_LAYER_DENSE: {
            eif_layer_t layer = {0};
            layer.type = node->type;
            if (node->params) layer.params = *(eif_layer_param_t*)node->params;
            if (node->num_inputs > 1) layer.weights = inputs[1];
            if (node->num_inputs > 2) layer.biases = inputs[2];
            
            int in_size = in_tensor->size_bytes / sizeof(float32_t);
            eif_layer_dense(&layer, (float32_t*)inputs[0], (float32_t*)outputs[0], in_size);
            break;
        }
        
        case EIF_LAYER_CONV2D: {
            if (node->num_inputs < 2) return EIF_STATUS_ERROR;
            
            eif_layer_t layer = {0};
            layer.type = node->type;
            if (node->params) layer.params = *(eif_layer_param_t*)node->params;
            layer.weights = inputs[1];
            if (node->num_inputs > 2) layer.biases = inputs[2];
            
            int in_h = in_tensor->dims[1];
            int in_w = in_tensor->dims[2];
            int in_c = in_tensor->dims[3];
            int out_h, out_w, out_c;
            
            eif_status_t status = eif_layer_conv2d(&layer, (float32_t*)inputs[0], (float32_t*)outputs[0], 
                            in_h, in_w, in_c, &out_h, &out_w, &out_c);
            if (status != EIF_STATUS_OK) return status;
            break;
        }
        
        case EIF_LAYER_ADD: {
            if (node->num_inputs < 2) return EIF_STATUS_ERROR;
            int size = in_tensor->size_bytes / sizeof(float32_t);
            float32_t* a = (float32_t*)inputs[0];
            float32_t* b = (float32_t*)inputs[1];
            float32_t* out = (float32_t*)outputs[0];
            for (int k = 0; k < size; k++) out[k] = a[k] + b[k];
            break;
        }
        
        case EIF_LAYER_SOFTMAX: {
            int size = in_tensor->size_bytes / sizeof(float32_t);
            eif_layer_softmax((float32_t*)inputs[0], (float32_t*)outputs[0], size);
            break;
        }
        
        case EIF_LAYER_SIGMOID: {
            int size = in_tensor->size_bytes / sizeof(float32_t);
            eif_layer_sigmoid((float32_t*)inputs[0], (float32_t*)outputs[0], size);
            break;
        }
        
        case EIF_LAYER_TANH: {
            int size = in_tensor->size_bytes / sizeof(float32_t);
            eif_layer_tanh((float32_t*)inputs[0], (float32_t*)outputs[0], size);
            break;
        }
        
        case EIF_LAYER_MAXPOOL2D:
        case EIF_LAYER_AVGPOOL2D: {
            eif_layer_t layer = {0};
            layer.type = node->type;
            if (node->params) layer.params = *(eif_layer_param_t*)node->params;
            
            int in_h = in_tensor->dims[1];
            int in_w = in_tensor->dims[2];
            int in_c = in_tensor->dims[3];
            int out_h, out_w;
            
            if (node->type == EIF_LAYER_MAXPOOL2D) {
                eif_layer_maxpool2d(&layer, (float32_t*)inputs[0], (float32_t*)outputs[0],
                                   in_h, in_w, in_c, &out_h, &out_w);
            } else {
                eif_layer_avgpool2d(&layer, (float32_t*)inputs[0], (float32_t*)outputs[0],
                                   in_h, in_w, in_c, &out_h, &out_w);
            }
            break;
        }
        
        case EIF_LAYER_FLATTEN:
        case EIF_LAYER_RESHAPE: {
            memcpy(outputs[0], inputs[0], in_tensor->size_bytes);
            break;
        }
        
        default:
            // Unsupported layer
            return EIF_STATUS_ERROR;
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Convenience Invoke with I/O Copy
// ============================================================================

eif_status_t eif_dl_invoke_io(
    eif_dl_context_t* ctx,
    const void* input,
    size_t input_size,
    void* output,
    size_t output_size
) {
    if (!ctx || !input || !output) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Copy input
    void* in_ptr = eif_dl_input_ptr(ctx, 0);
    if (!in_ptr) return EIF_STATUS_ERROR;
    size_t in_size = eif_dl_tensor_size(ctx, ctx->model->input_tensor_indices[0]);
    memcpy(in_ptr, input, input_size < in_size ? input_size : in_size);
    
    // Invoke
    eif_status_t status = eif_dl_invoke(ctx);
    if (status != EIF_STATUS_OK) return status;
    
    // Copy output
    const void* out_ptr = eif_dl_output_ptr(ctx, 0);
    if (!out_ptr) return EIF_STATUS_ERROR;
    size_t out_size = eif_dl_tensor_size(ctx, ctx->model->output_tensor_indices[0]);
    memcpy(output, out_ptr, output_size < out_size ? output_size : out_size);
    
    return EIF_STATUS_OK;
}

// ============================================================================
// State Management
// ============================================================================

eif_status_t eif_dl_reset_state(eif_dl_context_t* ctx) {
    if (!ctx) return EIF_STATUS_INVALID_ARGUMENT;
    if (ctx->arena.persistent_base && ctx->arena.persistent_size > 0) {
        memset(ctx->arena.persistent_base, 0, ctx->arena.persistent_size);
    }
    ctx->persistent_offset = 0;
    return EIF_STATUS_OK;
}

// ============================================================================
// Built-in Operator Implementations
// ============================================================================

static eif_status_t op_relu(eif_dl_context_t* ctx, const eif_layer_node_t* node, void** in, void** out) {
    const eif_tensor_t* t = &ctx->model->tensors[node->input_indices[0]];
    int size = t->size_bytes / sizeof(float32_t);
    eif_layer_relu((float32_t*)in[0], (float32_t*)out[0], size);
    return EIF_STATUS_OK;
}

static eif_status_t op_dense(eif_dl_context_t* ctx, const eif_layer_node_t* node, void** in, void** out) {
    const eif_tensor_t* t = &ctx->model->tensors[node->input_indices[0]];
    eif_layer_t layer = {0};
    layer.type = node->type;
    if (node->params) layer.params = *(eif_layer_param_t*)node->params;
    if (node->num_inputs > 1) layer.weights = in[1];
    if (node->num_inputs > 2) layer.biases = in[2];
    
    int in_size = t->size_bytes / sizeof(float32_t);
    eif_layer_dense(&layer, (float32_t*)in[0], (float32_t*)out[0], in_size);
    return EIF_STATUS_OK;
}

static eif_status_t op_conv2d(eif_dl_context_t* ctx, const eif_layer_node_t* node, void** in, void** out) {
    if (node->num_inputs < 2) return EIF_STATUS_ERROR;
    
    const eif_tensor_t* t = &ctx->model->tensors[node->input_indices[0]];
    eif_layer_t layer = {0};
    layer.type = node->type;
    if (node->params) layer.params = *(eif_layer_param_t*)node->params;
    layer.weights = in[1];
    if (node->num_inputs > 2) layer.biases = in[2];
    
    int in_h = t->dims[1], in_w = t->dims[2], in_c = t->dims[3];
    int out_h, out_w, out_c;
    
    return eif_layer_conv2d(&layer, (float32_t*)in[0], (float32_t*)out[0],
                    in_h, in_w, in_c, &out_h, &out_w, &out_c);
}

static eif_status_t op_add(eif_dl_context_t* ctx, const eif_layer_node_t* node, void** in, void** out) {
    if (node->num_inputs < 2) return EIF_STATUS_ERROR;
    
    const eif_tensor_t* t = &ctx->model->tensors[node->input_indices[0]];
    int size = t->size_bytes / sizeof(float32_t);
    
    float32_t* a = (float32_t*)in[0];
    float32_t* b = (float32_t*)in[1];
    float32_t* o = (float32_t*)out[0];
    
    for (int i = 0; i < size; i++) o[i] = a[i] + b[i];
    return EIF_STATUS_OK;
}

static eif_status_t op_softmax(eif_dl_context_t* ctx, const eif_layer_node_t* node, void** in, void** out) {
    const eif_tensor_t* t = &ctx->model->tensors[node->input_indices[0]];
    int size = t->size_bytes / sizeof(float32_t);
    eif_layer_softmax((float32_t*)in[0], (float32_t*)out[0], size);
    return EIF_STATUS_OK;
}
