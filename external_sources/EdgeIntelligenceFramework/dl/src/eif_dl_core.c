#include "eif_neural.h"
#include "eif_dl_internal.h"
#include <string.h>
#include <float.h>

// ============================================================================
// Initialization
// ============================================================================

eif_status_t eif_neural_init(eif_neural_context_t* ctx, const eif_model_t* model, eif_memory_pool_t* pool) {
    if (!ctx || !model || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    memset(ctx, 0, sizeof(eif_neural_context_t));
    ctx->model = model;
    
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
        
        // Inputs are used at node i
        for (int j = 0; j < node->num_inputs; j++) {
            int idx = node->input_indices[j];
            if (last_use[idx] < i) last_use[idx] = i;
        }
        
        // Outputs are produced at node i
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
        // Release tensors whose lifetime ended at (step - 1)
        if (step > 0) {
            for (int t = 0; t < model->num_tensors; t++) {
                if (model->tensors[t].is_variable && last_use[t] == step - 1) {
                    // Add to free list
                    if (num_free < MAX_FREE_BLOCKS) {
                        free_blocks[num_free].offset = offsets[t];
                        free_blocks[num_free].size = (model->tensors[t].size_bytes + 15) & ~15; // 16-byte aligned
                        num_free++;
                    }
                }
            }
        }
        
        // Allocate tensors that start at this step
        for (int t = 0; t < model->num_tensors; t++) {
            if (model->tensors[t].is_variable && first_use[t] == step) {
                size_t size = (model->tensors[t].size_bytes + 15) & ~15; // 16-byte aligned
                
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
                    // Reuse existing block
                    offsets[t] = free_blocks[best_idx].offset;
                    free_blocks[best_idx].offset += size;
                    free_blocks[best_idx].size -= size;
                    if (free_blocks[best_idx].size == 0) {
                        free_blocks[best_idx] = free_blocks[num_free - 1];
                        num_free--;
                    }
                } else {
                    // Allocate new space
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
    
    // Activation buffer
    ctx->arena.activation_size = activation_arena_size;
    if (activation_arena_size > 0) {
        ctx->arena.activation_base = (uint8_t*)eif_memory_alloc(pool, activation_arena_size, 16);
        if (!ctx->arena.activation_base) return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    // Scratch buffer (use model hint or default)
    ctx->arena.scratch_size = model->scratch_size > 0 ? model->scratch_size : 4096;
    ctx->arena.scratch_base = (uint8_t*)eif_memory_alloc(pool, ctx->arena.scratch_size, 16);
    if (!ctx->arena.scratch_base) return EIF_STATUS_OUT_OF_MEMORY;
    ctx->arena.scratch_used = 0;
    
    // Persistent buffer for RNN states
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
// Input/Output Accessors
// ============================================================================

eif_status_t eif_neural_set_input(eif_neural_context_t* ctx, int index, const void* data, size_t size) {
    if (!ctx || !ctx->model || !data) return EIF_STATUS_INVALID_ARGUMENT;
    if (index < 0 || index >= ctx->model->num_inputs) return EIF_STATUS_INVALID_ARGUMENT;
    
    int tensor_idx = ctx->model->input_tensor_indices[index];
    if (tensor_idx < 0 || tensor_idx >= ctx->model->num_tensors) return EIF_STATUS_ERROR;
    
    void* tensor_ptr = ctx->tensor_data[tensor_idx];
    size_t tensor_size = ctx->model->tensors[tensor_idx].size_bytes;
    
    if (size > tensor_size) return EIF_STATUS_INVALID_ARGUMENT;
    
    memcpy(tensor_ptr, data, size);
    return EIF_STATUS_OK;
}

eif_status_t eif_neural_get_output(eif_neural_context_t* ctx, int index, void* data, size_t size) {
    if (!ctx || !ctx->model || !data) return EIF_STATUS_INVALID_ARGUMENT;
    if (index < 0 || index >= ctx->model->num_outputs) return EIF_STATUS_INVALID_ARGUMENT;
    
    int tensor_idx = ctx->model->output_tensor_indices[index];
    if (tensor_idx < 0 || tensor_idx >= ctx->model->num_tensors) return EIF_STATUS_ERROR;
    
    void* tensor_ptr = ctx->tensor_data[tensor_idx];
    size_t tensor_size = ctx->model->tensors[tensor_idx].size_bytes;
    
    if (size > tensor_size) size = tensor_size;
    
    memcpy(data, tensor_ptr, size);
    return EIF_STATUS_OK;
}

void* eif_neural_get_input_ptr(eif_neural_context_t* ctx, int index) {
    if (!ctx || !ctx->model) return NULL;
    if (index < 0 || index >= ctx->model->num_inputs) return NULL;
    return ctx->tensor_data[ctx->model->input_tensor_indices[index]];
}

const void* eif_neural_get_output_ptr(eif_neural_context_t* ctx, int index) {
    if (!ctx || !ctx->model) return NULL;
    if (index < 0 || index >= ctx->model->num_outputs) return NULL;
    return ctx->tensor_data[ctx->model->output_tensor_indices[index]];
}

eif_status_t eif_neural_reset_state(eif_neural_context_t* ctx) {
    if (!ctx) return EIF_STATUS_INVALID_ARGUMENT;
    if (ctx->arena.persistent_base && ctx->arena.persistent_size > 0) {
        memset(ctx->arena.persistent_base, 0, ctx->arena.persistent_size);
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_neural_register_custom_op(
    eif_neural_context_t* ctx,
    uint32_t op_id,
    eif_custom_op_func_t func
) {
    if (!ctx || !func) return EIF_STATUS_INVALID_ARGUMENT;
    if (ctx->custom_ops.count >= EIF_MAX_CUSTOM_OPS) return EIF_STATUS_OUT_OF_MEMORY;
    
    // Check for duplicate
    for (int i = 0; i < ctx->custom_ops.count; i++) {
        if (ctx->custom_ops.op_ids[i] == op_id) {
            ctx->custom_ops.funcs[i] = func; // Overwrite
            return EIF_STATUS_OK;
        }
    }
    
    int idx = ctx->custom_ops.count++;
    ctx->custom_ops.op_ids[idx] = op_id;
    ctx->custom_ops.funcs[idx] = func;
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Graph Execution Engine
// ============================================================================

eif_status_t eif_neural_invoke(eif_neural_context_t* ctx) {
    if (!ctx || !ctx->model) return EIF_STATUS_INVALID_ARGUMENT;
    
    const eif_model_t* model = ctx->model;
    
    // Track persistent state offset for RNN layers
    size_t persistent_offset = 0;
    
    for (int i = 0; i < model->num_nodes; i++) {
        eif_layer_node_t* node = &model->nodes[i];
        
        // Reset scratch buffer before each layer
        eif_arena_scratch_reset(&ctx->arena);
        
        // Resolve primary input/output tensors
        void* input = NULL;
        void* output = NULL;
        eif_tensor_t* in_tensor = NULL;
        eif_tensor_t* out_tensor = NULL;
        
        if (node->num_inputs > 0) {
            int in_idx = node->input_indices[0];
            in_tensor = &model->tensors[in_idx];
            input = ctx->tensor_data[in_idx];
        }
        
        if (node->num_outputs > 0) {
            int out_idx = node->output_indices[0];
            out_tensor = &model->tensors[out_idx];
            output = ctx->tensor_data[out_idx];
        }
        
        // Dispatch to layer implementation
        switch (node->type) {
            case EIF_LAYER_ARGMAX: {
                eif_tensor_shape_t in_shape;
                for(int k=0; k<4; k++) in_shape.dim[k] = in_tensor->dims[k];
                eif_layer_param_t* params = NULL;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    params = (eif_layer_param_t*)(qp + 1);
                }
                eif_status_t status = eif_layer_argmax((float32_t*)input, (float32_t*)output, &in_shape, params);
                if (status != EIF_STATUS_OK) return status;
                break;
            }

            case EIF_LAYER_MINIMUM: {
                if (node->num_inputs < 2) return EIF_STATUS_ERROR;
                void* input2 = ctx->tensor_data[node->input_indices[1]];
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_minimum((float32_t*)input, (float32_t*)input2, (float32_t*)output, size);
                break;
            }

            case EIF_LAYER_MAXIMUM: {
                if (node->num_inputs < 2) return EIF_STATUS_ERROR;
                void* input2 = ctx->tensor_data[node->input_indices[1]];
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_maximum((float32_t*)input, (float32_t*)input2, (float32_t*)output, size);
                break;
            }

            case EIF_LAYER_CUSTOM: {
                // params points to uint32_t op_id followed by custom data
                if (!node->params) return EIF_STATUS_INVALID_ARGUMENT;
                uint32_t op_id = *(uint32_t*)node->params;
                
                eif_custom_op_func_t func = NULL;
                for (int k = 0; k < ctx->custom_ops.count; k++) {
                    if (ctx->custom_ops.op_ids[k] == op_id) {
                        func = ctx->custom_ops.funcs[k];
                        break;
                    }
                }
                
                if (func) {
                    // Collect all inputs and outputs
                    void* inputs[16]; // Max inputs
                    void* outputs[16]; // Max outputs
                    
                    for (int k = 0; k < node->num_inputs && k < 16; k++) {
                        inputs[k] = ctx->tensor_data[node->input_indices[k]];
                    }
                    for (int k = 0; k < node->num_outputs && k < 16; k++) {
                        outputs[k] = ctx->tensor_data[node->output_indices[k]];
                    }
                    
                    eif_status_t status = func(ctx, node, inputs, outputs);
                    if (status != EIF_STATUS_OK) return status;
                } else {
                    return EIF_STATUS_NOT_SUPPORTED;
                }
                break;
            }

            case EIF_LAYER_RELU: {
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_relu((float32_t*)input, (float32_t*)output, size);
                break;
            }

            case EIF_LAYER_RELU6: {
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_relu6((float32_t*)input, (float32_t*)output, size);
                break;
            }
            
            case EIF_LAYER_DENSE: {
                eif_layer_t layer_wrapper = {0};
                layer_wrapper.type = node->type;
                if (node->params) {
                    // Read Quant Params first
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    layer_wrapper.quant_params = *qp;
                    // Layer Params follow
                    void* lp = (void*)(qp + 1);
                    layer_wrapper.params = *(eif_layer_param_t*)lp;
                }
                
                if (node->num_inputs > 1) {
                    layer_wrapper.weights = ctx->tensor_data[node->input_indices[1]];
                }
                if (node->num_inputs > 2) {
                    layer_wrapper.biases = ctx->tensor_data[node->input_indices[2]];
                }
                
                // Check for Int8
                if (in_tensor->type == EIF_TENSOR_INT8) {
                    int in_size = in_tensor->size_bytes; // 1 byte per element
                    eif_layer_dense_int8(&layer_wrapper, (int8_t*)input, (int8_t*)output, in_size);
                } else {
                    int in_size = in_tensor->size_bytes / sizeof(float32_t);
                    eif_layer_dense(&layer_wrapper, (float32_t*)input, (float32_t*)output, in_size);
                }
                break;
            }
            
            case EIF_LAYER_RNN: {
                // Allocate state from persistent buffer (survives across invokes)
                if (!node->params) return EIF_STATUS_INVALID_ARGUMENT;
                
                eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                eif_layer_param_t* lp = (eif_layer_param_t*)(qp + 1);
                
                int units = lp->rnn.units;
                float32_t* state = (float32_t*)(ctx->arena.persistent_base + persistent_offset);
                persistent_offset += units * sizeof(float32_t);
                
                eif_layer_t layer_wrapper = {0};
                layer_wrapper.quant_params = *qp;
                layer_wrapper.params = *lp;
                layer_wrapper.weights = ctx->tensor_data[node->input_indices[1]];
                layer_wrapper.biases = ctx->tensor_data[node->input_indices[2]];
                
                int input_size = in_tensor->dims[2];
                eif_layer_rnn(&layer_wrapper, (float32_t*)input, (float32_t*)output, input_size, state);
                break;
            }
            
            case EIF_LAYER_LSTM: {
                if (!node->params) return EIF_STATUS_INVALID_ARGUMENT;
                
                eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                eif_layer_param_t* lp = (eif_layer_param_t*)(qp + 1);
                
                int units = lp->lstm.units;
                float32_t* state = (float32_t*)(ctx->arena.persistent_base + persistent_offset);
                persistent_offset += 2 * units * sizeof(float32_t); // h and c
                
                eif_layer_t layer_wrapper = {0};
                layer_wrapper.quant_params = *qp;
                layer_wrapper.params = *lp;
                layer_wrapper.weights = ctx->tensor_data[node->input_indices[1]];
                layer_wrapper.biases = ctx->tensor_data[node->input_indices[2]];
                
                int input_size = in_tensor->dims[2];
                eif_layer_lstm(&layer_wrapper, (float32_t*)input, (float32_t*)output, input_size, state);
                break;
            }
            
            case EIF_LAYER_GRU: {
                if (!node->params) return EIF_STATUS_INVALID_ARGUMENT;
                
                eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                eif_layer_param_t* lp = (eif_layer_param_t*)(qp + 1);
                
                int units = lp->gru.units;
                float32_t* state = (float32_t*)(ctx->arena.persistent_base + persistent_offset);
                persistent_offset += units * sizeof(float32_t);
                
                eif_layer_t layer_wrapper = {0};
                layer_wrapper.quant_params = *qp;
                layer_wrapper.params = *lp;
                layer_wrapper.weights = ctx->tensor_data[node->input_indices[1]];
                layer_wrapper.biases = ctx->tensor_data[node->input_indices[2]];
                
                int input_size = in_tensor->dims[2];
                eif_layer_gru(&layer_wrapper, (float32_t*)input, (float32_t*)output, input_size, state);
                break;
            }
            
            case EIF_LAYER_CONV2D: {
                if (node->num_inputs < 2) return EIF_STATUS_ERROR;
                
                eif_layer_t layer_wrapper = {0};
                layer_wrapper.type = node->type;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    layer_wrapper.quant_params = *qp;
                    void* lp = (void*)(qp + 1);
                    layer_wrapper.params = *(eif_layer_param_t*)lp;
                }
                
                layer_wrapper.weights = ctx->tensor_data[node->input_indices[1]];
                if (node->num_inputs > 2) {
                    layer_wrapper.biases = ctx->tensor_data[node->input_indices[2]];
                }
                
                int in_h = in_tensor->dims[1];
                int in_w = in_tensor->dims[2];
                int in_c = in_tensor->dims[3];
                int out_h, out_w, out_c;
                
                if (in_tensor->type == EIF_TENSOR_INT8) {
                    eif_layer_conv2d_int8(&layer_wrapper, (int8_t*)input, (int8_t*)output, in_h, in_w, in_c, &out_h, &out_w, &out_c);
                } else {
                    eif_status_t status = eif_layer_conv2d(&layer_wrapper, (float32_t*)input, (float32_t*)output, in_h, in_w, in_c, &out_h, &out_w, &out_c);
                    if (status != EIF_STATUS_OK) return status;
                }
                break;
            }

            case EIF_LAYER_CONV1D: {
                if (node->num_inputs < 2) return EIF_STATUS_ERROR;
                
                eif_layer_t layer_wrapper = {0};
                layer_wrapper.type = node->type;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    layer_wrapper.quant_params = *qp;
                    void* lp = (void*)(qp + 1);
                    layer_wrapper.params = *(eif_layer_param_t*)lp;
                }
                
                layer_wrapper.weights = ctx->tensor_data[node->input_indices[1]];
                if (node->num_inputs > 2) {
                    layer_wrapper.biases = ctx->tensor_data[node->input_indices[2]];
                }
                
                // Conv1D input is typically (N, W, C) or (N, 1, W, C)
                // Assuming (N, 1, W, C) for compatibility with 4D tensor structure
                int in_w = in_tensor->dims[2];
                int in_c = in_tensor->dims[3];
                int out_w, out_c;
                
                eif_layer_conv1d(&layer_wrapper, (float32_t*)input, (float32_t*)output, in_w, in_c, &out_w, &out_c);
                break;
            }
            
            case EIF_LAYER_DEPTHWISE_CONV2D: {
                if (node->num_inputs < 2) return EIF_STATUS_ERROR;
                
                eif_layer_t layer_wrapper = {0};
                layer_wrapper.type = node->type;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    layer_wrapper.quant_params = *qp;
                    void* lp = (void*)(qp + 1);
                    layer_wrapper.params = *(eif_layer_param_t*)lp;
                }
                
                layer_wrapper.weights = ctx->tensor_data[node->input_indices[1]];
                if (node->num_inputs > 2) {
                    layer_wrapper.biases = ctx->tensor_data[node->input_indices[2]];
                }
                
                int in_h = in_tensor->dims[1];
                int in_w = in_tensor->dims[2];
                int in_c = in_tensor->dims[3];
                int out_h, out_w, out_c;
                
                if (in_tensor->type == EIF_TENSOR_INT8) {
                    eif_layer_depthwise_conv2d_int8(&layer_wrapper, (int8_t*)input, (int8_t*)output, in_h, in_w, in_c, &out_h, &out_w, &out_c);
                } else {
                    eif_layer_depthwise_conv2d(&layer_wrapper, (float32_t*)input, (float32_t*)output, in_h, in_w, in_c, &out_h, &out_w, &out_c);
                }
                break;
            }
            
            case EIF_LAYER_QUANTIZE: {
                eif_layer_t layer_wrapper = {0};
                layer_wrapper.type = node->type;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    layer_wrapper.quant_params = *qp;
                    void* lp = (void*)(qp + 1);
                    layer_wrapper.params = *(eif_layer_param_t*)lp;
                }
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_quantize(&layer_wrapper, (float32_t*)input, (int8_t*)output, size);
                break;
            }
            
            case EIF_LAYER_DEQUANTIZE: {
                eif_layer_t layer_wrapper = {0};
                layer_wrapper.type = node->type;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    layer_wrapper.quant_params = *qp;
                    void* lp = (void*)(qp + 1);
                    layer_wrapper.params = *(eif_layer_param_t*)lp;
                }
                int size = in_tensor->size_bytes; // Input is int8
                eif_layer_dequantize(&layer_wrapper, (int8_t*)input, (float32_t*)output, size);
                break;
            }
            
            case EIF_LAYER_TRANSPOSE_CONV2D: {
                if (node->num_inputs < 2) return EIF_STATUS_ERROR;
                
                eif_layer_t layer_wrapper = {0};
                layer_wrapper.type = node->type;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    layer_wrapper.quant_params = *qp;
                    void* lp = (void*)(qp + 1);
                    layer_wrapper.params = *(eif_layer_param_t*)lp;
                }
                
                layer_wrapper.weights = ctx->tensor_data[node->input_indices[1]];
                if (node->num_inputs > 2) {
                    layer_wrapper.biases = ctx->tensor_data[node->input_indices[2]];
                }

                int in_h = in_tensor->dims[1];
                int in_w = in_tensor->dims[2];
                int in_c = in_tensor->dims[3];
                int out_h, out_w, out_c;

                eif_layer_transpose_conv2d(&layer_wrapper, (float32_t*)input, (float32_t*)output, in_h, in_w, in_c, &out_h, &out_w, &out_c);
                break;
            }
            
            case EIF_LAYER_MAXPOOL2D: {
                eif_layer_t layer_wrapper = {0};
                layer_wrapper.type = node->type;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    layer_wrapper.quant_params = *qp;
                    void* lp = (void*)(qp + 1);
                    layer_wrapper.params = *(eif_layer_param_t*)lp;
                }
                
                int in_h = in_tensor->dims[1];
                int in_w = in_tensor->dims[2];
                int in_c = in_tensor->dims[3];
                int out_h, out_w;
                
                eif_layer_maxpool2d(&layer_wrapper, (float32_t*)input, (float32_t*)output, in_h, in_w, in_c, &out_h, &out_w);
                break;
            }
            
            case EIF_LAYER_AVGPOOL2D: {
                eif_layer_t layer_wrapper = {0};
                layer_wrapper.type = node->type;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    layer_wrapper.quant_params = *qp;
                    void* lp = (void*)(qp + 1);
                    layer_wrapper.params = *(eif_layer_param_t*)lp;
                }
                
                int in_h = in_tensor->dims[1];
                int in_w = in_tensor->dims[2];
                int in_c = in_tensor->dims[3];
                int out_h, out_w;
                
                eif_layer_avgpool2d(&layer_wrapper, (float32_t*)input, (float32_t*)output, in_h, in_w, in_c, &out_h, &out_w);
                break;
            }
            
            case EIF_LAYER_GLOBAL_AVGPOOL2D: {
                // GAP usually doesn't have params, but if it did (e.g. keep_dims), we'd need to skip quant_params
                int in_h = in_tensor->dims[1];
                int in_w = in_tensor->dims[2];
                int in_c = in_tensor->dims[3];
                eif_layer_global_avgpool2d((float32_t*)input, (float32_t*)output, in_h, in_w, in_c);
                break;
            }
            
            case EIF_LAYER_RESHAPE:
            case EIF_LAYER_FLATTEN: {
                int size = in_tensor->size_bytes / sizeof(float32_t);
                if (node->type == EIF_LAYER_RESHAPE) {
                    eif_layer_reshape((float32_t*)input, (float32_t*)output, size);
                } else {
                    eif_layer_flatten((float32_t*)input, (float32_t*)output, size);
                }
                break;
            }
            
            case EIF_LAYER_CONCAT: {
                eif_layer_param_t* params = NULL;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    void* lp = (void*)(qp + 1);
                    params = (eif_layer_param_t*)lp;
                }
                int axis = params ? params->concat.axis : 3;
                
                // Generic concat implementation for any axis
                if (axis < 0) {
                    axis = in_tensor->num_dims + axis; // Convert negative index
                }
                
                if (axis >= (int)in_tensor->num_dims) {
                    return EIF_STATUS_INVALID_ARGUMENT;
                }
                
                int N = in_tensor->dims[0];
                int H = (in_tensor->num_dims > 1) ? in_tensor->dims[1] : 1;
                int W = (in_tensor->num_dims > 2) ? in_tensor->dims[2] : 1;
                int C = (in_tensor->num_dims > 3) ? in_tensor->dims[3] : 1;
                
                // Calculate output dimension on concat axis
                int out_dim_on_axis = 0;
                for(int j = 0; j < node->num_inputs; j++) {
                    int idx = node->input_indices[j];
                    out_dim_on_axis += model->tensors[idx].dims[axis];
                }
                
                float32_t* out_ptr = (float32_t*)output;
                int offset = 0;
                
                // Axis 0 (batch): stack along batch dimension
                if (axis == 0) {
                    for(int j = 0; j < node->num_inputs; j++) {
                        int idx = node->input_indices[j];
                        float32_t* in_ptr = (float32_t*)ctx->tensor_data[idx];
                        int batch_size = model->tensors[idx].dims[0];
                        int elements_per_batch = H * W * C;
                        memcpy(out_ptr + offset, in_ptr, batch_size * elements_per_batch * sizeof(float32_t));
                        offset += batch_size * elements_per_batch;
                    }
                }
                // Axis 3 (channels): interleave along channel dimension
                else if (axis == 3 || (axis == -1 && in_tensor->num_dims == 4)) {
                    int current_c_offset = 0;
                    for(int j = 0; j < node->num_inputs; j++) {
                        int idx = node->input_indices[j];
                        float32_t* in_ptr = (float32_t*)ctx->tensor_data[idx];
                        int c = model->tensors[idx].dims[3];
                        
                        for(int n = 0; n < N; n++) {
                            for(int h = 0; h < H; h++) {
                                for(int w = 0; w < W; w++) {
                                    int out_idx_val = ((n*H + h)*W + w)*out_dim_on_axis + current_c_offset;
                                    int in_idx_val = ((n*H + h)*W + w)*c;
                                    memcpy(out_ptr + out_idx_val, in_ptr + in_idx_val, c * sizeof(float32_t));
                                }
                            }
                        }
                        current_c_offset += c;
                    }
                }
                // Axis 1 or 2: generic concat for spatial dimensions
                else {
                    // Simplified: concatenate along specified spatial axis
                    // This is less optimized but handles all cases
                    for(int j = 0; j < node->num_inputs; j++) {
                        int idx = node->input_indices[j];
                        float32_t* in_ptr = (float32_t*)ctx->tensor_data[idx];
                        eif_tensor_t* in = &model->tensors[idx];
                        
                        // Calculate strides
                        int in_elements = 1;
                        for(uint32_t d = 0; d < in->num_dims; d++) {
                            in_elements *= in->dims[d];
                        }
                        memcpy(out_ptr + offset, in_ptr, in_elements * sizeof(float32_t));
                        offset += in_elements;
                    }
                }
                break;
            }
            
            case EIF_LAYER_SOFTMAX: {
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_softmax((float32_t*)input, (float32_t*)output, size);
                break;
            }
            
            case EIF_LAYER_SIGMOID: {
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_sigmoid((float32_t*)input, (float32_t*)output, size);
                break;
            }
            
            case EIF_LAYER_TANH: {
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_tanh((float32_t*)input, (float32_t*)output, size);
                break;
            }
            
            case EIF_LAYER_LEAKY_RELU: {
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_t layer_wrapper = {0};
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    layer_wrapper.quant_params = *qp;
                    void* lp = (void*)(qp + 1);
                    layer_wrapper.params = *(eif_layer_param_t*)lp;
                }
                eif_layer_leaky_relu(&layer_wrapper, (float32_t*)input, (float32_t*)output, size);
                break;
            }
            
            case EIF_LAYER_CLIP: {
                eif_layer_t layer_wrapper = {0};
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    layer_wrapper.quant_params = *qp;
                    void* lp = (void*)(qp + 1);
                    layer_wrapper.params = *(eif_layer_param_t*)lp;
                } else {
                    // Default to no clip if no params? Or full range.
                    layer_wrapper.params.clip.min_val = -FLT_MAX;
                    layer_wrapper.params.clip.max_val = FLT_MAX;
                }
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_clip(&layer_wrapper, (float32_t*)input, (float32_t*)output, size);
                break;
            }

            case EIF_LAYER_ADD: {
                if (node->num_inputs < 2) return EIF_STATUS_ERROR;
                void* input2 = ctx->tensor_data[node->input_indices[1]];
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_add((float32_t*)input, (float32_t*)input2, (float32_t*)output, size);
                break;
            }

            case EIF_LAYER_MULTIPLY: {
                if (node->num_inputs < 2) return EIF_STATUS_ERROR;
                void* input2 = ctx->tensor_data[node->input_indices[1]];
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_multiply((float32_t*)input, (float32_t*)input2, (float32_t*)output, size);
                break;
            }

            case EIF_LAYER_GELU: {
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_gelu((float32_t*)input, (float32_t*)output, size);
                break;
            }

            case EIF_LAYER_HARD_SWISH: {
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_hard_swish((float32_t*)input, (float32_t*)output, size);
                break;
            }
            
            case EIF_LAYER_BATCH_NORM: {
                if (node->num_inputs < 5) return EIF_STATUS_ERROR;
                float32_t* gamma = (float32_t*)ctx->tensor_data[node->input_indices[1]];
                float32_t* beta = (float32_t*)ctx->tensor_data[node->input_indices[2]];
                float32_t* mean = (float32_t*)ctx->tensor_data[node->input_indices[3]];
                float32_t* var = (float32_t*)ctx->tensor_data[node->input_indices[4]];
                
                eif_layer_param_t* params = NULL;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    void* lp = (void*)(qp + 1);
                    params = (eif_layer_param_t*)lp;
                }
                
                eif_tensor_shape_t shape;
                for(int k = 0; k < 4; k++) shape.dim[k] = in_tensor->dims[k];
                
                eif_status_t status = eif_layer_batch_norm((float32_t*)input, (float32_t*)output, &shape, params, mean, var, gamma, beta);
                if (status != EIF_STATUS_OK) return status;
                break;
            }

            case EIF_LAYER_DROPOUT: {
                eif_tensor_shape_t in_shape;
                for(int k=0; k<4; k++) in_shape.dim[k] = in_tensor->dims[k];
                
                eif_layer_param_t* params = NULL;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    params = (eif_layer_param_t*)(qp + 1);
                }
                eif_status_t status = eif_layer_dropout((float32_t*)input, (float32_t*)output, &in_shape, params);
                if (status != EIF_STATUS_OK) return status;
                break;
            }
            
            case EIF_LAYER_LAYER_NORM: {
                eif_layer_t layer_wrapper = {0};
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    layer_wrapper.quant_params = *qp;
                    void* lp = (void*)(qp + 1);
                    layer_wrapper.params = *(eif_layer_param_t*)lp;
                }
                if (node->num_inputs > 1) layer_wrapper.weights = ctx->tensor_data[node->input_indices[1]];
                if (node->num_inputs > 2) layer_wrapper.biases = ctx->tensor_data[node->input_indices[2]];
                
                int h = in_tensor->dims[1];
                int w = in_tensor->dims[2];
                int c = in_tensor->dims[3];
                eif_layer_layer_norm(&layer_wrapper, (float32_t*)input, (float32_t*)output, h, w, c);
                break;
            }
            
            case EIF_LAYER_RESIZE: {
                eif_tensor_shape_t in_shape, out_shape;
                for(int k = 0; k < 4; k++) {
                    in_shape.dim[k] = in_tensor->dims[k];
                    out_shape.dim[k] = out_tensor->dims[k];
                }
                eif_layer_param_t* params = NULL;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    void* lp = (void*)(qp + 1);
                    params = (eif_layer_param_t*)lp;
                }
                eif_status_t status = eif_layer_resize((float32_t*)input, (float32_t*)output, &in_shape, &out_shape, params);
                if (status != EIF_STATUS_OK) return status;
                break;
            }
            
            case EIF_LAYER_ATTENTION: {
                if (node->num_inputs < 3) return EIF_STATUS_ERROR;
                float32_t* q = (float32_t*)ctx->tensor_data[node->input_indices[0]];
                float32_t* k = (float32_t*)ctx->tensor_data[node->input_indices[1]];
                float32_t* v = (float32_t*)ctx->tensor_data[node->input_indices[2]];
                
                eif_tensor_t* t_q = &model->tensors[node->input_indices[0]];
                int batch = t_q->dims[0];
                int seq_len = t_q->dims[1];
                int embed_dim = t_q->dims[2];
                
                eif_status_t status = eif_layer_attention(q, k, v, (float32_t*)output, batch, seq_len, embed_dim);
                if (status != EIF_STATUS_OK) return status;
                break;
            }

            // --- New Operators Dispatch ---
            case EIF_LAYER_SUB: {
                if (node->num_inputs < 2) return EIF_STATUS_ERROR;
                void* input2 = ctx->tensor_data[node->input_indices[1]];
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_sub((float32_t*)input, (float32_t*)input2, (float32_t*)output, size);
                break;
            }
            case EIF_LAYER_DIV: {
                if (node->num_inputs < 2) return EIF_STATUS_ERROR;
                void* input2 = ctx->tensor_data[node->input_indices[1]];
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_div((float32_t*)input, (float32_t*)input2, (float32_t*)output, size);
                break;
            }
            case EIF_LAYER_EXP: {
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_exp((float32_t*)input, (float32_t*)output, size);
                break;
            }
            case EIF_LAYER_LOG: {
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_log((float32_t*)input, (float32_t*)output, size);
                break;
            }
            case EIF_LAYER_SQRT: {
                int size = in_tensor->size_bytes / sizeof(float32_t);
                eif_layer_sqrt((float32_t*)input, (float32_t*)output, size);
                break;
            }
            case EIF_LAYER_SPLIT: {
                // Outputs are multiple
                // We need to gather output pointers
                // This is tricky because 'output' arg is just one pointer.
                // But eif_neural_invoke loop sets output = ctx->tensor_data[node->output_indices[0]];
                // We need all output pointers.
                #define MAX_SPLIT_OUTPUTS 8
                void* outputs[MAX_SPLIT_OUTPUTS];
                int num_outputs = node->num_outputs;
                if (num_outputs > MAX_SPLIT_OUTPUTS) return EIF_STATUS_NOT_IMPLEMENTED;
                
                for(int i=0; i<num_outputs; i++) {
                    outputs[i] = ctx->tensor_data[node->output_indices[i]];
                }
                
                eif_tensor_shape_t in_shape;
                for(int k=0; k<4; k++) in_shape.dim[k] = in_tensor->dims[k];
                
                eif_layer_param_t* params = NULL;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    params = (eif_layer_param_t*)(qp + 1);
                }
                eif_status_t status = eif_layer_split((float32_t*)input, outputs, &in_shape, params, num_outputs);
                if (status != EIF_STATUS_OK) return status;
                break;
            }
            case EIF_LAYER_PAD: {
                eif_tensor_shape_t in_shape, out_shape;
                for(int k=0; k<4; k++) {
                    in_shape.dim[k] = in_tensor->dims[k];
                    out_shape.dim[k] = out_tensor->dims[k];
                }
                eif_layer_param_t* params = NULL;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    params = (eif_layer_param_t*)(qp + 1);
                }
                eif_status_t status = eif_layer_pad((float32_t*)input, (float32_t*)output, &in_shape, &out_shape, params);
                if (status != EIF_STATUS_OK) return status;
                break;
            }
            case EIF_LAYER_GATHER: {
                if (node->num_inputs < 2) return EIF_STATUS_ERROR;
                float32_t* indices = (float32_t*)ctx->tensor_data[node->input_indices[1]];
                eif_tensor_shape_t in_shape, out_shape;
                for(int k=0; k<4; k++) {
                    in_shape.dim[k] = in_tensor->dims[k];
                    out_shape.dim[k] = out_tensor->dims[k];
                }
                eif_layer_param_t* params = NULL;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    params = (eif_layer_param_t*)(qp + 1);
                }
                eif_status_t status = eif_layer_gather((float32_t*)input, indices, (float32_t*)output, &in_shape, &out_shape, params);
                if (status != EIF_STATUS_OK) return status;
                break;
            }
            case EIF_LAYER_MATMUL: {
                if (node->num_inputs < 2) return EIF_STATUS_ERROR;
                float32_t* b = (float32_t*)ctx->tensor_data[node->input_indices[1]];
                // Dimensions: A[M, K], B[K, N] -> C[M, N]
                // Assuming 2D for simplicity or last 2 dims
                int M = in_tensor->dims[in_tensor->num_dims - 2];
                int K = in_tensor->dims[in_tensor->num_dims - 1];
                int N = out_tensor->dims[out_tensor->num_dims - 1];
                eif_status_t status = eif_layer_matmul((float32_t*)input, b, (float32_t*)output, M, K, N);
                if (status != EIF_STATUS_OK) return status;
                break;
            }
            case EIF_LAYER_REDUCE_MEAN: {
                eif_tensor_shape_t in_shape, out_shape;
                for(int k=0; k<4; k++) {
                    in_shape.dim[k] = in_tensor->dims[k];
                    out_shape.dim[k] = out_tensor->dims[k];
                }
                eif_layer_param_t* params = NULL;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    params = (eif_layer_param_t*)(qp + 1);
                }
                eif_status_t status = eif_layer_reduce_mean((float32_t*)input, (float32_t*)output, &in_shape, &out_shape, params);
                if (status != EIF_STATUS_OK) return status;
                break;
            }
            case EIF_LAYER_REDUCE_SUM: {
                eif_tensor_shape_t in_shape, out_shape;
                for(int k=0; k<4; k++) {
                    in_shape.dim[k] = in_tensor->dims[k];
                    out_shape.dim[k] = out_tensor->dims[k];
                }
                eif_layer_param_t* params = NULL;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    params = (eif_layer_param_t*)(qp + 1);
                }
                eif_status_t status = eif_layer_reduce_sum((float32_t*)input, (float32_t*)output, &in_shape, &out_shape, params);
                if (status != EIF_STATUS_OK) return status;
                break;
            }
            case EIF_LAYER_TOPK: {
                // TopK has 2 outputs: Values, Indices
                // output arg is Values
                // We need to find Indices buffer
                if (node->num_outputs < 2) return EIF_STATUS_ERROR;
                float32_t* indices = (float32_t*)ctx->tensor_data[node->output_indices[1]];
                
                eif_tensor_shape_t in_shape;
                // Pad from left to ensure the last dimension ends up at dim[3]
                // This is required because eif_layer_topk hardcodes dim[3] as the target dimension
                int offset = 4 - in_tensor->num_dims;
                if (offset < 0) offset = 0;
                
                for(int k=0; k<4; k++) {
                    if (k < offset) {
                        in_shape.dim[k] = 1;
                    } else {
                        in_shape.dim[k] = in_tensor->dims[k - offset];
                    }
                }
                
                eif_layer_param_t* params = NULL;
                if (node->params) {
                    eif_quant_param_t* qp = (eif_quant_param_t*)node->params;
                    params = (eif_layer_param_t*)(qp + 1);
                }
                eif_status_t status = eif_layer_topk((float32_t*)input, (float32_t*)output, indices, &in_shape, params);
                if (status != EIF_STATUS_OK) return status;
                break;
            }
            
            case EIF_LAYER_EMBEDDING: {
                if (node->num_inputs < 2) return EIF_STATUS_ERROR;
                float32_t* in_data = (float32_t*)ctx->tensor_data[node->input_indices[0]];
                float32_t* weights = (float32_t*)ctx->tensor_data[node->input_indices[1]];
                
                eif_tensor_t* t_in = &model->tensors[node->input_indices[0]];
                eif_tensor_t* t_w = &model->tensors[node->input_indices[1]];
                
                int batch = t_in->dims[0];
                int seq_len = t_in->dims[1];
                int vocab_size = t_w->dims[0];
                int embed_dim = t_w->dims[1];
                
                eif_status_t status = eif_layer_embedding(in_data, (float32_t*)output, weights, batch, seq_len, vocab_size, embed_dim);
                if (status != EIF_STATUS_OK) return status;
                break;
            }
            
            default:
                // Unsupported layer
                break;
        }
    }
    
    return EIF_STATUS_OK;
}
