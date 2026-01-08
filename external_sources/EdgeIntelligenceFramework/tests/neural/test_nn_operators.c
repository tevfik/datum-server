#include "../framework/eif_test_runner.h"
#include "eif_neural.h"
#include "eif_dl_internal.h"
#include <math.h>
#include <float.h>

bool test_elementwise_ops() {
    float32_t a[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float32_t b[] = {0.5f, 2.0f, 1.5f, 8.0f};
    float32_t out[4];
    int size = 4;

    // ADD
    eif_layer_add(a, b, out, size);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, out[0], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, out[1], 1e-5f);

    // SUB
    eif_layer_sub(a, b, out, size);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, out[0], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, out[1], 1e-5f);

    // MUL
    eif_layer_multiply(a, b, out, size);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, out[0], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, out[1], 1e-5f);

    // DIV
    eif_layer_div(a, b, out, size);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, out[0], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, out[1], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, out[2], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, out[3], 1e-5f);

    return true;
}

bool test_activations() {
    float32_t in[] = {-1.0f, 0.0f, 1.0f, 3.0f};
    float32_t out[4];
    int size = 4;

    // GELU
    // 0 -> 0
    // 1 -> ~0.8413
    eif_layer_gelu(in, out, size);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, out[1], 1e-5f);
    // Approx check
    if (fabsf(out[2] - 0.8413f) > 0.01f) return false;

    // HardSwish
    // x * ReLU6(x+3)/6
    // 0 -> 0 * 3/6 = 0
    // 3 -> 3 * 6/6 = 3
    // -1 -> -1 * 2/6 = -0.333
    eif_layer_hard_swish(in, out, size);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, out[1], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, out[3], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(-0.333333f, out[0], 1e-4f);

    return true;
}

bool test_math_ops() {
    float32_t in[] = {1.0f, 4.0f, 0.0f};
    float32_t out[3];
    int size = 3;

    // SQRT
    eif_layer_sqrt(in, out, size);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, out[0], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, out[1], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, out[2], 1e-5f);

    // EXP
    eif_layer_exp(in, out, size);
    TEST_ASSERT_EQUAL_FLOAT(2.71828f, out[0], 1e-4f);

    // LOG
    float32_t in_log[] = {2.71828f, 1.0f};
    eif_layer_log(in_log, out, 2);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, out[0], 1e-4f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, out[1], 1e-5f);

    return true;
}

bool test_pad() {
    // Input: 1x2x2x1
    // [[1, 2], [3, 4]]
    float32_t input[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float32_t output[16]; // Enough for 4x4
    
    eif_tensor_shape_t in_shape = {{1, 2, 2, 1}};
    eif_tensor_shape_t out_shape = {{1, 4, 4, 1}};
    eif_layer_param_t param = {0};
    param.pad.pads[0] = 1; // top
    param.pad.pads[1] = 1; // bottom
    param.pad.pads[2] = 1; // left
    param.pad.pads[3] = 1; // right
    param.pad.constant_value = 0.0f;

    eif_layer_pad(input, output, &in_shape, &out_shape, &param);

    // 4x4 output. Inner 2x2 should be input. Border 0.
    // Row 0: 0 0 0 0
    // Row 1: 0 1 2 0
    // Row 2: 0 3 4 0
    // Row 3: 0 0 0 0
    
    TEST_ASSERT_EQUAL_FLOAT(0.0f, output[0], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, output[5], 1e-5f); // (1,1)
    TEST_ASSERT_EQUAL_FLOAT(2.0f, output[6], 1e-5f); // (1,2)
    TEST_ASSERT_EQUAL_FLOAT(3.0f, output[9], 1e-5f); // (2,1)
    TEST_ASSERT_EQUAL_FLOAT(4.0f, output[10], 1e-5f); // (2,2)

    return true;
}

bool test_split() {
    // Input: 1x4x1x1 -> [1, 2, 3, 4]
    float32_t input[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float32_t out1[2];
    float32_t out2[2];
    void* outputs[] = {out1, out2};
    
    eif_tensor_shape_t in_shape = {{1, 4, 1, 1}};
    eif_layer_param_t param = {0};
    param.split.axis = 1;
    param.split.num_splits = 2;

    eif_layer_split(input, outputs, &in_shape, &param, 2);

    TEST_ASSERT_EQUAL_FLOAT(1.0f, out1[0], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, out1[1], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, out2[0], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, out2[1], 1e-5f);

    return true;
}

bool test_gather() {
    // Input: [10, 20, 30, 40]
    float32_t input[] = {10.0f, 20.0f, 30.0f, 40.0f};
    // Indices: [3, 0]
    float32_t indices[] = {3.0f, 0.0f};
    float32_t output[2];
    
    eif_tensor_shape_t in_shape = {{4, 1, 1, 1}};
    eif_tensor_shape_t out_shape = {{2, 1, 1, 1}};
    eif_layer_param_t param = {0};
    param.gather.axis = 0;

    eif_layer_gather(input, indices, output, &in_shape, &out_shape, &param);

    TEST_ASSERT_EQUAL_FLOAT(40.0f, output[0], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, output[1], 1e-5f);

    return true;
}

bool test_matmul() {
    // A: 2x3
    // 1 2 3
    // 4 5 6
    float32_t A[] = {1, 2, 3, 4, 5, 6};
    
    // B: 3x2
    // 7 8
    // 9 1
    // 2 3
    float32_t B[] = {7, 8, 9, 1, 2, 3};
    
    // C: 2x2
    // Row 0: 1*7 + 2*9 + 3*2 = 7 + 18 + 6 = 31
    // Row 0: 1*8 + 2*1 + 3*3 = 8 + 2 + 9 = 19
    // Row 1: 4*7 + 5*9 + 6*2 = 28 + 45 + 12 = 85
    // Row 1: 4*8 + 5*1 + 6*3 = 32 + 5 + 18 = 55
    float32_t C[4];

    eif_layer_matmul(A, B, C, 2, 3, 2);

    TEST_ASSERT_EQUAL_FLOAT(31.0f, C[0], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(19.0f, C[1], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(85.0f, C[2], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(55.0f, C[3], 1e-5f);

    return true;
}

bool test_reduce() {
    // Input: 1x2x2x1
    // 1 2
    // 3 4
    float32_t input[] = {1, 2, 3, 4};
    float32_t output[2];
    
    eif_tensor_shape_t in_shape = {{1, 2, 2, 1}};
    eif_tensor_shape_t out_shape = {{1, 1, 2, 1}}; // Reducing axis 1 (H)
    eif_layer_param_t param = {0};
    param.reduce.axis = 1;

    // Reduce Sum along H
    // Col 0: 1+3 = 4
    // Col 1: 2+4 = 6
    eif_layer_reduce_sum(input, output, &in_shape, &out_shape, &param);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, output[0], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(6.0f, output[1], 1e-5f);

    // Reduce Mean along H
    // Col 0: 4/2 = 2
    // Col 1: 6/2 = 3
    eif_layer_reduce_mean(input, output, &in_shape, &out_shape, &param);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, output[0], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, output[1], 1e-5f);

    return true;
}

bool test_topk() {
    // Input: [10, 5, 20, 15]
    float32_t input[] = {10, 5, 20, 15};
    float32_t values[2];
    float32_t indices[2];
    
    eif_tensor_shape_t in_shape = {{1, 1, 1, 4}};
    eif_layer_param_t param = {0};
    param.topk.k = 2;

    eif_layer_topk(input, values, indices, &in_shape, &param);

    // Top 1: 20 (idx 2)
    // Top 2: 15 (idx 3)
    TEST_ASSERT_EQUAL_FLOAT(20.0f, values[0], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, indices[0], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(15.0f, values[1], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, indices[1], 1e-5f);

    return true;
}

bool test_clip() {
    float32_t input[] = {-5.0f, 0.0f, 5.0f, 10.0f};
    float32_t output[4];
    
    eif_layer_t layer;
    layer.params.clip.min_val = 0.0f;
    layer.params.clip.max_val = 6.0f;
    
    eif_layer_clip(&layer, input, output, 4);
    
    TEST_ASSERT_EQUAL_FLOAT(0.0f, output[0], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, output[1], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, output[2], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(6.0f, output[3], 1e-5f);
    
    return true;
}

bool test_flatten_reshape() {
    float32_t input[] = {1, 2, 3, 4};
    float32_t output[4];
    
    // Just verify copy
    eif_layer_flatten(input, output, 4);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, output[0], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, output[3], 1e-5f);
    
    memset(output, 0, sizeof(output));
    eif_layer_reshape(input, output, 4);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, output[0], 1e-5f);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, output[3], 1e-5f);
    
    return true;
}

bool test_layer_norm() {
    // Input: [1, 2, 3]
    // Mean: 2, Var: ((1-2)^2 + (2-2)^2 + (3-2)^2)/3 = 2/3 = 0.666...
    // Std: sqrt(0.666) = 0.8165
    // Norm: (x - 2) / 0.8165
    // 1 -> -1 / 0.8165 = -1.2247
    // 2 -> 0
    // 3 -> 1.2247
    
    float32_t input[] = {1.0f, 2.0f, 3.0f};
    float32_t output[3];
    
    eif_layer_t layer;
    // Layer norm usually has gamma/beta, but eif_layer_layer_norm signature in internal.h 
    // is: void eif_layer_layer_norm(const eif_layer_t* layer, const float32_t* input, float32_t* output, int h, int w, int c);
    // It seems it might assume HWC normalization or similar.
    // Let's check implementation of eif_layer_layer_norm in eif_dl_layers.c if possible, or assume standard behavior.
    // If it uses layer->weights/biases for gamma/beta.
    
    // For now, let's try with simple params.
    // Assuming it normalizes over the last dimension C.
    
    // We need to mock weights/biases if used.
    float32_t gamma[] = {1.0f, 1.0f, 1.0f};
    float32_t beta[] = {0.0f, 0.0f, 0.0f};
    layer.weights = gamma;
    layer.biases = beta;
    layer.params.layer_norm.epsilon = 1e-5f;
    
    eif_layer_layer_norm(&layer, input, output, 1, 1, 3);
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.2247f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, output[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.2247f, output[2]);
    
    return true;
}

bool test_global_avgpool() {
    // Input: 2x2x1
    // 1 2
    // 3 4
    // Avg: 2.5
    float32_t input[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float32_t output[1];
    
    eif_layer_global_avgpool2d(input, output, 2, 2, 1);
    
    TEST_ASSERT_EQUAL_FLOAT(2.5f, output[0], 1e-5f);
    
    return true;
}

BEGIN_TEST_SUITE(run_nn_operators_tests)
    RUN_TEST(test_elementwise_ops);
    RUN_TEST(test_activations);
    RUN_TEST(test_math_ops);
    RUN_TEST(test_pad);
    RUN_TEST(test_split);
    RUN_TEST(test_gather);
    RUN_TEST(test_matmul);
    RUN_TEST(test_reduce);
    RUN_TEST(test_topk);
    RUN_TEST(test_clip);
    RUN_TEST(test_flatten_reshape);
    RUN_TEST(test_layer_norm);
    RUN_TEST(test_global_avgpool);
END_TEST_SUITE()
