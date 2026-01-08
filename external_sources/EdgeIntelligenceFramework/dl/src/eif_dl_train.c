#include "eif_neural.h"
#include "eif_dl_internal.h"
#include <string.h>
#include <math.h>

// Helper: Get tensor data (forward or gradient)
static float32_t* get_tensor_data(eif_neural_context_t* ctx, int idx) {
    return (float32_t*)ctx->tensor_data[idx];
}

static float32_t* get_tensor_grad(eif_neural_train_ctx_t* train_ctx, int idx) {
    return (float32_t*)train_ctx->tensor_gradients[idx];
}

eif_status_t eif_neural_train_init(eif_neural_train_ctx_t* train_ctx, eif_neural_context_t* ctx, const eif_optimizer_t* opt, eif_memory_pool_t* pool) {
    train_ctx->ctx = ctx;
    train_ctx->optimizer = *opt;
    
    // Allocate pointers for gradients
    train_ctx->tensor_gradients = (void**)eif_memory_alloc(pool, ctx->model->num_tensors * sizeof(void*), 4);
    if (!train_ctx->tensor_gradients) return EIF_STATUS_ERROR;
    
    // Allocate gradient buffers for all tensors (activations and weights)
    // In a real optimized engine, we'd only allocate for weights and necessary activations.
    // For simplicity, we allocate for everything.
    for (int i = 0; i < ctx->model->num_tensors; i++) {
        eif_tensor_t* t = &ctx->model->tensors[i];
        train_ctx->tensor_gradients[i] = eif_memory_alloc(pool, t->size_bytes, 4);
        if (!train_ctx->tensor_gradients[i]) return EIF_STATUS_ERROR;
        memset(train_ctx->tensor_gradients[i], 0, t->size_bytes);
    }
    
    return EIF_STATUS_OK;
}

// Backward for Dense Layer
// y = x * W + b
// dL/dx = dL/dy * W^T
// dL/dW = x^T * dL/dy
// dL/db = sum(dL/dy)
static void backward_dense(eif_neural_train_ctx_t* train_ctx, const eif_layer_node_t* node) {
    eif_neural_context_t* ctx = train_ctx->ctx;
    
    int input_idx = node->input_indices[0];
    int weight_idx = node->input_indices[1];
    int bias_idx = (node->num_inputs > 2) ? node->input_indices[2] : -1;
    int output_idx = node->output_indices[0];
    
    float32_t* input = get_tensor_data(ctx, input_idx);
    float32_t* weights = get_tensor_data(ctx, weight_idx);
    float32_t* output_grad = get_tensor_grad(train_ctx, output_idx);
    
    float32_t* input_grad = get_tensor_grad(train_ctx, input_idx);
    float32_t* weight_grad = get_tensor_grad(train_ctx, weight_idx);
    float32_t* bias_grad = (bias_idx >= 0) ? get_tensor_grad(train_ctx, bias_idx) : NULL;
    
    eif_tensor_t* in_t = &ctx->model->tensors[input_idx];
    eif_tensor_t* w_t = &ctx->model->tensors[weight_idx];
    
    // Shapes
    // Input: [Batch, In] (Batch=1 usually)
    // Weights: [Out, In] (Standard EIF/TFLite convention for FullyConnected?) 
    // Wait, TFLite FullyConnected weights are [Out, In].
    // Let's assume Batch=1 for now.
    
    int n_in = in_t->dims[in_t->num_dims - 1];
    int n_out = w_t->dims[0]; // Weights are [Out, In]
    
    // 1. dL/dW = output_grad * input^T
    // output_grad: [1, Out]
    // input: [1, In]
    // dL/dW: [Out, In]
    for (int o = 0; o < n_out; o++) {
        float32_t grad_o = output_grad[o];
        for (int i = 0; i < n_in; i++) {
            weight_grad[o * n_in + i] += grad_o * input[i];
        }
        // dL/db
        if (bias_grad) {
            bias_grad[o] += grad_o;
        }
    }
    
    // 2. dL/dx = output_grad * W
    // dL/dx: [1, In]
    // W: [Out, In]
    for (int i = 0; i < n_in; i++) {
        float32_t sum = 0.0f;
        for (int o = 0; o < n_out; o++) {
            sum += output_grad[o] * weights[o * n_in + i];
        }
        input_grad[i] += sum;
    }
}

// Backward for ReLU
// y = max(0, x)
// dL/dx = dL/dy if x > 0 else 0
static void backward_relu(eif_neural_train_ctx_t* train_ctx, const eif_layer_node_t* node) {
    eif_neural_context_t* ctx = train_ctx->ctx;
    int input_idx = node->input_indices[0];
    int output_idx = node->output_indices[0];
    
    float32_t* input = get_tensor_data(ctx, input_idx);
    float32_t* output_grad = get_tensor_grad(train_ctx, output_idx);
    float32_t* input_grad = get_tensor_grad(train_ctx, input_idx);
    
    int size = 1;
    eif_tensor_t* t = &ctx->model->tensors[input_idx];
    for(int i=0; i<t->num_dims; i++) size *= t->dims[i];
    
    for (int i = 0; i < size; i++) {
        if (input[i] > 0) {
            input_grad[i] += output_grad[i];
        }
    }
}

eif_status_t eif_neural_backward(eif_neural_train_ctx_t* train_ctx, const float32_t* target) {
    eif_neural_context_t* ctx = train_ctx->ctx;
    
    // 1. Compute Output Gradient (MSE Loss)
    // L = 0.5 * (y - target)^2
    // dL/dy = (y - target)
    int out_idx = ctx->model->output_tensor_indices[0];
    float32_t* output = get_tensor_data(ctx, out_idx);
    float32_t* output_grad = get_tensor_grad(train_ctx, out_idx);
    
    eif_tensor_t* out_t = &ctx->model->tensors[out_idx];
    int out_size = 1;
    for(int i=0; i<out_t->num_dims; i++) out_size *= out_t->dims[i];
    
    for (int i = 0; i < out_size; i++) {
        output_grad[i] = (output[i] - target[i]);
    }
    
    // 2. Iterate backwards
    for (int i = ctx->model->num_nodes - 1; i >= 0; i--) {
        eif_layer_node_t* node = &ctx->model->nodes[i];
        
        switch (node->type) {
            case EIF_LAYER_DENSE:
                backward_dense(train_ctx, node);
                break;
            case EIF_LAYER_RELU:
                backward_relu(train_ctx, node);
                break;
            default:
                // Skip or error? For now skip, assuming simple DQN structure
                break;
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_neural_update(eif_neural_train_ctx_t* train_ctx) {
    eif_neural_context_t* ctx = train_ctx->ctx;
    float32_t lr = train_ctx->optimizer.learning_rate;
    
    for (int i = 0; i < ctx->model->num_tensors; i++) {
        eif_tensor_t* t = &ctx->model->tensors[i];
        // Only update variables (weights/biases) that are not inputs/activations?
        // In our model, weights are tensors.
        // We need to know which tensors are weights.
        // Heuristic: Tensors that are inputs to layers but not graph inputs are weights/biases?
        // Or check `is_variable`? In inference `is_variable` means RAM (activations).
        // Weights are usually const (Flash).
        // For training, we must have copied them to RAM or `data` points to RAM.
        
        // For now, update everything that has a gradient and is not a graph input/output?
        // Better: Update if it's a weight.
        // Let's assume we update all tensors that are NOT graph inputs/outputs and have gradients?
        // No, activations have gradients but shouldn't be updated.
        
        // We need to identify weights.
        // In `backward_dense`, we computed `weight_grad`.
        // We can just iterate and update if `weight_grad` is non-zero?
        // But `input_grad` is also non-zero.
        
        // Let's rely on the fact that weights are usually parameters 1 and 2 of Dense layer.
        // But here we are iterating tensors.
        
        // Note: Future enhancement - implement trainable tensor marking mechanism
        // Let's assume we only update tensors that were originally CONST (is_variable=false) but now we are training them?
        // But `is_variable` in `eif_tensor_t` means "allocated in arena".
        // Weights usually have `data` set and `is_variable=false`.
        
        if (!t->is_variable && t->data != NULL) {
            // This is a weight tensor.
            float32_t* data = (float32_t*)t->data; // This must be mutable!
            float32_t* grad = get_tensor_grad(train_ctx, i);
            
            int size = t->size_bytes / sizeof(float32_t);
            for (int k = 0; k < size; k++) {
                data[k] -= lr * grad[k];
                grad[k] = 0; // Reset gradient
            }
        }
    }
    
    // Also reset activation gradients
    for (int i = 0; i < ctx->model->num_tensors; i++) {
        eif_tensor_t* t = &ctx->model->tensors[i];
        if (t->is_variable) {
             float32_t* grad = get_tensor_grad(train_ctx, i);
             memset(grad, 0, t->size_bytes);
        }
    }
    
    return EIF_STATUS_OK;
}
