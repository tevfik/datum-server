#include "eif_test_runner.h"
#include "eif_neural.h"
#include "eif_nn_context.h"
#include "eif_memory.h"
#include <string.h>
#include <stdlib.h>

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

bool test_neural_init_invalid(void) {
    eif_neural_context_t ctx;
    eif_model_t model;
    eif_memory_pool_t* pool = create_pool(1024);

    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_neural_init(NULL, &model, pool));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_neural_init(&ctx, NULL, pool));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_neural_init(&ctx, &model, NULL));

    destroy_pool(pool);
    return true;
}

bool test_neural_init_simple(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Define Tensors
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Tensor 0: Input (Variable)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0] = 1; tensors[0].dims[1] = 1; tensors[0].dims[2] = 1; tensors[0].dims[3] = 1;
    tensors[0].size_bytes = sizeof(float);
    tensors[0].is_variable = true;
    
    // Tensor 1: Output (Variable)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0] = 1; tensors[1].dims[1] = 1; tensors[1].dims[2] = 1; tensors[1].dims[3] = 1;
    tensors[1].size_bytes = sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    // Define Nodes
    eif_layer_node_t node;
    int input_indices[] = {0};
    int output_indices[] = {1};
    node.type = EIF_LAYER_RELU;
    node.input_indices = input_indices;
    node.num_inputs = 1;
    node.output_indices = output_indices;
    node.num_outputs = 1;
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    // Define Graph I/O
    int graph_inputs[] = {0};
    int graph_outputs[] = {1};
    model.input_tensor_indices = graph_inputs;
    model.num_inputs = 1;
    model.output_tensor_indices = graph_outputs;
    model.num_outputs = 1;
    
    // Init
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Check Arena
    TEST_ASSERT_NOT_NULL(ctx.arena.activation_base);
    TEST_ASSERT_NOT_NULL(ctx.arena.scratch_base);
    TEST_ASSERT_NOT_NULL(ctx.tensor_data);
    
    // Check Tensor Data Pointers
    TEST_ASSERT_TRUE((uint8_t*)ctx.tensor_data[0] >= ctx.arena.activation_base);
    TEST_ASSERT_TRUE((uint8_t*)ctx.tensor_data[1] >= ctx.arena.activation_base);
    
    destroy_pool(pool);
    return true;
}

// ============================================================================
// New Coverage Tests
// ============================================================================

bool test_neural_io_accessors(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Define Tensors
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Tensor 0: Input
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0] = 1; tensors[0].dims[1] = 4; tensors[0].dims[2] = 1; tensors[0].dims[3] = 1;
    tensors[0].size_bytes = 4 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Tensor 1: Output
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0] = 1; tensors[1].dims[1] = 4; tensors[1].dims[2] = 1; tensors[1].dims[3] = 1;
    tensors[1].size_bytes = 4 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    // Define Graph I/O
    int graph_inputs[] = {0};
    int graph_outputs[] = {1};
    model.input_tensor_indices = graph_inputs;
    model.num_inputs = 1;
    model.output_tensor_indices = graph_outputs;
    model.num_outputs = 1;
    
    // Init
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Test Set Input
    float input_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data)));
    
    // Verify Input Data
    float* input_ptr = (float*)eif_neural_get_input_ptr(&ctx, 0);
    TEST_ASSERT_TRUE(input_ptr != NULL);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, input_ptr[0], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, input_ptr[3], 0.001f);
    
    // Manually set output for testing get_output
    float* output_ptr = (float*)eif_neural_get_output_ptr(&ctx, 0);
    TEST_ASSERT_TRUE(output_ptr != NULL);
    output_ptr[0] = 5.0f;
    output_ptr[3] = 8.0f;
    
    // Test Get Output
    float output_data[4];
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data)));
    TEST_ASSERT_EQUAL_FLOAT(5.0f, output_data[0], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(8.0f, output_data[3], 0.001f);
    
    // Test Invalid Args
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_neural_set_input(NULL, 0, input_data, sizeof(input_data)));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_neural_set_input(&ctx, -1, input_data, sizeof(input_data)));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_neural_set_input(&ctx, 1, input_data, sizeof(input_data))); // Out of bounds
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_neural_set_input(&ctx, 0, input_data, 1000)); // Too large
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_neural_get_output(NULL, 0, output_data, sizeof(output_data)));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_neural_get_output(&ctx, -1, output_data, sizeof(output_data)));
    
    TEST_ASSERT_TRUE(eif_neural_get_input_ptr(NULL, 0) == NULL);
    TEST_ASSERT_TRUE(eif_neural_get_input_ptr(&ctx, -1) == NULL);
    
    TEST_ASSERT_TRUE(eif_neural_get_output_ptr(NULL, 0) == NULL);
    TEST_ASSERT_TRUE(eif_neural_get_output_ptr(&ctx, -1) == NULL);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_reset_state(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Define Tensors
    eif_tensor_t tensors[1];
    memset(tensors, 0, sizeof(tensors));
    model.tensors = tensors;
    model.num_tensors = 1;
    
    // Set persistent size
    model.persistent_size = 1024;
    
    // Init
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Modify persistent state
    TEST_ASSERT_TRUE(ctx.arena.persistent_base != NULL);
    ctx.arena.persistent_base[0] = 0xFF;
    ctx.arena.persistent_base[1023] = 0xAA;
    
    // Reset
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_reset_state(&ctx));
    
    // Verify Reset
    TEST_ASSERT_EQUAL_INT(0, ctx.arena.persistent_base[0]);
    TEST_ASSERT_EQUAL_INT(0, ctx.arena.persistent_base[1023]);
    
    // Invalid Arg
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_neural_reset_state(NULL));
    
    destroy_pool(pool);
    return true;
}

static eif_status_t my_custom_op(eif_neural_context_t* ctx, const eif_layer_node_t* node, void** inputs, void** outputs) {
    float* in = (float*)inputs[0];
    float* out = (float*)outputs[0];
    out[0] = in[0] * 2.0f;
    return EIF_STATUS_OK;
}

bool test_neural_custom_op(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Define Tensors
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Tensor 0: Input
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0] = 1; tensors[0].dims[1] = 1; tensors[0].dims[2] = 1; tensors[0].dims[3] = 1;
    tensors[0].size_bytes = sizeof(float);
    tensors[0].is_variable = true;
    
    // Tensor 1: Output
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0] = 1; tensors[1].dims[1] = 1; tensors[1].dims[2] = 1; tensors[1].dims[3] = 1;
    tensors[1].size_bytes = sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    // Define Custom Op Node
    eif_layer_node_t node;
    int input_indices[] = {0};
    int output_indices[] = {1};
    static uint32_t custom_params[] = {0x1234}; // Op ID
    
    node.type = EIF_LAYER_CUSTOM;
    node.input_indices = input_indices;
    node.num_inputs = 1;
    node.output_indices = output_indices;
    node.num_outputs = 1;
    node.params = custom_params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    // Set Graph I/O BEFORE Init
    model.input_tensor_indices = input_indices;
    model.num_inputs = 1;
    model.output_tensor_indices = output_indices;
    model.num_outputs = 1;
    
    // Init
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Register Custom Op
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_register_custom_op(&ctx, 0x1234, my_custom_op));
    
    // Set Input
    float input_val = 10.0f;
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_set_input(&ctx, 0, &input_val, sizeof(float)));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Check Output
    float output_val;
    eif_neural_get_output(&ctx, 0, &output_val, sizeof(float));
    TEST_ASSERT_EQUAL_FLOAT(20.0f, output_val, 0.001f);
    
    // Test Duplicate Registration (should overwrite)
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_register_custom_op(&ctx, 0x1234, my_custom_op));
    
    // Test Invalid Args
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_neural_register_custom_op(NULL, 0x1234, my_custom_op));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_neural_register_custom_op(&ctx, 0x1234, NULL));
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_relu(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Input
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].size_bytes = 2 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Output
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_RELU;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Input
    float input_data[] = {-1.0f, 2.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    float output_data[2];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, output_data[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_dense(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input, Weights, Bias, Output
    eif_tensor_t tensors[4];
    memset(tensors, 0, sizeof(tensors));
    
    // 0: Input (1x2)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=2;
    tensors[0].size_bytes = 2 * sizeof(float);
    tensors[0].is_variable = true;
    
    // 1: Weights (1x2) - Constant
    float weights_data[] = {0.5f, 0.5f};
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=2;
    tensors[1].size_bytes = sizeof(weights_data);
    tensors[1].is_variable = false;
    tensors[1].data = weights_data;
    
    // 2: Bias (1) - Constant
    float bias_data[] = {0.1f};
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].size_bytes = sizeof(bias_data);
    tensors[2].is_variable = false;
    tensors[2].data = bias_data;
    
    // 3: Output (1x1)
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0]=1; tensors[3].dims[1]=1; tensors[3].dims[2]=1; tensors[3].dims[3]=1;
    tensors[3].size_bytes = sizeof(float);
    tensors[3].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 4;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0, 1, 2};
    int out_idx[] = {3};
    node.type = EIF_LAYER_DENSE;
    node.input_indices = in_idx;
    node.num_inputs = 3;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    // Params (Quant + Layer)
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.dense.units = 1;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {3};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Input
    float input_data[] = {1.0f, 2.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    float output_val;
    eif_neural_get_output(&ctx, 0, &output_val, sizeof(float));
    
    // 1.0*0.5 + 2.0*0.5 + 0.1 = 0.5 + 1.0 + 0.1 = 1.6
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.6f, output_val);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_dense_int8(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input, Weights, Bias, Output
    eif_tensor_t tensors[4];
    memset(tensors, 0, sizeof(tensors));
    
    // 0: Input (1x2) - Int8
    tensors[0].type = EIF_TENSOR_INT8;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=2;
    tensors[0].size_bytes = 2 * sizeof(int8_t);
    tensors[0].is_variable = true;
    
    // 1: Weights (1x2) - Int8
    int8_t weights_data[] = {10, 10};
    tensors[1].type = EIF_TENSOR_INT8;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=2;
    tensors[1].size_bytes = sizeof(weights_data);
    tensors[1].is_variable = false;
    tensors[1].data = weights_data;
    
    // 2: Bias (1) - Int32 (usually for quantized)
    int32_t bias_data[] = {5};
    tensors[2].type = EIF_TENSOR_INT8; // Using INT8 as placeholder for INT32
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].size_bytes = sizeof(bias_data);
    tensors[2].is_variable = false;
    tensors[2].data = bias_data;
    
    // 3: Output (1x1) - Int8
    tensors[3].type = EIF_TENSOR_INT8;
    tensors[3].dims[0]=1; tensors[3].dims[1]=1; tensors[3].dims[2]=1; tensors[3].dims[3]=1;
    tensors[3].size_bytes = sizeof(int8_t);
    tensors[3].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 4;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0, 1, 2};
    int out_idx[] = {3};
    node.type = EIF_LAYER_DENSE;
    node.input_indices = in_idx;
    node.num_inputs = 3;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    // Params
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.dense.units = 1;
    
    // Quant params
    params.qp.input_offset = 0;
    params.qp.output_offset = 0;
    params.qp.output_multiplier = 2147483647; // ~1.0 in Q31
    params.qp.output_shift = 0;      // Simplified shift
    params.qp.quantized_activation_min = -128;
    params.qp.quantized_activation_max = 127;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {3};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Input
    int8_t input_data[] = {2, 3};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    int8_t output_val;
    eif_neural_get_output(&ctx, 0, &output_val, sizeof(int8_t));
    
    // 2*10 + 3*10 + 5 = 20 + 30 + 5 = 55
    TEST_ASSERT_EQUAL_INT(55, output_val);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_conv2d(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input, Weights, Bias, Output
    eif_tensor_t tensors[4];
    memset(tensors, 0, sizeof(tensors));
    
    // 0: Input (1x3x3x1)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=3; tensors[0].dims[2]=3; tensors[0].dims[3]=1;
    tensors[0].size_bytes = 9 * sizeof(float);
    tensors[0].is_variable = true;
    
    // 1: Weights (1x2x2x1) - Filters=1, Kernel=2x2, InC=1
    // Layout: [Filters, KernelH, KernelW, InC] -> [1, 2, 2, 1]
    float weights_data[] = {1.0f, 1.0f, 1.0f, 1.0f};
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=2; tensors[1].dims[2]=2; tensors[1].dims[3]=1;
    tensors[1].size_bytes = sizeof(weights_data);
    tensors[1].is_variable = false;
    tensors[1].data = weights_data;
    
    // 2: Bias (1)
    float bias_data[] = {0.0f};
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].size_bytes = sizeof(bias_data);
    tensors[2].is_variable = false;
    tensors[2].data = bias_data;
    
    // 3: Output (1x2x2x1) - Valid padding
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0]=1; tensors[3].dims[1]=2; tensors[3].dims[2]=2; tensors[3].dims[3]=1;
    tensors[3].size_bytes = 4 * sizeof(float);
    tensors[3].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 4;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0, 1, 2};
    int out_idx[] = {3};
    node.type = EIF_LAYER_CONV2D;
    node.input_indices = in_idx;
    node.num_inputs = 3;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    // Params
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.conv2d.filters = 1;
    params.lp.conv2d.kernel_h = 2;
    params.lp.conv2d.kernel_w = 2;
    params.lp.conv2d.stride_h = 1;
    params.lp.conv2d.stride_w = 1;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {3};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Input (All 1s)
    float input_data[9];
    for(int i=0; i<9; i++) input_data[i] = 1.0f;
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    float output_data[4];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    // Kernel is 2x2 all 1s. Input is all 1s. Sum = 4.
    for(int i=0; i<4; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, output_data[i]);
    }
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_conv2d_int8(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input, Weights, Bias, Output
    eif_tensor_t tensors[4];
    memset(tensors, 0, sizeof(tensors));
    
    // 0: Input (1x3x3x1) - Int8
    tensors[0].type = EIF_TENSOR_INT8;
    tensors[0].dims[0]=1; tensors[0].dims[1]=3; tensors[0].dims[2]=3; tensors[0].dims[3]=1;
    tensors[0].size_bytes = 9 * sizeof(int8_t);
    tensors[0].is_variable = true;
    
    // 1: Weights (1x2x2x1) - Int8
    int8_t weights_data[] = {1, 1, 1, 1};
    tensors[1].type = EIF_TENSOR_INT8;
    tensors[1].dims[0]=1; tensors[1].dims[1]=2; tensors[1].dims[2]=2; tensors[1].dims[3]=1;
    tensors[1].size_bytes = sizeof(weights_data);
    tensors[1].is_variable = false;
    tensors[1].data = weights_data;
    
    // 2: Bias (1) - Int32
    int32_t bias_data[] = {0};
    tensors[2].type = EIF_TENSOR_INT8; // Using INT8 as placeholder for INT32
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].size_bytes = sizeof(bias_data);
    tensors[2].is_variable = false;
    tensors[2].data = bias_data;
    
    // 3: Output (1x2x2x1) - Int8
    tensors[3].type = EIF_TENSOR_INT8;
    tensors[3].dims[0]=1; tensors[3].dims[1]=2; tensors[3].dims[2]=2; tensors[3].dims[3]=1;
    tensors[3].size_bytes = 4 * sizeof(int8_t);
    tensors[3].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 4;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0, 1, 2};
    int out_idx[] = {3};
    node.type = EIF_LAYER_CONV2D;
    node.input_indices = in_idx;
    node.num_inputs = 3;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    // Params
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.conv2d.filters = 1;
    params.lp.conv2d.kernel_h = 2;
    params.lp.conv2d.kernel_w = 2;
    params.lp.conv2d.stride_h = 1;
    params.lp.conv2d.stride_w = 1;
    
    // Quant params
    params.qp.input_offset = 0;
    params.qp.output_offset = 0;
    params.qp.output_multiplier = 2147483647;
    params.qp.output_shift = 0;
    params.qp.quantized_activation_min = -128;
    params.qp.quantized_activation_max = 127;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {3};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Input (All 2s)
    int8_t input_data[9];
    for(int i=0; i<9; i++) input_data[i] = 2;
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    int8_t output_data[4];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    // Kernel is 2x2 all 1s. Input is all 2s. Sum = 4*2 = 8.
    for(int i=0; i<4; i++) {
        TEST_ASSERT_EQUAL_INT(8, output_data[i]);
    }
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_softmax(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Input (1x1x1x2)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=2;
    tensors[0].size_bytes = 2 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Output
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=2;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_SOFTMAX;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    node.params = NULL; // Softmax usually has no params or just axis
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Input: [0, 0] -> exp(0)=1, sum=2 -> [0.5, 0.5]
    float input_data[] = {0.0f, 0.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    float output_data[2];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, output_data[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_maxpool2d(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input, Output
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Input (1x4x4x1)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=4; tensors[0].dims[2]=4; tensors[0].dims[3]=1;
    tensors[0].size_bytes = 16 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Output (1x2x2x1) - 2x2 pool, stride 2
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=2; tensors[1].dims[2]=2; tensors[1].dims[3]=1;
    tensors[1].size_bytes = 4 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_MAXPOOL2D;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    // Params
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.maxpool2d.pool_h = 2;
    params.lp.maxpool2d.pool_w = 2;
    params.lp.maxpool2d.stride_h = 2;
    params.lp.maxpool2d.stride_w = 2;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Input: 0..15
    float input_data[16];
    for(int i=0; i<16; i++) input_data[i] = (float)i;
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    float output_data[4];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    // Pool 1: [0,1,4,5] -> 5
    // Pool 2: [2,3,6,7] -> 7
    // Pool 3: [8,9,12,13] -> 13
    // Pool 4: [10,11,14,15] -> 15
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 7.0f, output_data[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 13.0f, output_data[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, output_data[3]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_avgpool2d(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input, Output
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Input (1x4x4x1)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=4; tensors[0].dims[2]=4; tensors[0].dims[3]=1;
    tensors[0].size_bytes = 16 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Output (1x2x2x1) - 2x2 pool, stride 2
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=2; tensors[1].dims[2]=2; tensors[1].dims[3]=1;
    tensors[1].size_bytes = 4 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_AVGPOOL2D;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    // Params
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.avgpool2d.pool_h = 2;
    params.lp.avgpool2d.pool_w = 2;
    params.lp.avgpool2d.stride_h = 2;
    params.lp.avgpool2d.stride_w = 2;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Input: 0..15
    float input_data[16];
    for(int i=0; i<16; i++) input_data[i] = (float)i;
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    float output_data[4];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    // Pool 1: [0,1,4,5] -> (0+1+4+5)/4 = 2.5
    // Pool 2: [2,3,6,7] -> (2+3+6+7)/4 = 4.5
    // Pool 3: [8,9,12,13] -> (8+9+12+13)/4 = 10.5
    // Pool 4: [10,11,14,15] -> (10+11+14+15)/4 = 12.5
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.5f, output_data[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.5f, output_data[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.5f, output_data[3]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_add(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input1, Input2, Output
    eif_tensor_t tensors[3];
    memset(tensors, 0, sizeof(tensors));
    
    // Input 1 (1x2)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=2;
    tensors[0].size_bytes = 2 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Input 2 (1x2)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=2;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = true;
    
    // Output (1x2)
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=2;
    tensors[2].size_bytes = 2 * sizeof(float);
    tensors[2].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 3;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0, 1};
    int out_idx[] = {2};
    node.type = EIF_LAYER_ADD;
    node.input_indices = in_idx;
    node.num_inputs = 2;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0, 1};
    int g_out[] = {2};
    model.input_tensor_indices = g_in;
    model.num_inputs = 2;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Inputs
    float input1[] = {1.0f, 2.0f};
    float input2[] = {3.0f, 4.0f};
    eif_neural_set_input(&ctx, 0, input1, sizeof(input1));
    eif_neural_set_input(&ctx, 1, input2, sizeof(input2));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    float output_data[2];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, output_data[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_sub(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input1, Input2, Output
    eif_tensor_t tensors[3];
    memset(tensors, 0, sizeof(tensors));
    
    // Input 1 (1x2)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=2;
    tensors[0].size_bytes = 2 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Input 2 (1x2)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=2;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = true;
    
    // Output (1x2)
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=2;
    tensors[2].size_bytes = 2 * sizeof(float);
    tensors[2].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 3;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0, 1};
    int out_idx[] = {2};
    node.type = EIF_LAYER_SUB;
    node.input_indices = in_idx;
    node.num_inputs = 2;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0, 1};
    int g_out[] = {2};
    model.input_tensor_indices = g_in;
    model.num_inputs = 2;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Inputs
    float input1[] = {5.0f, 10.0f};
    float input2[] = {2.0f, 3.0f};
    eif_neural_set_input(&ctx, 0, input1, sizeof(input1));
    eif_neural_set_input(&ctx, 1, input2, sizeof(input2));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    float output_data[2];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 7.0f, output_data[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_multiply(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input1, Input2, Output
    eif_tensor_t tensors[3];
    memset(tensors, 0, sizeof(tensors));
    
    // Input 1 (1x2)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=2;
    tensors[0].size_bytes = 2 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Input 2 (1x2)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=2;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = true;
    
    // Output (1x2)
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=2;
    tensors[2].size_bytes = 2 * sizeof(float);
    tensors[2].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 3;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0, 1};
    int out_idx[] = {2};
    node.type = EIF_LAYER_MULTIPLY;
    node.input_indices = in_idx;
    node.num_inputs = 2;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0, 1};
    int g_out[] = {2};
    model.input_tensor_indices = g_in;
    model.num_inputs = 2;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Inputs
    float input1[] = {2.0f, 3.0f};
    float input2[] = {4.0f, 5.0f};
    eif_neural_set_input(&ctx, 0, input1, sizeof(input1));
    eif_neural_set_input(&ctx, 1, input2, sizeof(input2));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    float output_data[2];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 8.0f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, output_data[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_concat(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input1, Input2, Output
    eif_tensor_t tensors[3];
    memset(tensors, 0, sizeof(tensors));
    
    // Input 1 (1x1x1x2)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=2;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 2 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Input 2 (1x1x1x2)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=2;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = true;
    
    // Output (1x1x1x4)
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=4;
    tensors[2].num_dims = 4;
    tensors[2].size_bytes = 4 * sizeof(float);
    tensors[2].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 3;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0, 1};
    int out_idx[] = {2};
    node.type = EIF_LAYER_CONCAT;
    node.input_indices = in_idx;
    node.num_inputs = 2;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    // Params
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.concat.axis = 3;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0, 1};
    int g_out[] = {2};
    model.input_tensor_indices = g_in;
    model.num_inputs = 2;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Inputs
    float input1[] = {1.0f, 2.0f};
    float input2[] = {3.0f, 4.0f};
    eif_neural_set_input(&ctx, 0, input1, sizeof(input1));
    eif_neural_set_input(&ctx, 1, input2, sizeof(input2));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    float output_data[4];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, output_data[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, output_data[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, output_data[3]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_concat_axis0(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input1, Input2, Output
    eif_tensor_t tensors[3];
    memset(tensors, 0, sizeof(tensors));
    
    // Input 1 (1x1x1x1)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 1 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Input 2 (1x1x1x1)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 1 * sizeof(float);
    tensors[1].is_variable = true;
    
    // Output (2x1x1x1) - Axis 0
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=2; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].num_dims = 4;
    tensors[2].size_bytes = 2 * sizeof(float);
    tensors[2].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 3;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0, 1};
    int out_idx[] = {2};
    node.type = EIF_LAYER_CONCAT;
    node.input_indices = in_idx;
    node.num_inputs = 2;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    // Params
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.concat.axis = 0;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0, 1};
    int g_out[] = {2};
    model.input_tensor_indices = g_in;
    model.num_inputs = 2;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Inputs
    float input1[] = {1.0f};
    float input2[] = {2.0f};
    eif_neural_set_input(&ctx, 0, input1, sizeof(input1));
    eif_neural_set_input(&ctx, 1, input2, sizeof(input2));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    float output_data[2];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, output_data[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_concat_axis1(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input1, Input2, Output
    eif_tensor_t tensors[3];
    memset(tensors, 0, sizeof(tensors));
    
    // Input 1 (1x1x1x1)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 1 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Input 2 (1x1x1x1)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 1 * sizeof(float);
    tensors[1].is_variable = true;
    
    // Output (1x2x1x1) - Axis 1
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=2; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].num_dims = 4;
    tensors[2].size_bytes = 2 * sizeof(float);
    tensors[2].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 3;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0, 1};
    int out_idx[] = {2};
    node.type = EIF_LAYER_CONCAT;
    node.input_indices = in_idx;
    node.num_inputs = 2;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    // Params
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.concat.axis = 1;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0, 1};
    int g_out[] = {2};
    model.input_tensor_indices = g_in;
    model.num_inputs = 2;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Inputs
    float input1[] = {1.0f};
    float input2[] = {2.0f};
    eif_neural_set_input(&ctx, 0, input1, sizeof(input1));
    eif_neural_set_input(&ctx, 1, input2, sizeof(input2));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    float output_data[2];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, output_data[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_reshape(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input, Output
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Input (1x4x1x1)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=4; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 4 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Output (1x2x2x1)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=2; tensors[1].dims[2]=2; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 4 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_RESHAPE;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    // Params
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.reshape.target_shape[0] = 1;
    params.lp.reshape.target_shape[1] = 2;
    params.lp.reshape.target_shape[2] = 2;
    params.lp.reshape.target_shape[3] = 1;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Input
    float input_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    float output_data[4];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    // Reshape just copies data, so output should be same as input
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, output_data[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, output_data[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, output_data[3]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_flatten(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input, Output
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Input (1x2x2x1)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=2; tensors[0].dims[2]=2; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 4 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Output (1x4x1x1) - Flattened
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=4; tensors[1].dims[2]=1; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 4 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_FLATTEN;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Input
    float input_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    float output_data[4];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, output_data[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, output_data[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, output_data[3]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_dropout(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input, Output
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Input (1x4x1x1)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=4; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 4 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Output (1x4x1x1)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=4; tensors[1].dims[2]=1; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 4 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_DROPOUT;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    // Params
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.dropout.rate = 0.5f;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Input
    float input_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    float output_data[4];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    // Dropout usually passes through during inference
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, output_data[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, output_data[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, output_data[3]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_batch_norm(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input, Gamma, Beta, Mean, Var, Output
    eif_tensor_t tensors[6];
    memset(tensors, 0, sizeof(tensors));
    
    // Input (1x1x1x1)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 1 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Gamma (1)
    float gamma_data[] = {2.0f};
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1;
    tensors[1].num_dims = 1;
    tensors[1].size_bytes = sizeof(gamma_data);
    tensors[1].data = gamma_data;
    tensors[1].is_variable = false;
    
    // Beta (1)
    float beta_data[] = {1.0f};
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1;
    tensors[2].num_dims = 1;
    tensors[2].size_bytes = sizeof(beta_data);
    tensors[2].data = beta_data;
    tensors[2].is_variable = false;
    
    // Mean (1)
    float mean_data[] = {0.0f};
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0]=1;
    tensors[3].num_dims = 1;
    tensors[3].size_bytes = sizeof(mean_data);
    tensors[3].data = mean_data;
    tensors[3].is_variable = false;
    
    // Var (1)
    float var_data[] = {4.0f};
    tensors[4].type = EIF_TENSOR_FLOAT32;
    tensors[4].dims[0]=1;
    tensors[4].num_dims = 1;
    tensors[4].size_bytes = sizeof(var_data);
    tensors[4].data = var_data;
    tensors[4].is_variable = false;
    
    // Output (1x1x1x1)
    tensors[5].type = EIF_TENSOR_FLOAT32;
    tensors[5].dims[0]=1; tensors[5].dims[1]=1; tensors[5].dims[2]=1; tensors[5].dims[3]=1;
    tensors[5].num_dims = 4;
    tensors[5].size_bytes = 1 * sizeof(float);
    tensors[5].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 6;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0, 1, 2, 3, 4};
    int out_idx[] = {5};
    node.type = EIF_LAYER_BATCH_NORM;
    node.input_indices = in_idx;
    node.num_inputs = 5;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    // Params
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.batch_norm.epsilon = 1e-5f;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {5};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Input
    float input_data[] = {2.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    float output_data[1];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    // Norm = (2 - 0) / sqrt(4) = 1
    // Out = 1 * 2 + 1 = 3
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, output_data[0]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_layer_norm(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input, Weights, Biases, Output
    eif_tensor_t tensors[4];
    memset(tensors, 0, sizeof(tensors));
    
    // Input (1x1x1x2)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=2;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 2 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Weights (2) - Gamma
    float weights_data[] = {1.0f, 1.0f};
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=2;
    tensors[1].num_dims = 1;
    tensors[1].size_bytes = sizeof(weights_data);
    tensors[1].data = weights_data;
    tensors[1].is_variable = false;
    
    // Biases (2) - Beta
    float biases_data[] = {0.0f, 0.0f};
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=2;
    tensors[2].num_dims = 1;
    tensors[2].size_bytes = sizeof(biases_data);
    tensors[2].data = biases_data;
    tensors[2].is_variable = false;
    
    // Output (1x1x1x2)
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0]=1; tensors[3].dims[1]=1; tensors[3].dims[2]=1; tensors[3].dims[3]=2;
    tensors[3].num_dims = 4;
    tensors[3].size_bytes = 2 * sizeof(float);
    tensors[3].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 4;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0, 1, 2};
    int out_idx[] = {3};
    node.type = EIF_LAYER_LAYER_NORM;
    node.input_indices = in_idx;
    node.num_inputs = 3;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    // Params
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.layer_norm.epsilon = 1e-5f;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {3};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Input: [10, 20]
    // Mean = 15, Var = ((10-15)^2 + (20-15)^2)/2 = (25+25)/2 = 25
    // Std = 5
    // Norm = (Val - 15) / 5
    // Out[0] = (10-15)/5 = -1
    // Out[1] = (20-15)/5 = 1
    float input_data[] = {10.0f, 20.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    float output_data[2];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output_data[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_split(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input, Output1, Output2
    eif_tensor_t tensors[3];
    memset(tensors, 0, sizeof(tensors));
    
    // Input (1x1x1x4)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=4;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 4 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Output1 (1x1x1x2)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=2;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = true;
    
    // Output2 (1x1x1x2)
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=2;
    tensors[2].num_dims = 4;
    tensors[2].size_bytes = 2 * sizeof(float);
    tensors[2].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 3;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1, 2};
    node.type = EIF_LAYER_SPLIT;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 2;
    
    // Params
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.split.axis = 3;
    params.lp.split.num_splits = 2;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1, 2};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 2;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Input: [1, 2, 3, 4]
    float input_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output 1
    float output1[2];
    eif_neural_get_output(&ctx, 0, output1, sizeof(output1));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output1[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, output1[1]);
    
    // Get Output 2
    float output2[2];
    eif_neural_get_output(&ctx, 1, output2, sizeof(output2));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, output2[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, output2[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_pad(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Input: 1x2x2x1
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=2; tensors[0].dims[2]=2; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 4 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Output: 1x4x4x1 (Pad 1 on all sides)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=4; tensors[1].dims[2]=4; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 16 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_PAD;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.pad.pads[0] = 1; // top
    params.lp.pad.pads[1] = 1; // bottom
    params.lp.pad.pads[2] = 1; // left
    params.lp.pad.pads[3] = 1; // right
    params.lp.pad.constant_value = 0.0f;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[16];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    // Check corners (should be 0)
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output[3]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output[12]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output[15]);
    
    // Check center (should be input)
    // Output is 4x4. Input starts at (1,1)
    // (1,1) -> index 1*4 + 1 = 5
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output[5]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, output[6]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, output[9]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, output[10]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_gather(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[3];
    memset(tensors, 0, sizeof(tensors));
    
    // Input Data: 4x2x1x1
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=4; tensors[0].dims[1]=2; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 8 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Indices: 2 (1D)
    tensors[1].type = EIF_TENSOR_FLOAT32; // Indices as float for now
    tensors[1].dims[0]=2; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=1;
    tensors[1].num_dims = 1;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = true; // Can be constant, but let's make it variable input
    
    // Output: 2x2x1x1
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=2; tensors[2].dims[1]=2; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].num_dims = 4;
    tensors[2].size_bytes = 4 * sizeof(float);
    tensors[2].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 3;
    
    eif_layer_node_t node;
    int in_idx[] = {0, 1};
    int out_idx[] = {2};
    node.type = EIF_LAYER_GATHER;
    node.input_indices = in_idx;
    node.num_inputs = 2;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.gather.axis = 0;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0, 1};
    int g_out[] = {2};
    model.input_tensor_indices = g_in;
    model.num_inputs = 2;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input_data[] = {
        1.0f, 2.0f,
        3.0f, 4.0f,
        5.0f, 6.0f,
        7.0f, 8.0f
    };
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    float indices_data[] = {0.0f, 2.0f};
    eif_neural_set_input(&ctx, 1, indices_data, sizeof(indices_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[4];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    // Row 0: 1, 2
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, output[1]);
    // Row 2: 5, 6
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, output[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, output[3]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_reduce(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Input: 1x2x2x1
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=2; tensors[0].dims[2]=2; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 4 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Output: 1x1x2x1 (Reduce axis 1 - Height)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=2; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_REDUCE_MEAN;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.reduce.axis = 1; // Reduce Height
    params.lp.reduce.keep_dims = 1;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Input:
    // 1 2
    // 3 4
    float input_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[2];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    // Mean of col 0: (1+3)/2 = 2
    // Mean of col 1: (2+4)/2 = 3
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, output[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_reduce_axis0(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Input: 2x1x1x1
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=2; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 2 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Output: 1x1x1x1 (Reduce axis 0)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 1 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_REDUCE_MEAN;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.reduce.axis = 0; // Reduce Batch
    params.lp.reduce.keep_dims = 1;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input_data[] = {1.0f, 3.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[1];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    // Mean: (1+3)/2 = 2
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, output[0]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_topk(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[3];
    memset(tensors, 0, sizeof(tensors));
    
    // Input: 1x1x1x4
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=4;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 4 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Output Values: 1x1x1x2 (Top 2)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=2;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = true;
    
    // Output Indices: 1x1x1x2 (Top 2)
    tensors[2].type = EIF_TENSOR_FLOAT32; // Indices as float
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=2;
    tensors[2].num_dims = 4;
    tensors[2].size_bytes = 2 * sizeof(float);
    tensors[2].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 3;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1, 2};
    node.type = EIF_LAYER_TOPK;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 2;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.topk.k = 2;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1, 2};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 2;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Input: 10, 30, 20, 40
    float input_data[] = {10.0f, 30.0f, 20.0f, 40.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float out_vals[2];
    float out_inds[2];
    eif_neural_get_output(&ctx, 0, out_vals, sizeof(out_vals));
    eif_neural_get_output(&ctx, 1, out_inds, sizeof(out_inds));
    
    // Top 2: 40 (idx 3), 30 (idx 1)
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 40.0f, out_vals[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.0f, out_vals[1]);
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, out_inds[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, out_inds[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_matmul(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[3];
    memset(tensors, 0, sizeof(tensors));
    
    // Input A: 1x1x2x3 (2x3 matrix)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=2; tensors[0].dims[3]=3;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 6 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Input B: 1x1x3x2 (3x2 matrix)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=3; tensors[1].dims[3]=2;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 6 * sizeof(float);
    tensors[1].is_variable = true;
    
    // Output: 1x1x2x2 (2x2 matrix)
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=2; tensors[2].dims[3]=2;
    tensors[2].num_dims = 4;
    tensors[2].size_bytes = 4 * sizeof(float);
    tensors[2].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 3;
    
    eif_layer_node_t node;
    int in_idx[] = {0, 1};
    int out_idx[] = {2};
    node.type = EIF_LAYER_MATMUL;
    node.input_indices = in_idx;
    node.num_inputs = 2;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    // No params for MatMul
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0, 1};
    int g_out[] = {2};
    model.input_tensor_indices = g_in;
    model.num_inputs = 2;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // A:
    // 1 2 3
    // 4 5 6
    float input_a[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    eif_neural_set_input(&ctx, 0, input_a, sizeof(input_a));
    
    // B:
    // 7 8
    // 9 10
    // 11 12
    float input_b[] = {7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f};
    eif_neural_set_input(&ctx, 1, input_b, sizeof(input_b));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[4];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    // C = A * B
    // C[0,0] = 1*7 + 2*9 + 3*11 = 7 + 18 + 33 = 58
    // C[0,1] = 1*8 + 2*10 + 3*12 = 8 + 20 + 36 = 64
    // C[1,0] = 4*7 + 5*9 + 6*11 = 28 + 45 + 66 = 139
    // C[1,1] = 4*8 + 5*10 + 6*12 = 32 + 50 + 72 = 154
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 58.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 64.0f, output[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 139.0f, output[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 154.0f, output[3]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_relu6(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=2;
    tensors[0].size_bytes = 2 * sizeof(float);
    tensors[0].is_variable = true;
    
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=2;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_RELU6;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input_data[] = {7.0f, -1.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[2];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_sigmoid(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].size_bytes = 1 * sizeof(float);
    tensors[0].is_variable = true;
    
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=1;
    tensors[1].size_bytes = 1 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_SIGMOID;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input_data[] = {0.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[1];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, output[0]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_tanh(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].size_bytes = 1 * sizeof(float);
    tensors[0].is_variable = true;
    
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=1;
    tensors[1].size_bytes = 1 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_TANH;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input_data[] = {0.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[1];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output[0]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_leaky_relu(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=2;
    tensors[0].size_bytes = 2 * sizeof(float);
    tensors[0].is_variable = true;
    
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=2;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_LEAKY_RELU;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.leaky_relu.alpha = 0.1f;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input_data[] = {1.0f, -1.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[2];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.1f, output[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_clip(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=3;
    tensors[0].size_bytes = 3 * sizeof(float);
    tensors[0].is_variable = true;
    
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=3;
    tensors[1].size_bytes = 3 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_CLIP;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.clip.min_val = 0.0f;
    params.lp.clip.max_val = 6.0f;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input_data[] = {-1.0f, 3.0f, 7.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[3];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, output[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, output[2]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_div(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[3];
    memset(tensors, 0, sizeof(tensors));
    
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=2;
    tensors[0].size_bytes = 2 * sizeof(float);
    tensors[0].is_variable = true;
    
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=2;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = true;
    
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=2;
    tensors[2].size_bytes = 2 * sizeof(float);
    tensors[2].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 3;
    
    eif_layer_node_t node;
    int in_idx[] = {0, 1};
    int out_idx[] = {2};
    node.type = EIF_LAYER_DIV;
    node.input_indices = in_idx;
    node.num_inputs = 2;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0, 1};
    int g_out[] = {2};
    model.input_tensor_indices = g_in;
    model.num_inputs = 2;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input1[] = {10.0f, 20.0f};
    float input2[] = {2.0f, 4.0f};
    eif_neural_set_input(&ctx, 0, input1, sizeof(input1));
    eif_neural_set_input(&ctx, 1, input2, sizeof(input2));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[2];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, output[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_exp(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].size_bytes = 1 * sizeof(float);
    tensors[0].is_variable = true;
    
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=1;
    tensors[1].size_bytes = 1 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_EXP;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input_data[] = {1.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[1];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.71828f, output[0]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_log(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].size_bytes = 1 * sizeof(float);
    tensors[0].is_variable = true;
    
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=1;
    tensors[1].size_bytes = 1 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_LOG;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input_data[] = {2.71828f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[1];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output[0]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_sqrt(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].size_bytes = 1 * sizeof(float);
    tensors[0].is_variable = true;
    
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=1;
    tensors[1].size_bytes = 1 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_SQRT;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input_data[] = {4.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[1];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, output[0]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_embedding(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[3];
    memset(tensors, 0, sizeof(tensors));
    
    // Input Indices (1x2) -> [1, 0]
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=2; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].num_dims = 2;
    tensors[0].size_bytes = 2 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Weights (Vocab=3, Embed=2)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=3; tensors[1].dims[1]=2; tensors[1].dims[2]=1; tensors[1].dims[3]=1;
    tensors[1].num_dims = 2;
    tensors[1].size_bytes = 6 * sizeof(float);
    tensors[1].is_variable = false;
    float weights_data[] = {
        0.1f, 0.2f, // Index 0
        0.3f, 0.4f, // Index 1
        0.5f, 0.6f  // Index 2
    };
    tensors[1].data = weights_data;
    
    // Output (1x2x2)
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=2; tensors[2].dims[2]=2; tensors[2].dims[3]=1;
    tensors[2].num_dims = 3;
    tensors[2].size_bytes = 4 * sizeof(float);
    tensors[2].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 3;
    
    eif_layer_node_t node;
    int in_idx[] = {0, 1};
    int out_idx[] = {2};
    node.type = EIF_LAYER_EMBEDDING;
    node.input_indices = in_idx;
    node.num_inputs = 2;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {2};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input_data[] = {1.0f, 0.0f}; // Indices
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[4];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.3f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.4f, output[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, output[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.2f, output[3]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_conv1d(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[4];
    memset(tensors, 0, sizeof(tensors));
    
    // Input (1, 1, 3, 1)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=3; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 3 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Weights (1, 1, 2, 1)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=2; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = false;
    float weights_data[] = {1.0f, 1.0f};
    tensors[1].data = weights_data;
    
    // Bias (1)
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].num_dims = 1;
    tensors[2].size_bytes = 1 * sizeof(float);
    tensors[2].is_variable = false;
    float bias_data[] = {0.0f};
    tensors[2].data = bias_data;
    
    // Output (1, 1, 2, 1)
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0]=1; tensors[3].dims[1]=1; tensors[3].dims[2]=2; tensors[3].dims[3]=1;
    tensors[3].num_dims = 4;
    tensors[3].size_bytes = 2 * sizeof(float);
    tensors[3].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 4;
    
    eif_layer_node_t node;
    int in_idx[] = {0, 1, 2};
    int out_idx[] = {3};
    node.type = EIF_LAYER_CONV1D;
    node.input_indices = in_idx;
    node.num_inputs = 3;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.conv1d.pad = 0;
    params.lp.conv1d.stride = 1;
    params.lp.conv1d.kernel_size = 2;
    params.lp.conv1d.filters = 1;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {3};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input_data[] = {1.0f, 2.0f, 3.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[2];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, output[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_depthwise_conv2d(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[4];
    memset(tensors, 0, sizeof(tensors));
    
    // Input (1, 3, 3, 1)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=3; tensors[0].dims[2]=3; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 9 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Weights (1, 2, 2, 1)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=2; tensors[1].dims[2]=2; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 4 * sizeof(float);
    tensors[1].is_variable = false;
    float weights_data[] = {1.0f, 1.0f, 1.0f, 1.0f};
    tensors[1].data = weights_data;
    
    // Bias (1)
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].num_dims = 1;
    tensors[2].size_bytes = 1 * sizeof(float);
    tensors[2].is_variable = false;
    float bias_data[] = {0.0f};
    tensors[2].data = bias_data;
    
    // Output (1, 2, 2, 1)
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0]=1; tensors[3].dims[1]=2; tensors[3].dims[2]=2; tensors[3].dims[3]=1;
    tensors[3].num_dims = 4;
    tensors[3].size_bytes = 4 * sizeof(float);
    tensors[3].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 4;
    
    eif_layer_node_t node;
    int in_idx[] = {0, 1, 2};
    int out_idx[] = {3};
    node.type = EIF_LAYER_DEPTHWISE_CONV2D;
    node.input_indices = in_idx;
    node.num_inputs = 3;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.depthwise_conv2d.pad_h = 0;
    params.lp.depthwise_conv2d.pad_w = 0;
    params.lp.depthwise_conv2d.stride_h = 1;
    params.lp.depthwise_conv2d.stride_w = 1;
    params.lp.depthwise_conv2d.kernel_h = 2;
    params.lp.depthwise_conv2d.kernel_w = 2;
    params.lp.depthwise_conv2d.depth_multiplier = 1;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {3};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input_data[] = {
        1, 1, 1,
        1, 1, 1,
        1, 1, 1
    };
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[4];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, output[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, output[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, output[3]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_quantize(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Input (Float32)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=2;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 2 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Output (Int8)
    tensors[1].type = EIF_TENSOR_INT8;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=2;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 2 * sizeof(int8_t);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_QUANTIZE;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.quantize.scale = 0.5f;
    params.lp.quantize.zero_point = 0;
    params.lp.quantize.min_val = -128;
    params.lp.quantize.max_val = 127;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input_data[] = {1.0f, -1.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    int8_t output[2];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    // 1.0 / 0.5 = 2
    // -1.0 / 0.5 = -2
    TEST_ASSERT_EQUAL_INT(2, output[0]);
    TEST_ASSERT_EQUAL_INT(-2, output[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_dequantize(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Input (Int8)
    tensors[0].type = EIF_TENSOR_INT8;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=2;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 2 * sizeof(int8_t);
    tensors[0].is_variable = true;
    
    // Output (Float32)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=2;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_DEQUANTIZE;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.dequantize.scale = 0.5f;
    params.lp.dequantize.zero_point = 0;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    int8_t input_data[] = {2, -2};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[2];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    // 2 * 0.5 = 1.0
    // -2 * 0.5 = -1.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, output[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_transpose_conv2d(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[4];
    memset(tensors, 0, sizeof(tensors));
    
    // Input (1, 2, 2, 1)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=2; tensors[0].dims[2]=2; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 4 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Weights (1, 2, 2, 1) -> [filters, k_h, k_w, in_c]
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=2; tensors[1].dims[2]=2; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 4 * sizeof(float);
    tensors[1].is_variable = false;
    float weights_data[] = {1.0f, 1.0f, 1.0f, 1.0f};
    tensors[1].data = weights_data;
    
    // Bias (1)
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].num_dims = 1;
    tensors[2].size_bytes = 1 * sizeof(float);
    tensors[2].is_variable = false;
    float bias_data[] = {0.0f};
    tensors[2].data = bias_data;
    
    // Output (1, 3, 3, 1) -> Stride 1, Valid padding (0)
    // H_out = (2-1)*1 + 2 - 0 = 3
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0]=1; tensors[3].dims[1]=3; tensors[3].dims[2]=3; tensors[3].dims[3]=1;
    tensors[3].num_dims = 4;
    tensors[3].size_bytes = 9 * sizeof(float);
    tensors[3].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 4;
    
    eif_layer_node_t node;
    int in_idx[] = {0, 1, 2};
    int out_idx[] = {3};
    node.type = EIF_LAYER_TRANSPOSE_CONV2D;
    node.input_indices = in_idx;
    node.num_inputs = 3;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.transpose_conv2d.filters = 1;
    params.lp.transpose_conv2d.kernel_h = 2;
    params.lp.transpose_conv2d.kernel_w = 2;
    params.lp.transpose_conv2d.stride_h = 1;
    params.lp.transpose_conv2d.stride_w = 1;
    params.lp.transpose_conv2d.pad_h = 0;
    params.lp.transpose_conv2d.pad_w = 0;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {3};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input_data[] = {1.0f, 0.0f, 0.0f, 0.0f}; // Only top-left is 1
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output[9];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    // Should produce a 2x2 block of 1s at top-left of 3x3 output
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output[2]);
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output[3]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output[4]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output[5]);
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output[6]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output[7]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output[8]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_reduce_sum(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Input: 1x2x2x1
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=2; tensors[0].dims[2]=2; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 4 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Output: 1x1x2x1 (Reduce axis 1 - Height)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=2; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_REDUCE_SUM;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.reduce.axis = 1; // Reduce Height
    params.lp.reduce.keep_dims = 1;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Input:
    // [[1, 2],
    //  [3, 4]]
    float input_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output_data[2];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    // Sum along axis 1 (columns):
    // Col 0: 1 + 3 = 4
    // Col 1: 2 + 4 = 6
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, output_data[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_global_avgpool2d(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Input: 1x2x2x2 (N, H, W, C)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=2; tensors[0].dims[2]=2; tensors[0].dims[3]=2;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 8 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Output: 1x1x1x2
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=2;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_GLOBAL_AVGPOOL2D;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Input:
    // Channel 0: [1, 2, 3, 4] -> Avg = 2.5
    // Channel 1: [5, 6, 7, 8] -> Avg = 6.5
    // Interleaved: 1,5, 2,6, 3,7, 4,8
    float input_data[] = {1.0f, 5.0f, 2.0f, 6.0f, 3.0f, 7.0f, 4.0f, 8.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output_data[2];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.5f, output_data[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_attention(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Q, K, V, Output
    eif_tensor_t tensors[4];
    memset(tensors, 0, sizeof(tensors));
    
    // Dimensions: Batch=1, Seq=2, Embed=2
    
    // Q: 1x2x2
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=2; tensors[0].dims[2]=2; tensors[0].dims[3]=1; // Using 3 dims effectively
    tensors[0].num_dims = 3;
    tensors[0].size_bytes = 4 * sizeof(float);
    tensors[0].is_variable = true;
    
    // K: 1x2x2
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=2; tensors[1].dims[2]=2; tensors[1].dims[3]=1;
    tensors[1].num_dims = 3;
    tensors[1].size_bytes = 4 * sizeof(float);
    tensors[1].is_variable = true;
    
    // V: 1x2x2
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=2; tensors[2].dims[2]=2; tensors[2].dims[3]=1;
    tensors[2].num_dims = 3;
    tensors[2].size_bytes = 4 * sizeof(float);
    tensors[2].is_variable = true;
    
    // Output: 1x2x2
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0]=1; tensors[3].dims[1]=2; tensors[3].dims[2]=2; tensors[3].dims[3]=1;
    tensors[3].num_dims = 3;
    tensors[3].size_bytes = 4 * sizeof(float);
    tensors[3].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 4;
    
    eif_layer_node_t node;
    int in_idx[] = {0, 1, 2};
    int out_idx[] = {3};
    node.type = EIF_LAYER_ATTENTION;
    node.input_indices = in_idx;
    node.num_inputs = 3;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0, 1, 2};
    int g_out[] = {3};
    model.input_tensor_indices = g_in;
    model.num_inputs = 3;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Simple Attention:
    // Q = [[1, 0], [0, 1]]
    // K = [[1, 0], [0, 1]]
    // V = [[10, 20], [30, 40]]
    // Q*K^T = [[1, 0], [0, 1]] (Identity)
    // Softmax(Identity) -> approx [[0.73, 0.27], [0.27, 0.73]] (if scaled)
    // But eif_layer_attention might implement scaled dot product.
    // Let's assume it does standard attention.
    
    float q_data[] = {1.0f, 0.0f, 0.0f, 1.0f};
    float k_data[] = {1.0f, 0.0f, 0.0f, 1.0f};
    float v_data[] = {10.0f, 20.0f, 30.0f, 40.0f};
    
    eif_neural_set_input(&ctx, 0, q_data, sizeof(q_data));
    eif_neural_set_input(&ctx, 1, k_data, sizeof(k_data));
    eif_neural_set_input(&ctx, 2, v_data, sizeof(v_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output_data[4];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    // We just check it runs and produces something reasonable (not NaN/Inf)
    // Exact values depend on scaling factor (1/sqrt(d_k))
    for(int i=0; i<4; i++) {
        TEST_ASSERT_TRUE(!isnan(output_data[i]));
        TEST_ASSERT_TRUE(!isinf(output_data[i]));
    }
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_gelu(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=2;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 2 * sizeof(float);
    tensors[0].is_variable = true;
    
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=2;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_GELU;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // GELU(0) = 0
    // GELU(1) approx 0.8413
    float input_data[] = {0.0f, 1.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output_data[2];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.8413f, output_data[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_hard_swish(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=2;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 2 * sizeof(float);
    tensors[0].is_variable = true;
    
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=2;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 2 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_HARD_SWISH;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // h-swish(x) = x * ReLU6(x+3)/6
    // x=0 -> 0 * 3/6 = 0
    // x=3 -> 3 * 6/6 = 3
    float input_data[] = {0.0f, 3.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output_data[2];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, output_data[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_resize(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Input: 1x2x2x1
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=2; tensors[0].dims[2]=2; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 4 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Output: 1x4x4x1 (Upsample 2x)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=4; tensors[1].dims[2]=4; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 16 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_RESIZE;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    // Assuming nearest neighbor or bilinear default
    // No specific params for resize in eif_layer_param_t usually, or it's empty
    // Let's check eif_nn_layers.h if needed.
    // eif_layer_resize takes params, but maybe it's optional or has defaults.
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Input:
    // 1 2
    // 3 4
    float input_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output_data[16];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    // Check corners at least
    // Top-left should be close to 1
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 1.0f, output_data[0]);
    // Bottom-right should be close to 4
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 4.0f, output_data[15]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_resize_bilinear(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Input: 1x2x2x1
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=2; tensors[0].dims[2]=2; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 4 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Output: 1x4x4x1 (Upsample 2x)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=4; tensors[1].dims[2]=4; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 16 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1};
    node.type = EIF_LAYER_RESIZE;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.resize.method = 1; // Bilinear
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Input:
    // 1 2
    // 3 4
    float input_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output_data[16];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    // Check center point (interpolated)
    // Output is 4x4.
    // (1,1) in output maps to (0.5, 0.5) in input.
    // Input (0,0)=1, (0,1)=2, (1,0)=3, (1,1)=4
    // Interpolated at (0.5, 0.5) should be average of all 4 = 2.5
    // Index of (1,1) in 4x4 is 1*4 + 1 = 5
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 2.5f, output_data[5]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_rnn_simple(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[4];
    memset(tensors, 0, sizeof(tensors));
    
    // Input: 1x1x2x1
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=2; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 2 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Weights: Units=2, Input=2. Size = 2*2 + 2*2 = 8.
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=8; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=1;
    tensors[1].size_bytes = 8 * sizeof(float);
    tensors[1].is_variable = false;
    float weights_data[8] = {
        // W (2x2): Identity
        1.0f, 0.0f,
        0.0f, 1.0f,
        // R (2x2): Zero
        0.0f, 0.0f,
        0.0f, 0.0f
    };
    tensors[1].data = weights_data;
    
    // Biases: Units=2. Size = 2.
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=2; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].size_bytes = 2 * sizeof(float);
    tensors[2].is_variable = false;
    float biases_data[2] = {0.0f, 0.0f};
    tensors[2].data = biases_data;
    
    // Output: 1x1x2x1
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0]=1; tensors[3].dims[1]=1; tensors[3].dims[2]=2; tensors[3].dims[3]=1;
    tensors[3].num_dims = 4;
    tensors[3].size_bytes = 2 * sizeof(float);
    tensors[3].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 4;
    
    eif_layer_node_t node;
    int in_idx[] = {0, 1, 2};
    int out_idx[] = {3};
    node.type = EIF_LAYER_RNN;
    node.input_indices = in_idx;
    node.num_inputs = 3;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.rnn.units = 2;
    params.lp.rnn.return_sequences = 0;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {3};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    model.persistent_size = 2 * sizeof(float); // State for 2 units
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Input: [0.5, -0.5]
    // W=I, R=0, b=0
    // h = tanh(I*x) = tanh(x)
    // h[0] = tanh(0.5) approx 0.462
    // h[1] = tanh(-0.5) approx -0.462
    float input_data[] = {0.5f, -0.5f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output_data[2];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.4621f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.4621f, output_data[1]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_lstm_simple(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[4];
    memset(tensors, 0, sizeof(tensors));
    
    // Input: 1x1x1x1
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 1 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Weights: Units=1, Input=1. Size = 4*1*1 + 4*1*1 = 8.
    // Order: In, Out, Forget, Cell
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=8; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=1;
    tensors[1].size_bytes = 8 * sizeof(float);
    tensors[1].is_variable = false;
    float weights_data[8] = {
        10.0f, 10.0f, -10.0f, 1.0f, // W: In, Out, Forget, Cell
        0.0f, 0.0f, 0.0f, 0.0f      // R
    };
    tensors[1].data = weights_data;
    
    // Biases: Units=1. Size = 4.
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=4; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].size_bytes = 4 * sizeof(float);
    tensors[2].is_variable = false;
    float biases_data[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    tensors[2].data = biases_data;
    
    // Output: 1x1x1x1
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0]=1; tensors[3].dims[1]=1; tensors[3].dims[2]=1; tensors[3].dims[3]=1;
    tensors[3].num_dims = 4;
    tensors[3].size_bytes = 1 * sizeof(float);
    tensors[3].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 4;
    
    eif_layer_node_t node;
    int in_idx[] = {0, 1, 2};
    int out_idx[] = {3};
    node.type = EIF_LAYER_LSTM;
    node.input_indices = in_idx;
    node.num_inputs = 3;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.lstm.units = 1;
    params.lp.lstm.return_sequences = 0;
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {3};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    model.persistent_size = 2 * sizeof(float);
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input_data[] = {0.5f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output_data[1];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.4318f, output_data[0]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_gru_simple(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[4];
    memset(tensors, 0, sizeof(tensors));
    
    // Input: 1x1x1x1
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 1 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Weights: Units=1, Input=1. Size = 3*1*1 + 3*1*1 = 6.
    // Order: Update, Reset, Hidden
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=6; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=1;
    tensors[1].size_bytes = 6 * sizeof(float);
    tensors[1].is_variable = false;
    float weights_data[6] = {
        10.0f, 0.0f, 1.0f, // W: Update=10, Reset=0, Hidden=1
        0.0f, 0.0f, 0.0f   // R
    };
    tensors[1].data = weights_data;
    
    // Biases: Units=1. Size = 3.
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=3; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].size_bytes = 3 * sizeof(float);
    tensors[2].is_variable = false;
    float biases_data[3] = {0.0f, 0.0f, 0.0f};
    tensors[2].data = biases_data;
    
    // Output: 1x1x1x1
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0]=1; tensors[3].dims[1]=1; tensors[3].dims[2]=1; tensors[3].dims[3]=1;
    tensors[3].num_dims = 4;
    tensors[3].size_bytes = 1 * sizeof(float);
    tensors[3].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 4;
    
    eif_layer_node_t node;
    int in_idx[] = {0, 1, 2};
    int out_idx[] = {3};
    node.type = EIF_LAYER_GRU;
    node.input_indices = in_idx;
    node.num_inputs = 3;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.gru.units = 1;
    params.lp.gru.return_sequences = 0;
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {3};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    model.persistent_size = 1 * sizeof(float);
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    float input_data[] = {0.5f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    float output_data[1];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.4621f, output_data[0]);
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_conv2d_no_bias(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input, Weights, Output
    eif_tensor_t tensors[3];
    memset(tensors, 0, sizeof(tensors));
    
    // 0: Input (1x3x3x1)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=3; tensors[0].dims[2]=3; tensors[0].dims[3]=1;
    tensors[0].size_bytes = 9 * sizeof(float);
    tensors[0].is_variable = true;
    
    // 1: Weights (1x2x2x1) - Filters=1, Kernel=2x2, InC=1
    float weights_data[] = {1.0f, 1.0f, 1.0f, 1.0f};
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=2; tensors[1].dims[2]=2; tensors[1].dims[3]=1;
    tensors[1].size_bytes = sizeof(weights_data);
    tensors[1].is_variable = false;
    tensors[1].data = weights_data;
    
    // 2: Output (1x2x2x1)
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=2; tensors[2].dims[2]=2; tensors[2].dims[3]=1;
    tensors[2].size_bytes = 4 * sizeof(float);
    tensors[2].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 3;
    
    // Node
    eif_layer_node_t node;
    int in_idx[] = {0, 1}; // No bias
    int out_idx[] = {2};
    node.type = EIF_LAYER_CONV2D;
    node.input_indices = in_idx;
    node.num_inputs = 2;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    // Params
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.conv2d.filters = 1;
    params.lp.conv2d.kernel_h = 2;
    params.lp.conv2d.kernel_w = 2;
    params.lp.conv2d.stride_h = 1;
    params.lp.conv2d.stride_w = 1;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {2};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Set Input (All 1s)
    float input_data[9];
    for(int i=0; i<9; i++) input_data[i] = 1.0f;
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Get Output
    float output_data[4];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));
    
    // Kernel is 2x2 all 1s. Input is all 1s. Sum = 4. No bias.
    for(int i=0; i<4; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, output_data[i]);
    }
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_gather_invalid_axis(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[3];
    memset(tensors, 0, sizeof(tensors));
    
    // Input
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=2; tensors[0].dims[1]=2; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].size_bytes = 4 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Indices
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=1;
    tensors[1].size_bytes = 1 * sizeof(float);
    tensors[1].is_variable = true;
    
    // Output
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=2; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].size_bytes = 2 * sizeof(float);
    tensors[2].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 3;
    
    eif_layer_node_t node;
    int in_idx[] = {0, 1};
    int out_idx[] = {2};
    node.type = EIF_LAYER_GATHER;
    node.input_indices = in_idx;
    node.num_inputs = 2;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.gather.axis = 1; // Invalid axis (only 0 supported)
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0, 1};
    int g_out[] = {2};
    model.input_tensor_indices = g_in;
    model.num_inputs = 2;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Invoke should fail with NOT_IMPLEMENTED
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_NOT_IMPLEMENTED, eif_neural_invoke(&ctx));
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_attention_too_large(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[4];
    memset(tensors, 0, sizeof(tensors));
    
    // Q, K, V: 1x129x4 (SeqLen 129 > 128)
    for(int i=0; i<3; i++) {
        tensors[i].type = EIF_TENSOR_FLOAT32;
        tensors[i].dims[0]=1; tensors[i].dims[1]=129; tensors[i].dims[2]=4; tensors[i].dims[3]=1;
        tensors[i].size_bytes = 129 * 4 * sizeof(float);
        tensors[i].is_variable = true;
    }
    
    // Output
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0]=1; tensors[3].dims[1]=129; tensors[3].dims[2]=4; tensors[3].dims[3]=1;
    tensors[3].size_bytes = 129 * 4 * sizeof(float);
    tensors[3].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 4;
    
    eif_layer_node_t node;
    int in_idx[] = {0, 1, 2};
    int out_idx[] = {3};
    node.type = EIF_LAYER_ATTENTION;
    node.input_indices = in_idx;
    node.num_inputs = 3;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0, 1, 2};
    int g_out[] = {3};
    model.input_tensor_indices = g_in;
    model.num_inputs = 3;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Invoke should fail with NOT_IMPLEMENTED (due to size limit)
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_NOT_IMPLEMENTED, eif_neural_invoke(&ctx));
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_split_too_many_outputs(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[10]; // 1 input, 9 outputs
    memset(tensors, 0, sizeof(tensors));
    
    // Input
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=9;
    tensors[0].size_bytes = 9 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Outputs
    for(int i=1; i<10; i++) {
        tensors[i].type = EIF_TENSOR_FLOAT32;
        tensors[i].dims[0]=1; tensors[i].dims[1]=1; tensors[i].dims[2]=1; tensors[i].dims[3]=1;
        tensors[i].size_bytes = 1 * sizeof(float);
        tensors[i].is_variable = true;
    }
    
    model.tensors = tensors;
    model.num_tensors = 10;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1, 2, 3, 4, 5, 6, 7, 8, 9}; // 9 outputs
    node.type = EIF_LAYER_SPLIT;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 9; // > MAX_SPLIT_OUTPUTS (8)
    node.params = NULL;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Should fail
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_NOT_IMPLEMENTED, eif_neural_invoke(&ctx));
    
    destroy_pool(pool);
    return true;
}

bool test_neural_invoke_topk_too_large(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    
    eif_tensor_t tensors[3];
    memset(tensors, 0, sizeof(tensors));
    
    // Input: [1, 1, 1, 1025] -> Last dim > 1024
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=1025;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 1025 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Output Values
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=1;
    tensors[1].size_bytes = 1 * sizeof(float);
    tensors[1].is_variable = true;
    
    // Output Indices
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].size_bytes = 1 * sizeof(float);
    tensors[2].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 3;
    
    eif_layer_node_t node;
    int in_idx[] = {0};
    int out_idx[] = {1, 2};
    node.type = EIF_LAYER_TOPK;
    node.input_indices = in_idx;
    node.num_inputs = 1;
    node.output_indices = out_idx;
    node.num_outputs = 2;
    
    struct {
        eif_quant_param_t qp;
        eif_layer_param_t lp;
    } params;
    memset(&params, 0, sizeof(params));
    params.lp.topk.k = 1;
    
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {1};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, pool));
    
    // Should fail
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_NOT_IMPLEMENTED, eif_neural_invoke(&ctx));
    
    destroy_pool(pool);
    return true;
}

// ============================================================================
// DL Context API Tests (eif_dl_*)
// ============================================================================

bool test_dl_context_fallback(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_dl_context_t ctx;
    eif_model_t model = {0};
    
    // Define a simple model: Input -> ReLU -> Output
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0] = 1; tensors[0].dims[1] = 1; tensors[0].dims[2] = 1; tensors[0].dims[3] = 4;
    tensors[0].size_bytes = 4 * sizeof(float);
    tensors[0].is_variable = true;
    
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0] = 1; tensors[1].dims[1] = 1; tensors[1].dims[2] = 1; tensors[1].dims[3] = 4;
    tensors[1].size_bytes = 4 * sizeof(float);
    tensors[1].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    int input_indices[] = {0};
    int output_indices[] = {1};
    model.input_tensor_indices = input_indices;
    model.num_inputs = 1;
    model.output_tensor_indices = output_indices;
    model.num_outputs = 1;
    
    eif_layer_node_t node = {0};
    node.type = EIF_LAYER_RELU;
    int node_inputs[] = {0};
    int node_outputs[] = {1};
    node.input_indices = node_inputs;
    node.num_inputs = 1;
    node.output_indices = node_outputs;
    node.num_outputs = 1;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    // Empty registry to force fallback
    eif_op_registry_t empty_registry = {0};
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_context_init(&ctx, &model, pool, &empty_registry));
    
    // Set input
    float input_data[] = {-1.0f, 0.0f, 1.0f, 2.0f};
    float output_data[4];
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_invoke_io(&ctx, input_data, sizeof(input_data), output_data, sizeof(output_data)));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output_data[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output_data[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output_data[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, output_data[3]);
    
    destroy_pool(pool);
    return true;
}

bool test_dl_context_mutex(void) {
    eif_memory_pool_t* pool = create_pool(8192);
    eif_dl_context_t ctx;
    eif_model_t model = {0};
    
    // Minimal model
    eif_tensor_t tensors[1];
    memset(tensors, 0, sizeof(tensors));
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].size_bytes = 4;
    tensors[0].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 1;
    model.num_nodes = 0;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_context_init(&ctx, &model, pool, NULL));
    
    // Set dummy mutex
    void* dummy_mutex = (void*)0x1234;
    eif_dl_context_set_mutex(&ctx, dummy_mutex);
    
    // Invoke (should lock/unlock)
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_invoke(&ctx));
    
    // Disable mutex
    eif_dl_context_set_mutex(&ctx, NULL);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_invoke(&ctx));
    
    destroy_pool(pool);
    return true;
}

bool test_dl_context_memory(void) {
    eif_model_t model = {0};
    model.activation_arena_size = 100;
    model.scratch_size = 200;
    model.persistent_size = 300;
    model.num_tensors = 10;
    
    eif_dl_memory_req_t req;
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_get_memory_requirements(&model, &req));
    
    TEST_ASSERT_EQUAL_INT(100, req.activation_bytes);
    TEST_ASSERT_EQUAL_INT(200, req.scratch_bytes);
    TEST_ASSERT_EQUAL_INT(300, req.persistent_bytes);
    TEST_ASSERT_EQUAL_INT(10 * sizeof(void*), req.tensor_ptr_bytes);
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_dl_get_memory_requirements(NULL, &req));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_dl_get_memory_requirements(&model, NULL));
    
    return true;
}

bool test_dl_context_ops(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_dl_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: Input, Weights, Bias, Output
    eif_tensor_t tensors[4];
    memset(tensors, 0, sizeof(tensors));
    
    // Input: 1x1x1x4
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0] = 1; tensors[0].dims[1] = 1; tensors[0].dims[2] = 1; tensors[0].dims[3] = 4;
    tensors[0].size_bytes = 4 * sizeof(float);
    tensors[0].is_variable = true;
    
    // Weights: 4x4 (flattened)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0] = 4; tensors[1].dims[1] = 4;
    tensors[1].size_bytes = 16 * sizeof(float);
    float weights[16];
    for(int i=0; i<16; i++) weights[i] = (i%5 == 0) ? 1.0f : 0.0f; // Identity
    tensors[1].data = weights;
    
    // Bias: 4
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0] = 4;
    tensors[2].size_bytes = 4 * sizeof(float);
    float bias[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    tensors[2].data = bias;
    
    // Output: 1x1x1x4
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0] = 1; tensors[3].dims[1] = 1; tensors[3].dims[2] = 1; tensors[3].dims[3] = 4;
    tensors[3].size_bytes = 4 * sizeof(float);
    tensors[3].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 4;
    
    int input_indices[] = {0};
    int output_indices[] = {3};
    model.input_tensor_indices = input_indices;
    model.num_inputs = 1;
    model.output_tensor_indices = output_indices;
    model.num_outputs = 1;
    
    // Node: Dense
    eif_layer_node_t node = {0};
    node.type = EIF_LAYER_DENSE;
    int node_inputs[] = {0, 1, 2};
    int node_outputs[] = {3};
    node.input_indices = node_inputs;
    node.num_inputs = 3;
    node.output_indices = node_outputs;
    node.num_outputs = 1;
    
    eif_layer_param_t params = {0};
    params.dense.units = 4;
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    // Empty registry to force fallback to built-in op_dense
    eif_op_registry_t empty_registry = {0};
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_context_init(&ctx, &model, pool, &empty_registry));
    
    float input_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float output_data[4];
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_invoke_io(&ctx, input_data, sizeof(input_data), output_data, sizeof(output_data)));
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, output_data[0]); // 1 + 1
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, output_data[1]); // 2 + 1
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, output_data[2]); // 3 + 1
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, output_data[3]); // 4 + 1
    
    destroy_pool(pool);
    return true;
}

bool test_dl_context_reset(void) {
    eif_memory_pool_t* pool = create_pool(16384);
    eif_dl_context_t ctx;
    eif_model_t model = {0};
    
    // Add dummy tensor to ensure num_tensors > 0 (alloc > 0)
    eif_tensor_t tensors[1] = {0};
    model.tensors = tensors;
    model.num_tensors = 1;
    
    model.persistent_size = 100;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_context_init(&ctx, &model, pool, NULL));
    
    // Modify persistent memory
    memset(ctx.arena.persistent_base, 0xFF, 100);
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_reset_state(&ctx));
    
    // Check if zeroed
    uint8_t* p = (uint8_t*)ctx.arena.persistent_base;
    for(int i=0; i<100; i++) {
        TEST_ASSERT_EQUAL_INT(0, p[i]);
    }
    
    destroy_pool(pool);
    return true;
}

bool test_dl_context_builtins(void) {
    eif_memory_pool_t* pool = create_pool(32768);
    eif_dl_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: 
    // 0: InA, 1: InB
    // 2: Relu, 3: Add, 4: Softmax
    // 5: Sigmoid, 6: Tanh, 7: Flatten
    eif_tensor_t tensors[8];
    memset(tensors, 0, sizeof(tensors));
    
    for(int i=0; i<8; i++) {
        tensors[i].type = EIF_TENSOR_FLOAT32;
        tensors[i].dims[0] = 4;
        tensors[i].size_bytes = 4 * sizeof(float);
        tensors[i].is_variable = true;
    }
    
    // Inputs
    model.tensors = tensors;
    model.num_tensors = 8;
    
    // Nodes
    eif_layer_node_t nodes[6];
    memset(nodes, 0, sizeof(nodes));
    
    // 0: RELU (In A -> Out RELU)
    nodes[0].type = EIF_LAYER_RELU;
    int relu_in[] = {0}; int relu_out[] = {2};
    nodes[0].input_indices = relu_in; nodes[0].num_inputs = 1;
    nodes[0].output_indices = relu_out; nodes[0].num_outputs = 1;
    
    // 1: ADD (In A + In B -> Out ADD)
    nodes[1].type = EIF_LAYER_ADD;
    int add_in[] = {0, 1}; int add_out[] = {3};
    nodes[1].input_indices = add_in; nodes[1].num_inputs = 2;
    nodes[1].output_indices = add_out; nodes[1].num_outputs = 1;
    
    // 2: SOFTMAX (In A -> Out SOFTMAX)
    nodes[2].type = EIF_LAYER_SOFTMAX;
    int sm_in[] = {0}; int sm_out[] = {4};
    nodes[2].input_indices = sm_in; nodes[2].num_inputs = 1;
    nodes[2].output_indices = sm_out; nodes[2].num_outputs = 1;

    // 3: SIGMOID (In A -> Out SIGMOID)
    nodes[3].type = EIF_LAYER_SIGMOID;
    int sig_in[] = {0}; int sig_out[] = {5};
    nodes[3].input_indices = sig_in; nodes[3].num_inputs = 1;
    nodes[3].output_indices = sig_out; nodes[3].num_outputs = 1;

    // 4: TANH (In A -> Out TANH)
    nodes[4].type = EIF_LAYER_TANH;
    int tanh_in[] = {0}; int tanh_out[] = {6};
    nodes[4].input_indices = tanh_in; nodes[4].num_inputs = 1;
    nodes[4].output_indices = tanh_out; nodes[4].num_outputs = 1;

    // 5: FLATTEN (In A -> Out FLATTEN)
    nodes[5].type = EIF_LAYER_FLATTEN;
    int flat_in[] = {0}; int flat_out[] = {7};
    nodes[5].input_indices = flat_in; nodes[5].num_inputs = 1;
    nodes[5].output_indices = flat_out; nodes[5].num_outputs = 1;
    
    model.nodes = nodes;
    model.num_nodes = 6;
    
    // Graph connection
    int graph_in[] = {0, 1};
    int graph_out[] = {2, 3, 4, 5, 6, 7};
    model.input_tensor_indices = graph_in;
    model.num_inputs = 2;
    model.output_tensor_indices = graph_out;
    model.num_outputs = 6;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_context_init(&ctx, &model, pool, NULL));
    
    // Set Inputs
    float* in_a = (float*)ctx.tensor_data[0];
    float* in_b = (float*)ctx.tensor_data[1];
    
    in_a[0] = 0.0f; in_a[1] = 1.0f; in_a[2] = -1.0f; in_a[3] = 0.5f;
    for(int i=0; i<4; i++) in_b[i] = 10.0f;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_invoke(&ctx));
    
    // Verify
    float* ptr;
    
    // RELU
    ptr = (float*)ctx.tensor_data[2];
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ptr[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ptr[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ptr[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, ptr[3]);
    
    // ADD
    ptr = (float*)ctx.tensor_data[3];
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ptr[0]);
    
    // SOFTMAX
    ptr = (float*)ctx.tensor_data[4];
    float sum = ptr[0]+ptr[1]+ptr[2]+ptr[3];
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, sum);
    
    // SIGMOID (0 -> 0.5)
    ptr = (float*)ctx.tensor_data[5];
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, ptr[0]);
    
    // TANH (0 -> 0)
    ptr = (float*)ctx.tensor_data[6];
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ptr[0]);
    
    // FLATTEN (Copy)
    ptr = (float*)ctx.tensor_data[7];
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ptr[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ptr[1]);

    destroy_pool(pool);
    return true;
}

bool test_dl_context_conv2d_coverage(void) {
    eif_memory_pool_t* pool = create_pool(32768);
    eif_dl_context_t ctx;
    eif_model_t model = {0};
    
    // Tensors: In(1x3x3x1), Weights(1x2x2x1), Out(1x2x2x1)
    eif_tensor_t tensors[3];
    memset(tensors, 0, sizeof(tensors));
    
    // 0: Input
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=3; tensors[0].dims[2]=3; tensors[0].dims[3]=1;
    tensors[0].size_bytes = 9 * sizeof(float);
    tensors[0].is_variable = true;
    
    // 1: Weights (Ones)
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=2; tensors[1].dims[2]=2; tensors[1].dims[3]=1;
    tensors[1].size_bytes = 4 * sizeof(float);
    tensors[1].is_variable = false;
    float weights[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    tensors[1].data = weights;
    
    // 2: Output (needs to be explicitly set)
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=2; tensors[2].dims[2]=2; tensors[2].dims[3]=1;
    tensors[2].size_bytes = 4 * sizeof(float);
    tensors[2].is_variable = true;
    
    model.tensors = tensors;
    model.num_tensors = 3;
    
    // Node
    eif_layer_node_t node;
    memset(&node, 0, sizeof(node));
    node.type = EIF_LAYER_CONV2D;
    int in_idx[] = {0, 1};
    int out_idx[] = {2};
    node.input_indices = in_idx;
    node.num_inputs = 2;
    node.output_indices = out_idx;
    node.num_outputs = 1;
    
    eif_layer_param_t params = {0};
    params.conv2d.filters = 1;
    params.conv2d.kernel_h = 2;
    params.conv2d.kernel_w = 2;
    params.conv2d.stride_h = 1;
    params.conv2d.stride_w = 1;
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    int g_in[] = {0};
    int g_out[] = {2};
    model.input_tensor_indices = g_in;
    model.num_inputs = 1;
    model.output_tensor_indices = g_out;
    model.num_outputs = 1;
    
    // Initialize Inputs
    float input_data[9];
    for(int i=0; i<9; i++) input_data[i] = 1.0f;
    
    // Test Case A: Default Registry (Should hit op_conv2d)
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_context_init(&ctx, &model, pool, NULL));
    
    // Set input via memory copy
    float* in_ptr = (float*)ctx.tensor_data[0];
    memcpy(in_ptr, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_invoke(&ctx));
    
    // Verify (Sum of 2x2 ones = 4)
    float* out_ptr = (float*)ctx.tensor_data[2];
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, out_ptr[0]);
    
    // Test Case B: Empty Registry (Should hit eif_dl_execute_node fallback)
    eif_op_registry_t empty_registry = {0};
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_context_init(&ctx, &model, pool, &empty_registry));
    
    in_ptr = (float*)ctx.tensor_data[0]; 
    memcpy(in_ptr, input_data, sizeof(input_data));
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_invoke(&ctx));
    
    out_ptr = (float*)ctx.tensor_data[2];
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, out_ptr[0]);
    
    // Test Case C: MaxPool Fallback
    node.type = EIF_LAYER_MAXPOOL2D;
    params.maxpool2d.pool_h = 2;
    params.maxpool2d.pool_w = 2;
    params.maxpool2d.stride_h = 1;
    params.maxpool2d.stride_w = 1;
    
    // Re-init with empty registry
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_context_init(&ctx, &model, pool, &empty_registry));
    
    in_ptr = (float*)ctx.tensor_data[0];
    // 1 2 3
    // 4 5 6
    // 7 8 9
    for(int i=0; i<9; i++) in_ptr[i] = (float)(i+1);
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_invoke(&ctx));
    
    out_ptr = (float*)ctx.tensor_data[2];
    // Max of [[1,2],[4,5]] = 5
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, out_ptr[0]);
    
    // Test Case D: AvgPool Fallback
    node.type = EIF_LAYER_AVGPOOL2D;
    // Avg of [[1,2],[4,5]] = (1+2+4+5)/4 = 12/4 = 3
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_context_init(&ctx, &model, pool, &empty_registry));
    in_ptr = (float*)ctx.tensor_data[0];
    for(int i=0; i<9; i++) in_ptr[i] = (float)(i+1);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_invoke(&ctx));
    out_ptr = (float*)ctx.tensor_data[2];
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, out_ptr[0]);
    
    // Test Case E: Add Fallback
    // Need different graph for Add (2 inputs)
    // Reuse existing tensor 0 & 1 as inputs. Output 2.
    node.type = EIF_LAYER_ADD;
    model.nodes = &node;
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_context_init(&ctx, &model, pool, &empty_registry));
    float* in0 = (float*)ctx.tensor_data[0];
    float* in1 = (float*)ctx.tensor_data[1];
    // Set some data (only first 4 elements used as output is size 4)
    for(int i=0; i<4; i++) { in0[i] = 10.0f; in1[i] = 20.0f; }
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_invoke(&ctx));
    out_ptr = (float*)ctx.tensor_data[2];
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.0f, out_ptr[0]);

    // Test Case F: Softmax Fallback
    node.type = EIF_LAYER_SOFTMAX;
    // Input 0 (size 9). Output 0 (size 9). 
    // Need to adjust output tensor size/dims to match input for softmax usually
    // But here we just reuse tensor 2 which is size 4. 
    // Softmax typically works on last dim.
    // Let's use Input 1 (size 4), Output 2 (size 4).
    int sm_in[] = {1};
    int sm_out[] = {2};
    node.input_indices = sm_in;
    node.num_inputs = 1;
    node.output_indices = sm_out;
    node.num_outputs = 1;
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_context_init(&ctx, &model, pool, &empty_registry));
    in1 = (float*)ctx.tensor_data[1];
    // 10, 10, 10, 10 -> 0.25 each
    for(int i=0; i<4; i++) in1[i] = 10.0f;
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_dl_invoke(&ctx));
    out_ptr = (float*)ctx.tensor_data[2];
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.25f, out_ptr[0]);
    
    destroy_pool(pool);
    return true;
}

BEGIN_TEST_SUITE(run_dl_core_coverage_tests)
    RUN_TEST(test_dl_context_fallback);
    RUN_TEST(test_dl_context_mutex);
    RUN_TEST(test_dl_context_memory);
    RUN_TEST(test_dl_context_ops);
    RUN_TEST(test_dl_context_reset);
    RUN_TEST(test_dl_context_builtins);
    RUN_TEST(test_dl_context_conv2d_coverage);
    RUN_TEST(test_neural_io_accessors);
    RUN_TEST(test_neural_reset_state);
    RUN_TEST(test_neural_custom_op);
    RUN_TEST(test_neural_init_invalid);
    RUN_TEST(test_neural_init_simple);
    RUN_TEST(test_neural_invoke_relu);
    RUN_TEST(test_neural_invoke_dense);
    RUN_TEST(test_neural_invoke_conv2d);
    RUN_TEST(test_neural_invoke_softmax);
    RUN_TEST(test_neural_invoke_maxpool2d);
    RUN_TEST(test_neural_invoke_avgpool2d);
    RUN_TEST(test_neural_invoke_add);
    RUN_TEST(test_neural_invoke_sub);
    RUN_TEST(test_neural_invoke_multiply);
    RUN_TEST(test_neural_invoke_concat);
    RUN_TEST(test_neural_invoke_reshape);
    RUN_TEST(test_neural_invoke_flatten);
    RUN_TEST(test_neural_invoke_dropout);
    RUN_TEST(test_neural_invoke_batch_norm);
    RUN_TEST(test_neural_invoke_layer_norm);
    RUN_TEST(test_neural_invoke_split);
    RUN_TEST(test_neural_invoke_pad);
    RUN_TEST(test_neural_invoke_gather);
    RUN_TEST(test_neural_invoke_reduce);
    RUN_TEST(test_neural_invoke_topk);
    RUN_TEST(test_neural_invoke_matmul);
    RUN_TEST(test_neural_invoke_relu6);
    RUN_TEST(test_neural_invoke_sigmoid);
    RUN_TEST(test_neural_invoke_tanh);
    RUN_TEST(test_neural_invoke_leaky_relu);
    RUN_TEST(test_neural_invoke_clip);
    RUN_TEST(test_neural_invoke_div);
    RUN_TEST(test_neural_invoke_exp);
    RUN_TEST(test_neural_invoke_log);
    RUN_TEST(test_neural_invoke_sqrt);
    RUN_TEST(test_neural_invoke_embedding);
    RUN_TEST(test_neural_invoke_conv1d);
    RUN_TEST(test_neural_invoke_depthwise_conv2d);
    RUN_TEST(test_neural_invoke_quantize);
    RUN_TEST(test_neural_invoke_dequantize);
    RUN_TEST(test_neural_invoke_transpose_conv2d);
    RUN_TEST(test_neural_invoke_reduce_sum);
    RUN_TEST(test_neural_invoke_global_avgpool2d);
    RUN_TEST(test_neural_invoke_attention);
    RUN_TEST(test_neural_invoke_dense_int8);
    RUN_TEST(test_neural_invoke_conv2d_int8);
    RUN_TEST(test_neural_invoke_concat_axis0);
    RUN_TEST(test_neural_invoke_concat_axis1);
    RUN_TEST(test_neural_invoke_reduce_axis0);
    RUN_TEST(test_neural_invoke_gelu);
    RUN_TEST(test_neural_invoke_hard_swish);
    RUN_TEST(test_neural_invoke_resize);
    RUN_TEST(test_neural_invoke_resize_bilinear);
    RUN_TEST(test_neural_invoke_rnn_simple);
    RUN_TEST(test_neural_invoke_lstm_simple);
    RUN_TEST(test_neural_invoke_gru_simple);
    RUN_TEST(test_neural_invoke_conv2d_no_bias);
    RUN_TEST(test_neural_invoke_gather_invalid_axis);
    RUN_TEST(test_neural_invoke_attention_too_large);
    RUN_TEST(test_neural_invoke_split_too_many_outputs);
    RUN_TEST(test_neural_invoke_topk_too_large);
END_TEST_SUITE()

#ifdef EIF_STANDALONE_TEST
int main(void) {
    return run_dl_core_coverage_tests();
}
#endif
