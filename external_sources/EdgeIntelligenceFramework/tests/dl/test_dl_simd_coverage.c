#include "../framework/eif_test_runner.h"
#include "eif_neural.h"
#include "eif_dl_internal.h"
#include <stdlib.h>
#include <string.h>

// Helper to create a layer
static void setup_conv2d_layer(eif_layer_t* layer, int k_h, int k_w, int stride_h, int stride_w, 
                             const float32_t* weights, const float32_t* biases) {
    layer->type = EIF_LAYER_CONV2D;
    layer->params.conv2d.kernel_h = k_h;
    layer->params.conv2d.kernel_w = k_w;
    layer->params.conv2d.stride_h = stride_h;
    layer->params.conv2d.stride_w = stride_w;
    layer->params.conv2d.pad_h = 0;
    layer->params.conv2d.pad_w = 0;
    layer->weights = weights;
    layer->biases = biases;
}

// Test Case 1: Channels = 8 (Exact SIMD block)
bool test_conv2d_simd_c8() {
    // Input: 1x1x8 (1 pixel, 8 channels)
    // Filter: 1x1x8, 1 filter
    // Output: 1x1x1
    
    int in_h = 1, in_w = 1, in_c = 8;
    int out_h = 1, out_w = 1, out_c = 1;
    int k_h = 1, k_w = 1;
    
    float32_t input[8];
    for(int i=0; i<8; i++) input[i] = 1.0f;
    
    float32_t weights[8]; // 1 filter * 1*1 * 8
    for(int i=0; i<8; i++) weights[i] = 0.5f;
    
    float32_t bias[1] = {0.0f};
    float32_t output[1] = {0.0f};
    
    eif_layer_t layer;
    setup_conv2d_layer(&layer, k_h, k_w, 1, 1, weights, bias);
    
    eif_conv2d_simd(&layer, input, output, in_h, in_w, in_c, out_h, out_w, out_c);
    
    // Expected: sum(1.0 * 0.5) * 8 = 4.0
    TEST_ASSERT_EQUAL_FLOAT(4.0f, output[0], 1e-5f);
    
    return true;
}

// Test Case 2: Channels = 10 (SIMD block + Remainder)
bool test_conv2d_simd_c10() {
    // Input: 1x1x10
    // Filter: 1x1x10, 1 filter
    
    int in_h = 1, in_w = 1, in_c = 10;
    int out_h = 1, out_w = 1, out_c = 1;
    int k_h = 1, k_w = 1;
    
    float32_t input[10];
    for(int i=0; i<10; i++) input[i] = 1.0f;
    
    float32_t weights[10];
    for(int i=0; i<10; i++) weights[i] = 0.5f;
    
    float32_t bias[1] = {1.0f}; // Add bias
    float32_t output[1] = {0.0f};
    
    eif_layer_t layer;
    setup_conv2d_layer(&layer, k_h, k_w, 1, 1, weights, bias);
    
    eif_conv2d_simd(&layer, input, output, in_h, in_w, in_c, out_h, out_w, out_c);
    
    // Expected: sum(1.0 * 0.5) * 10 + bias = 5.0 + 1.0 = 6.0
    TEST_ASSERT_EQUAL_FLOAT(6.0f, output[0], 1e-5f);
    
    return true;
}

// Test Case 3: Channels = 3 (Only Remainder)
bool test_conv2d_simd_c3() {
    // Input: 1x1x3
    // Filter: 1x1x3, 1 filter
    
    int in_h = 1, in_w = 1, in_c = 3;
    int out_h = 1, out_w = 1, out_c = 1;
    int k_h = 1, k_w = 1;
    
    float32_t input[3] = {1.0f, 2.0f, 3.0f};
    float32_t weights[3] = {0.5f, 0.5f, 0.5f};
    
    float32_t bias[1] = {0.0f};
    float32_t output[1] = {0.0f};
    
    eif_layer_t layer;
    setup_conv2d_layer(&layer, k_h, k_w, 1, 1, weights, bias);
    
    eif_conv2d_simd(&layer, input, output, in_h, in_w, in_c, out_h, out_w, out_c);
    
    // Expected: 0.5 + 1.0 + 1.5 = 3.0
    TEST_ASSERT_EQUAL_FLOAT(3.0f, output[0], 1e-5f);
    
    return true;
}

// Test Case 4: Larger Spatial (2x2)
bool test_conv2d_simd_spatial() {
    // Input: 2x2x8
    // Filter: 1x1x8, 1 filter
    // Output: 2x2x1
    
    int in_h = 2, in_w = 2, in_c = 8;
    int out_h = 2, out_w = 2, out_c = 1;
    int k_h = 1, k_w = 1;
    
    float32_t input[32]; // 2*2*8
    for(int i=0; i<32; i++) input[i] = 1.0f;
    
    float32_t weights[8];
    for(int i=0; i<8; i++) weights[i] = 1.0f;
    
    float32_t bias[1] = {0.0f};
    float32_t output[4];
    
    eif_layer_t layer;
    setup_conv2d_layer(&layer, k_h, k_w, 1, 1, weights, bias);
    
    eif_conv2d_simd(&layer, input, output, in_h, in_w, in_c, out_h, out_w, out_c);
    
    // Expected: sum(1.0 * 1.0) * 8 = 8.0 for all 4 pixels
    for(int i=0; i<4; i++) {
        TEST_ASSERT_EQUAL_FLOAT(8.0f, output[i], 1e-5f);
    }
    
    return true;
}

// Test Case 5: No Bias
bool test_conv2d_simd_no_bias() {
    int in_h = 1, in_w = 1, in_c = 8;
    int out_h = 1, out_w = 1, out_c = 1;
    int k_h = 1, k_w = 1;
    
    float32_t input[8];
    for(int i=0; i<8; i++) input[i] = 1.0f;
    
    float32_t weights[8];
    for(int i=0; i<8; i++) weights[i] = 0.5f;
    
    float32_t output[1] = {0.0f};
    
    eif_layer_t layer;
    setup_conv2d_layer(&layer, k_h, k_w, 1, 1, weights, NULL); // NULL bias
    
    eif_conv2d_simd(&layer, input, output, in_h, in_w, in_c, out_h, out_w, out_c);
    
    TEST_ASSERT_EQUAL_FLOAT(4.0f, output[0], 1e-5f);
    
    return true;
}

BEGIN_TEST_SUITE(run_dl_simd_coverage_tests)
    RUN_TEST(test_conv2d_simd_c8);
    RUN_TEST(test_conv2d_simd_c10);
    RUN_TEST(test_conv2d_simd_c3);
    RUN_TEST(test_conv2d_simd_spatial);
    RUN_TEST(test_conv2d_simd_no_bias);
END_TEST_SUITE()
