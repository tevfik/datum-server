#include "eif_test_runner.h"
#include "eif_neural.h"
#include "eif_nn_context.h"
#include "eif_fixedpoint.h"
#include <string.h>
#include <stdlib.h>

// Forward declaration if not in public header, but it should be available via eif_neural.h or internal
// Check if eif_layer_dense_q7 is exposed. It's likely internal. 
// If it is internal, we might need to declare it or include internal header.
// Based on grep, it was used in dispatcher coverage, so it might be available.
// Let's declare it here to be safe if it's not in public headers.
void eif_layer_dense_q7(const eif_layer_t* layer, const q7_t* input, q7_t* output, int input_size);

// Helpers
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

bool test_layer_dense_q7_offset(void) {
    // Test eif_layer_dense_q7 with input_offset != 0
    
    eif_layer_t layer;
    memset(&layer, 0, sizeof(eif_layer_t));
    
    // Weights: 1 unit, 2 inputs. [0, 1]
    int8_t weights[] = {10, -10}; 
    // Biases: [0]
    int32_t biases[] = {5};
    
    layer.type = EIF_LAYER_DENSE;
    layer.weights = weights;
    layer.biases = biases;
    layer.params.dense.units = 1;
    
    // Quantization Parameters
    layer.quant_params.input_offset = 128; // Non-zero offset
    layer.quant_params.output_offset = 0;
    layer.quant_params.quantized_activation_min = -128;
    layer.quant_params.quantized_activation_max = 127;
    
    // Input: [-128, 0]
    // real_val = (q - offset) * scale. Here we just test the math.
    // acc += (input + offset) * weight
    // input[0] = -128. input[0] + 128 = 0. 0 * 10 = 0.
    // input[1] = 0.    input[1] + 128 = 128. 128 * -10 = -1280.
    // acc = bias + term1 + term2 = 5 + 0 - 1280 = -1275.
    // output = acc + out_offset = -1275.
    // clamp(-128, 127) -> -128.
    
    int8_t input[] = {-128, 0};
    int8_t output[1];
    
    eif_layer_dense_q7(&layer, input, output, 2);
    
    // Expected: -128
    TEST_ASSERT_EQUAL_INT(-128, output[0]);
    
    // Test with another case to verify positive logic with offset
    // input[0] = -127. (-127 + 128) = 1. 1 * 10 = 10.
    // input[1] = -128. (-128 + 128) = 0. 0 * -10 = 0.
    // acc = 5 + 10 + 0 = 15.
    // output = 15.
    
    input[0] = -127;
    input[1] = -128;
    eif_layer_dense_q7(&layer, input, output, 2);
    
    TEST_ASSERT_EQUAL_INT(15, output[0]);
    
    return true;
}

// Forward declaration
void eif_layer_depthwise_conv2d(const eif_layer_t* layer, const float32_t* input, float32_t* output, int in_h, int in_w, int in_c, int* out_h, int* out_w, int* out_c);

bool test_layer_depthwise_avx2_trigger(void) {
    eif_layer_t layer;
    memset(&layer, 0, sizeof(eif_layer_t));
    
    // Params for AVX2 trigger: depth_multiplier=1, in_c%8==0
    layer.params.depthwise_conv2d.depth_multiplier = 1;
    layer.params.depthwise_conv2d.stride_h = 1;
    layer.params.depthwise_conv2d.stride_w = 1;
    layer.params.depthwise_conv2d.pad_h = 0;
    layer.params.depthwise_conv2d.pad_w = 0;
    
    // Kernel 3x3
    int k_h = 3;
    int k_w = 3;
    layer.params.depthwise_conv2d.kernel_h = k_h;
    layer.params.depthwise_conv2d.kernel_w = k_w;
    
    // Input 4x4, Channels 8
    int in_h = 4;
    int in_w = 4;
    int in_c = 8;
    
    // Weights: [1, k_h, k_w, in_c] (depth_mult=1) -> [k_h * k_w * in_c]
    // Layout might be diff depending on implementation, usually [k_h, k_w, in_c, m] or similar.
    // In eif_dl_layers.c: weights[(ky * k_w + kx) * o_c + out_channel]
    // Since o_c = in_c * m = 8 * 1 = 8.
    // Weights size = 3*3*8 = 72 floats.
    float32_t weights[3*3*8];
    for(int i=0; i<72; i++) weights[i] = 1.0f;
    layer.weights = weights;
    
    // Biases: [8]
    float32_t biases[8];
    for(int i=0; i<8; i++) biases[i] = 0.5f;
    layer.biases = biases;
    
    // Input: 4*4*8 = 128 floats
    float32_t input[4*4*8];
    for(int i=0; i<128; i++) input[i] = 1.0f;
    
    // Output: (4-3)/1 + 1 = 2. 2x2.
    // 2*2*8 = 32 floats.
    float32_t output[2*2*8];
    
    int out_h, out_w, out_c;
    
    eif_layer_depthwise_conv2d(&layer, input, output, in_h, in_w, in_c, &out_h, &out_w, &out_c);
    
    // Verify dimensions
    TEST_ASSERT_EQUAL_INT(2, out_h);
    TEST_ASSERT_EQUAL_INT(2, out_w);
    TEST_ASSERT_EQUAL_INT(8, out_c);
    
    return true;
}

// Forward declaration
void eif_layer_lstm(const eif_layer_t* layer, const float32_t* input, float32_t* output, int input_size, float32_t* state);

bool test_layer_lstm_units_limit(void) {
    eif_layer_t layer;
    memset(&layer, 0, sizeof(eif_layer_t));
    layer.params.lstm.units = 129; // > 128
    
    float32_t dummy_weights[1];
    layer.weights = dummy_weights;
    
    float32_t state[300]; 
    float32_t input[1];
    float32_t output[130];
    
    // Call - should return immediately without crash or action
    eif_layer_lstm(&layer, input, output, 1, state);
    
    return true;
}

bool test_train_relu_positive_inputs(void) {
    eif_memory_pool_t* pool = create_pool(16384); // Increased size
    eif_neural_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    // Model: Input -> ReLU -> Output
    eif_model_t model = {0};
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    
    // Tensor 0: Input (1 element)
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = sizeof(float32_t);
    tensors[0].is_variable = true;
    
    // Tensor 1: Output
    tensors[1] = tensors[0];
    tensors[1].data = NULL; // Variable
    
    model.tensors = tensors;
    model.num_tensors = 2;
    
    int model_in[] = {0};
    int model_out[] = {1};
    model.input_tensor_indices = model_in;
    model.num_inputs = 1;
    model.output_tensor_indices = model_out;
    model.num_outputs = 1;
    
    eif_layer_node_t nodes[1];
    memset(nodes, 0, sizeof(nodes));
    int n0_in[] = {0};
    int n0_out[] = {1};
    nodes[0].type = EIF_LAYER_RELU;
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 1;
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;
    nodes[0].params = NULL;
    
    model.nodes = nodes;
    model.num_nodes = 1;
    
    if (eif_neural_init(&ctx, &model, pool) != EIF_STATUS_OK) return false;
    
    // Input: Positive value to trigger gradient propagation
    float32_t input[] = {2.0f};
    if (eif_neural_set_input(&ctx, 0, input, sizeof(input)) != EIF_STATUS_OK) return false;
    
    if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) return false;
    
    // Train Context
    eif_neural_train_ctx_t train_ctx;
    eif_optimizer_t opt = {0};
    opt.learning_rate = 0.01f;
    
    if (eif_neural_train_init(&train_ctx, &ctx, &opt, pool) != EIF_STATUS_OK) return false;
    
    float32_t target[] = {1.0f}; 
    if (eif_neural_backward(&train_ctx, target) != EIF_STATUS_OK) return false;
    
    destroy_pool(pool);
    return true;
}

// Mock op for scratch stats
static eif_status_t mock_scratch_op(eif_dl_context_t* ctx, const eif_layer_node_t* node, void** in, void** out) {
    // Simulate usage
    ctx->arena.scratch_used = 127;
    return EIF_STATUS_OK;
} 

bool test_context_scratch_stats(void) {
    eif_memory_pool_t* pool = create_pool(4096);
    eif_dl_context_t ctx; 
    
    eif_model_t model = {0};
    eif_tensor_t tensors[2] = {0};
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = sizeof(float32_t);
    tensors[0].is_variable = true;
    tensors[1] = tensors[0];

    model.tensors = tensors;
    model.num_tensors = 2;
    int model_in[] = {0}; int model_out[] = {1};
    model.input_tensor_indices = model_in; model.num_inputs = 1;
    model.output_tensor_indices = model_out; model.num_outputs = 1;

    eif_layer_node_t nodes[1] = {0};
    int n0_in[] = {0}; int n0_out[] = {1};
    nodes[0].type = EIF_LAYER_RELU; 
    nodes[0].input_indices = n0_in; nodes[0].num_inputs = 1;
    nodes[0].output_indices = n0_out; nodes[0].num_outputs = 1;
    model.nodes = nodes; model.num_nodes = 1;
    model.scratch_size = 1024; // Explicit small scratch

    // Custom registry
    eif_op_registry_t reg = {0};
    reg.funcs[EIF_LAYER_RELU] = mock_scratch_op;

    eif_status_t init_status = eif_dl_context_init(&ctx, &model, pool, &reg);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, init_status);
    
    eif_dl_invoke(&ctx);
    
    TEST_ASSERT_EQUAL_INT(127, ctx.stats.peak_scratch_bytes);
    
    destroy_pool(pool);
    return true;
}

bool test_invoke_no_registry(void) {
    eif_memory_pool_t* pool = create_pool(4096);
    eif_dl_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    eif_model_t model = {0};
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = sizeof(float32_t);
    tensors[0].is_variable = true;
    tensors[1] = tensors[0];
    tensors[1].data = NULL;

    model.tensors = tensors;
    model.num_tensors = 2;
    
    int model_in[] = {0};
    int model_out[] = {1};
    model.input_tensor_indices = model_in;
    model.num_inputs = 1;
    model.output_tensor_indices = model_out;
    model.num_outputs = 1;

    eif_layer_node_t nodes[1];
    memset(nodes, 0, sizeof(nodes));
    int n0_in[] = {0};
    int n0_out[] = {1};
    nodes[0].type = EIF_LAYER_RELU; 
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 1;
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;
    
    model.nodes = nodes;
    model.num_nodes = 1;
    model.scratch_size = 1024; // Explicit small scratch
    
    eif_status_t init_status = eif_dl_context_init(&ctx, &model, pool, NULL);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, init_status);
    
    // Force registry to NULL AFTER init
    ctx.op_registry = NULL;
    
    // Check tensor data allocation
    if (!ctx.tensor_data) {
        // TEST_FAIL_MESSAGE not defined
        return false;
    }
    
    float32_t input[] = {1.0f};
    
    // Use proper helper if available, or just copy
    // eif_neural_set_input expects neural_ctx, might fail cast?
    // Using simple memcpy since we know layout
    void* in_ptr = ctx.tensor_data[0];
    memcpy(in_ptr, input, sizeof(input));
    
    eif_status_t status = eif_dl_invoke(&ctx);
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    destroy_pool(pool);
    return true;
}

// Run suite
BEGIN_TEST_SUITE(run_layer_coverage_tests)
    RUN_TEST(test_layer_dense_q7_offset);
    RUN_TEST(test_layer_depthwise_avx2_trigger);
    RUN_TEST(test_layer_lstm_units_limit);
    RUN_TEST(test_train_relu_positive_inputs);
    RUN_TEST(test_invoke_no_registry);
    RUN_TEST(test_context_scratch_stats);
END_TEST_SUITE()
