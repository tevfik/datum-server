#include "eif_neural.h"
#include "eif_test_runner.h"
#include <string.h>

static uint8_t pool_buffer[1024 * 1024]; // 1MB
static eif_memory_pool_t pool;

void setup_neural_full() {
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
}

// Helper to create a simple 1-node model
static void create_single_node_model(eif_model_t* model, eif_layer_type_t type, 
                                     int* in_dims, int* out_dims, 
                                     void* params, 
                                     float32_t* input_data, float32_t* output_data,
                                     float32_t* weights_data, float32_t* bias_data) {
    
    static eif_tensor_t tensors[4];
    static eif_layer_node_t node;
    static int in_indices[3];
    static int out_indices[1];
    
    // Reset static buffers to avoid stale data
    memset(tensors, 0, sizeof(tensors));
    memset(&node, 0, sizeof(node));
    memset(in_indices, 0, sizeof(in_indices));
    memset(out_indices, 0, sizeof(out_indices));
    
    // 0: Input
    tensors[0].type = EIF_TENSOR_FLOAT32;
    memcpy(tensors[0].dims, in_dims, 4*sizeof(int));
    tensors[0].size_bytes = in_dims[0]*in_dims[1]*in_dims[2]*in_dims[3]*sizeof(float32_t);
    tensors[0].is_variable = true;
    tensors[0].data = input_data; // Initial data (will be copied to arena)
    
    // 1: Output
    tensors[1].type = EIF_TENSOR_FLOAT32;
    memcpy(tensors[1].dims, out_dims, 4*sizeof(int));
    tensors[1].size_bytes = out_dims[0]*out_dims[1]*out_dims[2]*out_dims[3]*sizeof(float32_t);
    tensors[1].is_variable = true;
    
    int num_inputs = 1;
    in_indices[0] = 0;
    
    // Weights
    if (weights_data) {
        tensors[2].type = EIF_TENSOR_FLOAT32;
        // Dims need to be set based on layer type, but for now just dummy or passed
        tensors[2].dims[0] = out_dims[3]; tensors[2].dims[1] = 3; tensors[2].dims[2] = 3; tensors[2].dims[3] = in_dims[3]; // Example Conv
        tensors[2].size_bytes = 10000; // Large buffer for test output
        tensors[2].is_variable = false;
        tensors[2].data = weights_data;
        in_indices[num_inputs++] = 2;
    }
    
    // Bias
    if (bias_data) {
        tensors[3].type = EIF_TENSOR_FLOAT32;
        tensors[3].dims[0] = out_dims[3];
        tensors[3].size_bytes = out_dims[3]*sizeof(float32_t);
        tensors[3].is_variable = false;
        tensors[3].data = bias_data;
        in_indices[num_inputs++] = 3;
    }
    
    out_indices[0] = 1;
    
    node.type = type;
    node.num_inputs = num_inputs;
    node.input_indices = in_indices;
    node.num_outputs = 1;
    node.output_indices = out_indices;
    node.params = params;
    
    model->nodes = &node;
    model->num_nodes = 1;
    model->tensors = tensors;
    model->num_tensors = 2 + (weights_data?1:0) + (bias_data?1:0);
    
    static int model_in[] = {0};
    static int model_out[] = {1};
    model->input_tensor_indices = model_in;
    model->num_inputs = 1;
    model->output_tensor_indices = model_out;
    model->num_outputs = 1;
    
    // Set arena sizes
    model->activation_arena_size = 64 * 1024; // 64KB should be enough for tests
    model->scratch_size = 16 * 1024;          // 16KB scratch
    model->persistent_size = 0;               // No RNN state needed
}

typedef struct {
    eif_quant_param_t quant;
    eif_layer_param_t layer;
} full_params_t;

bool test_conv2d_dispatch() {
    setup_neural_full();
    eif_model_t model;
    
    int in_dims[] = {1, 4, 4, 1};
    int out_dims[] = {1, 2, 2, 1}; // Valid padding, stride 2?
    
    float32_t input[16];
    for(int i=0; i<16; i++) input[i] = 1.0f;
    
    float32_t weights[9] = {1,1,1, 1,1,1, 1,1,1}; // 3x3 kernel of 1s
    float32_t bias[1] = {0};
    
    full_params_t params;
    memset(&params, 0, sizeof(params));
    params.layer.conv2d.filters = 1;
    params.layer.conv2d.kernel_h = 3;
    params.layer.conv2d.kernel_w = 3;
    params.layer.conv2d.stride_h = 1;
    params.layer.conv2d.stride_w = 1;
    params.layer.conv2d.pad_h = 0;
    params.layer.conv2d.pad_w = 0;
    
    // Output size: (4-3)/1 + 1 = 2. So 2x2 output.
    
    create_single_node_model(&model, EIF_LAYER_CONV2D, in_dims, out_dims, &params, input, NULL, weights, bias);
    
    eif_neural_context_t ctx;
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, &pool));
    
    eif_neural_set_input(&ctx, 0, input, sizeof(input));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float32_t output[4];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    // 3x3 sum of 1s = 9.
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 9.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 9.0f, output[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 9.0f, output[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 9.0f, output[3]);
    
    return true;
}

bool test_maxpool_dispatch() {
    setup_neural_full();
    eif_model_t model;
    
    int in_dims[] = {1, 4, 4, 1};
    int out_dims[] = {1, 2, 2, 1};
    
    float32_t input[16];
    for(int i=0; i<16; i++) input[i] = (float)i;
    
    full_params_t params;
    memset(&params, 0, sizeof(params));
    params.layer.maxpool2d.pool_h = 2;
    params.layer.maxpool2d.pool_w = 2;
    params.layer.maxpool2d.stride_h = 2;
    params.layer.maxpool2d.stride_w = 2;
    
    create_single_node_model(&model, EIF_LAYER_MAXPOOL2D, in_dims, out_dims, &params, input, NULL, NULL, NULL);
    
    eif_neural_context_t ctx;
    eif_neural_init(&ctx, &model, &pool);
    eif_neural_set_input(&ctx, 0, input, sizeof(input));
    eif_neural_invoke(&ctx);
    
    float32_t output[4];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    // Input:
    // 0  1  2  3
    // 4  5  6  7
    // 8  9 10 11
    // 12 13 14 15
    
    // MaxPool 2x2 stride 2:
    // [0,1,4,5] -> 5
    // [2,3,6,7] -> 7
    // [8,9,12,13] -> 13
    // [10,11,14,15] -> 15
    
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 5.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 7.0f, output[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 13.0f, output[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 15.0f, output[3]);
    
    return true;
}

bool test_concat_dispatch() {
    setup_neural_full();
    eif_model_t model;
    
    // Concat 2 tensors of shape [1, 1, 1, 2] along axis 3 -> [1, 1, 1, 4]
    int dims[] = {1, 1, 1, 2};
    int out_dims[] = {1, 1, 1, 4};
    
    float32_t in1[] = {1.0f, 2.0f};
    float32_t in2[] = {3.0f, 4.0f};
    
    static eif_tensor_t tensors[3];
    static eif_layer_node_t node;
    static int in_indices[2];
    static int out_indices[1];
    
    memset(tensors, 0, sizeof(tensors));
    memset(&node, 0, sizeof(node));
    
    // Tensor 0: Input 1
    tensors[0].type = EIF_TENSOR_FLOAT32;
    memcpy(tensors[0].dims, dims, 4*sizeof(int));
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 2*sizeof(float32_t);
    tensors[0].is_variable = true;
    tensors[0].data = in1;
    
    // Tensor 1: Input 2
    tensors[1].type = EIF_TENSOR_FLOAT32;
    memcpy(tensors[1].dims, dims, 4*sizeof(int));
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 2*sizeof(float32_t);
    tensors[1].is_variable = true;
    tensors[1].data = in2;
    
    // Tensor 2: Output
    tensors[2].type = EIF_TENSOR_FLOAT32;
    memcpy(tensors[2].dims, out_dims, 4*sizeof(int));
    tensors[2].num_dims = 4;
    tensors[2].size_bytes = 4*sizeof(float32_t);
    tensors[2].is_variable = true;
    
    in_indices[0] = 0;
    in_indices[1] = 1;
    out_indices[0] = 2;
    
    static full_params_t params;
    memset(&params, 0, sizeof(params));
    params.layer.concat.axis = 3;
    
    node.type = EIF_LAYER_CONCAT;
    node.num_inputs = 2;
    node.input_indices = in_indices;
    node.num_outputs = 1;
    node.output_indices = out_indices;
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    model.tensors = tensors;
    model.num_tensors = 3;
    
    static int model_in[] = {0, 1};
    static int model_out[] = {2};
    model.input_tensor_indices = model_in;
    model.num_inputs = 2;
    model.output_tensor_indices = model_out;
    model.num_outputs = 1;
    
    model.activation_arena_size = 1024;
    model.scratch_size = 1024;
    model.persistent_size = 0;
    
    eif_neural_context_t ctx;
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, &pool));
    
    eif_neural_set_input(&ctx, 0, in1, sizeof(in1));
    eif_neural_set_input(&ctx, 1, in2, sizeof(in2));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float32_t output[4];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    TEST_ASSERT_EQUAL_FLOAT(1.0f, output[0], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, output[1], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, output[2], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, output[3], 1e-5f);
    
    return true;
}

BEGIN_TEST_SUITE(run_neural_full_tests)
    RUN_TEST(test_conv2d_dispatch);
    RUN_TEST(test_maxpool_dispatch);
    RUN_TEST(test_concat_dispatch);
END_TEST_SUITE()
