#include "eif_neural.h"
#include "eif_test_runner.h"
#include <string.h>
#include <stdlib.h>

static uint8_t pool_buffer[1024 * 1024]; // 1MB
static eif_memory_pool_t pool;

void setup_neural_core() {
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
}

// Helper to create a model with a specific layer type
static bool run_layer_test(eif_layer_type_t type, void* params, int in_size, int out_size) {
    eif_model_t model;
    memset(&model, 0, sizeof(model));
    
    static eif_tensor_t tensors[2];
    static eif_layer_node_t node;
    static int in_indices[1] = {0};
    static int out_indices[1] = {1};
    
    memset(tensors, 0, sizeof(tensors));
    memset(&node, 0, sizeof(node));
    
    // Input Tensor
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0] = 1; tensors[0].dims[1] = in_size;
    tensors[0].num_dims = 2;
    tensors[0].size_bytes = in_size * sizeof(float32_t);
    tensors[0].is_variable = true;
    
    // Output Tensor
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0] = 1; tensors[1].dims[1] = out_size;
    tensors[1].num_dims = 2;
    tensors[1].size_bytes = out_size * sizeof(float32_t);
    tensors[1].is_variable = true;
    
    node.type = type;
    node.num_inputs = 1;
    node.input_indices = in_indices;
    node.num_outputs = 1;
    node.output_indices = out_indices;
    node.params = params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    model.tensors = tensors;
    model.num_tensors = 2;
    
    static int model_in[] = {0};
    static int model_out[] = {1};
    model.input_tensor_indices = model_in;
    model.num_inputs = 1;
    model.output_tensor_indices = model_out;
    model.num_outputs = 1;
    
    model.activation_arena_size = 4096;
    model.scratch_size = 4096;
    
    eif_neural_context_t ctx;
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, &pool));
    
    float32_t* input_data = (float32_t*)malloc(in_size * sizeof(float32_t));
    for(int i=0; i<in_size; i++) input_data[i] = 0.5f; // Dummy data
    
    eif_neural_set_input(&ctx, 0, input_data, in_size * sizeof(float32_t));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float32_t* output_data = (float32_t*)malloc(out_size * sizeof(float32_t));
    eif_neural_get_output(&ctx, 0, output_data, out_size * sizeof(float32_t));
    
    free(input_data);
    free(output_data);
    return true;
}

bool test_activations_dispatch() {
    setup_neural_core();
    TEST_ASSERT_TRUE(run_layer_test(EIF_LAYER_RELU, NULL, 10, 10));
    TEST_ASSERT_TRUE(run_layer_test(EIF_LAYER_RELU6, NULL, 10, 10));
    TEST_ASSERT_TRUE(run_layer_test(EIF_LAYER_SIGMOID, NULL, 10, 10));
    TEST_ASSERT_TRUE(run_layer_test(EIF_LAYER_TANH, NULL, 10, 10));
    TEST_ASSERT_TRUE(run_layer_test(EIF_LAYER_SOFTMAX, NULL, 10, 10));
    return true;
}

bool test_pooling_dispatch() {
    setup_neural_core();
    
    eif_layer_param_t params;
    memset(&params, 0, sizeof(params));
    params.maxpool2d.pool_h = 1; params.maxpool2d.pool_w = 1;
    params.maxpool2d.stride_h = 1; params.maxpool2d.stride_w = 1;
    
    TEST_ASSERT_TRUE(run_layer_test(EIF_LAYER_MAXPOOL2D, &params, 10, 10));
    
    memset(&params, 0, sizeof(params));
    params.avgpool2d.pool_h = 1; params.avgpool2d.pool_w = 1;
    params.avgpool2d.stride_h = 1; params.avgpool2d.stride_w = 1;
    
    TEST_ASSERT_TRUE(run_layer_test(EIF_LAYER_AVGPOOL2D, &params, 10, 10));
    return true;
}

bool test_reshape_dispatch() {
    setup_neural_core();
    eif_layer_param_t params;
    memset(&params, 0, sizeof(params));
    params.reshape.target_shape[0] = 1;
    params.reshape.target_shape[1] = 10;
    
    TEST_ASSERT_TRUE(run_layer_test(EIF_LAYER_RESHAPE, &params, 10, 10));
    return true;
}

BEGIN_TEST_SUITE(run_neural_core_tests)
    RUN_TEST(test_activations_dispatch);
    RUN_TEST(test_pooling_dispatch);
    RUN_TEST(test_reshape_dispatch);
END_TEST_SUITE()
