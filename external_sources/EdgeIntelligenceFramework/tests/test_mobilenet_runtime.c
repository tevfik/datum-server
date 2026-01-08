#include "eif_neural.h"
#include "eif_dl_internal.h" // For internal layer access if needed
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Simple test to verify DepthwiseConv2D -> ReLU6 -> GlobalAvgPool pipeline
// Manually constructing the EIF model structure in memory.

#define POOL_SIZE 1024 * 1024

int main() {
    printf("Starting MobileNet Block Runtime Test...\n");

    // 1. Setup Memory Pool
    uint8_t* pool_buffer = (uint8_t*)malloc(POOL_SIZE);
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, POOL_SIZE);

    // 2. Define Tensors
    // T0: Input [1, 4, 4, 2] (Batch, H, W, C) - Small input for testing
    // T1: Weights Depthwise [1, 3, 3, 2] (K_H, K_W, C, M) - M=1
    // T2: Output Depthwise [1, 4, 4, 2] (Same padding)
    // T3: Output ReLU6 [1, 4, 4, 2]
    // T4: Output GlobalAvgPool [1, 1, 1, 2]
    
    #define NUM_TENSORS 5
    eif_tensor_t tensors[NUM_TENSORS];
    
    // T0: Input
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].num_dims = 4;
    tensors[0].dims[0]=1; tensors[0].dims[1]=4; tensors[0].dims[2]=4; tensors[0].dims[3]=2;
    tensors[0].size_bytes = 1 * 4 * 4 * 2 * sizeof(float);
    tensors[0].is_variable = 1; // Input is variable
    
    // T1: Weights (Constant)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].num_dims = 4;
    tensors[1].dims[0]=1; tensors[1].dims[1]=3; tensors[1].dims[2]=3; tensors[1].dims[3]=2; // OHWI format for Depthwise? 
    // EIF Depthwise expects: [K_H, K_W, C, M] flattened? 
    // Let's check implementation: weights[(ky * k_w + kx) * o_c + out_channel]
    // o_c = in_c * multiplier.
    // So it expects [K_H, K_W, O_C].
    tensors[1].size_bytes = 3 * 3 * 2 * sizeof(float);
    tensors[1].is_variable = 0;
    float weights_data[3*3*2];
    for(int i=0; i<18; i++) weights_data[i] = 1.0f; // All ones kernel
    tensors[1].data = weights_data;

    // T2: Output Depthwise (Variable)
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].num_dims = 4;
    tensors[2].dims[0]=1; tensors[2].dims[1]=4; tensors[2].dims[2]=4; tensors[2].dims[3]=2;
    tensors[2].size_bytes = 1 * 4 * 4 * 2 * sizeof(float);
    tensors[2].is_variable = 1;

    // T3: Output ReLU6 (Variable)
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].num_dims = 4;
    tensors[3].dims[0]=1; tensors[3].dims[1]=4; tensors[3].dims[2]=4; tensors[3].dims[3]=2;
    tensors[3].size_bytes = 1 * 4 * 4 * 2 * sizeof(float);
    tensors[3].is_variable = 1;

    // T4: Output GAP (Variable)
    tensors[4].type = EIF_TENSOR_FLOAT32;
    tensors[4].num_dims = 4;
    tensors[4].dims[0]=1; tensors[4].dims[1]=1; tensors[4].dims[2]=1; tensors[4].dims[3]=2;
    tensors[4].size_bytes = 1 * 1 * 1 * 2 * sizeof(float);
    tensors[4].is_variable = 1;

    // 3. Define Nodes
    #define NUM_NODES 3
    eif_layer_node_t nodes[NUM_NODES];
    eif_layer_param_t params[NUM_NODES];

    // Node 0: Depthwise Conv2D
    // Inputs: T0, T1. Outputs: T2.
    int n0_in[] = {0, 1};
    int n0_out[] = {2};
    nodes[0].type = EIF_LAYER_DEPTHWISE_CONV2D;
    nodes[0].num_inputs = 2;
    nodes[0].input_indices = n0_in;
    nodes[0].num_outputs = 1;
    nodes[0].output_indices = n0_out;
    params[0].depthwise_conv2d.kernel_h = 3;
    params[0].depthwise_conv2d.kernel_w = 3;
    params[0].depthwise_conv2d.stride_h = 1;
    params[0].depthwise_conv2d.stride_w = 1;
    params[0].depthwise_conv2d.depth_multiplier = 1;
    // Padding SAME for 4x4 input with 3x3 kernel stride 1 -> Output 4x4
    // EIF currently does VALID padding logic in the code: o_h = (in_h - k_h) / stride + 1
    // (4-3)/1 + 1 = 2. 
    // Wait, the implementation I saw earlier:
    // int o_h = (in_h - k_h) / stride_h + 1;
    // This is VALID padding.
    // To support SAME, I need to handle padding or adjust input size.
    // For this test, let's assume VALID padding behavior.
    // Input 4x4, Kernel 3x3 -> Output 2x2.
    // Let's update Tensor dims for T2 and T3 to be 2x2.
    tensors[2].dims[1]=2; tensors[2].dims[2]=2;
    tensors[2].size_bytes = 1 * 2 * 2 * 2 * sizeof(float);
    tensors[3].dims[1]=2; tensors[3].dims[2]=2;
    tensors[3].size_bytes = 1 * 2 * 2 * 2 * sizeof(float);
    
    nodes[0].params = &params[0];

    // Node 1: ReLU6
    // Inputs: T2. Outputs: T3.
    int n1_in[] = {2};
    int n1_out[] = {3};
    nodes[1].type = EIF_LAYER_RELU6;
    nodes[1].num_inputs = 1;
    nodes[1].input_indices = n1_in;
    nodes[1].num_outputs = 1;
    nodes[1].output_indices = n1_out;
    nodes[1].params = NULL;

    // Node 2: Global Avg Pool
    // Inputs: T3. Outputs: T4.
    int n2_in[] = {3};
    int n2_out[] = {4};
    nodes[2].type = EIF_LAYER_GLOBAL_AVGPOOL2D;
    nodes[2].num_inputs = 1;
    nodes[2].input_indices = n2_in;
    nodes[2].num_outputs = 1;
    nodes[2].output_indices = n2_out;
    nodes[2].params = NULL;

    // 4. Construct Model
    eif_model_t model;
    memset(&model, 0, sizeof(eif_model_t)); // Initialize to zero
    model.tensors = tensors;
    model.num_tensors = NUM_TENSORS;
    model.nodes = nodes;
    model.num_nodes = NUM_NODES;
    int model_inputs[] = {0};
    model.input_tensor_indices = model_inputs;
    model.num_inputs = 1;
    int model_outputs[] = {4};
    model.output_tensor_indices = model_outputs;
    model.num_outputs = 1;

    // 5. Initialize Runtime
    eif_neural_context_t ctx;
    eif_status_t status = eif_neural_init(&ctx, &model, &pool);
    if (status != EIF_STATUS_OK) {
        printf("FAILED: eif_neural_init returned %d\n", status);
        return 1;
    }
    printf("Runtime Initialized. Arena Size: %zu bytes\n", ctx.arena.activation_size);

    // 6. Set Input Data
    // Input: 4x4x2. Let's set all to 1.0.
    float input_data[4*4*2];
    for(int i=0; i<32; i++) input_data[i] = 2.0f;
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));

    // 7. Run Inference
    status = eif_neural_invoke(&ctx);
    if (status != EIF_STATUS_OK) {
        printf("FAILED: eif_neural_invoke returned %d\n", status);
        return 1;
    }

    // 8. Check Output
    float output_data[2];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));

    printf("Output: [%f, %f]\n", output_data[0], output_data[1]);

    // Verification Logic:
    // Input: 2.0 everywhere.
    // Depthwise Conv (3x3 kernel of 1.0s):
    // Each output pixel sums 3x3=9 input pixels.
    // Value = 9 * (2.0 * 1.0) = 18.0.
    // ReLU6: Clamps 18.0 to 6.0.
    // GlobalAvgPool: Average of 2x2 (4 pixels) of 6.0s.
    // Result should be 6.0.

    if (fabs(output_data[0] - 6.0f) < 0.001f && fabs(output_data[1] - 6.0f) < 0.001f) {
        printf("SUCCESS: Output matches expected value (6.0).\n");
    } else {
        printf("FAILURE: Expected 6.0, got [%f, %f]\n", output_data[0], output_data[1]);
        return 1;
    }

    free(pool_buffer);
    return 0;
}
