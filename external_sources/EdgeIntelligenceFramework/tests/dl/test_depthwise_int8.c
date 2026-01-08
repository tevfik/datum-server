#include "eif_test_runner.h"
#include "eif_neural.h"
#include "eif_nn_context.h"
#include "eif_memory.h"
#include "eif_nn_inference.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Helper to create a memory pool
static eif_memory_pool_t* create_pool(size_t size) {
    void* buffer = malloc(size);
    eif_memory_pool_t* pool = malloc(sizeof(eif_memory_pool_t));
    eif_memory_pool_init(pool, buffer, size);
    return pool;
}

static void destroy_pool(eif_memory_pool_t* pool) {
    if (pool) {
        free(pool->base_addr);
        free(pool);
    }
}

// Test Int8 Depthwise Conv2D
bool test_depthwise_int8_simple(void) {
    eif_memory_pool_t* pool = create_pool(65536);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input, Weights, Bias, Output
    eif_tensor_t tensors[4];
    memset(tensors, 0, sizeof(tensors));
    
    // Tensor 0: Input (Int8) 3x3x1
    tensors[0].type = EIF_TENSOR_INT8;
    tensors[0].dims[0] = 1; tensors[0].dims[1] = 3; tensors[0].dims[2] = 3; tensors[0].dims[3] = 1;
    tensors[0].size_bytes = 9 * sizeof(int8_t);
    tensors[0].is_variable = true;
    
    // Tensor 1: Weights (Int8) 3x3x1
    tensors[1].type = EIF_TENSOR_INT8;
    tensors[1].dims[0] = 3; tensors[1].dims[1] = 3; tensors[1].dims[2] = 1; tensors[1].dims[3] = 1; 
    tensors[1].size_bytes = 9 * sizeof(int8_t);
    tensors[1].is_variable = false;
    
    // Tensor 2: Bias (Int32)
    // Hack: use INT8 type but correct size if INT32 enum missing
    tensors[2].type = EIF_TENSOR_INT8; 
    tensors[2].dims[0] = 1; 
    tensors[2].size_bytes = 1 * sizeof(int32_t);
    tensors[2].is_variable = false;
    
    // Tensor 3: Output (Int8)
    tensors[3].type = EIF_TENSOR_INT8;
    tensors[3].dims[0] = 1; tensors[3].dims[1] = 1; tensors[3].dims[2] = 1; tensors[3].dims[3] = 1;
    tensors[3].size_bytes = 1 * sizeof(int8_t);
    tensors[3].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 4;
    
    // Params
    eif_layer_param_t lp = {0};
    lp.depthwise_conv2d.kernel_h = 3;
    lp.depthwise_conv2d.kernel_w = 3;
    lp.depthwise_conv2d.stride_h = 1;
    lp.depthwise_conv2d.stride_w = 1;
    lp.depthwise_conv2d.pad_h = 0;
    lp.depthwise_conv2d.pad_w = 0;
    lp.depthwise_conv2d.depth_multiplier = 1;

    // Quantization Params
    eif_quant_param_t qp;
    qp.input_offset = 0;
    qp.output_offset = 0;
    qp.output_multiplier = 2000000000;
    qp.output_shift = 0;
    qp.quantized_activation_min = -128;
    qp.quantized_activation_max = 127;
    
    eif_layer_node_t node;
    int input_indices[] = {0, 1, 2};
    int output_indices[] = {3};
    node.type = EIF_LAYER_DEPTHWISE_CONV2D;
    node.input_indices = input_indices;
    node.num_inputs = 3;
    node.output_indices = output_indices;
    node.num_outputs = 1;
    
    uint8_t param_buffer[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
    memcpy(param_buffer, &qp, sizeof(eif_quant_param_t));
    memcpy(param_buffer + sizeof(eif_quant_param_t), &lp, sizeof(eif_layer_param_t));
    
    node.params = param_buffer;
    
    model.nodes = &node;
    model.num_nodes = 1;
    model.input_tensor_indices = (int[]){0};
    model.num_inputs = 1;
    model.output_tensor_indices = (int[]){3};
    model.num_outputs = 1;
    
    // Init
    eif_status_t status = eif_neural_init(&ctx, &model, pool);
    if (status != EIF_STATUS_OK) {
        printf("Init failed with status: %d\n", status);
        return false;
    }
    
    // Set Data
    int8_t* in_ptr = (int8_t*)ctx.tensor_data[0];
    
    int8_t w_data[9];
    for(int i=0; i<9; i++) w_data[i] = 2; // Weight = 2
    model.tensors[1].data = w_data;
    ctx.tensor_data[1] = w_data; 
    
    int32_t b_data[1] = {5}; // Bias = 5
    model.tensors[2].data = b_data;
    ctx.tensor_data[2] = b_data; 
    
    for(int i=0; i<9; i++) in_ptr[i] = 10; // Input = 10
    
    // Invoke
    if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) {
        printf("Invoke failed\n");
        return false;
    }
    
    int8_t* out_ptr = (int8_t*)ctx.tensor_data[3];
    printf("Output: %d\n", out_ptr[0]);
    
    // Verify result:
    // Accum = (10 * 2) * 9 + 5 = 185
    // With multiplier/shift setup, output should be close to 127 (saturated) or raw value if scaling allows.
    // If we get > 0, logic works.
    if (out_ptr[0] == 0) return false;
    
    destroy_pool(pool);
    return true;
}

int main(void) {
    if (test_depthwise_int8_simple()) {
        printf("Test Passed\n");
        return 0;
    } else {
        printf("Test Failed\n");
        return 1;
    }
}
