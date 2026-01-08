#include "eif_dl_internal.h"
#include "eif_quantize.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#define EPSILON 0.1f

void test_quantize_dequantize() {
    printf("Testing Quantize/Dequantize...\n");
    
    eif_layer_t layer;
    memset(&layer, 0, sizeof(layer));
    layer.type = EIF_LAYER_QUANTIZE;
    layer.params.quantize.scale = 0.5f;
    layer.params.quantize.zero_point = -10;
    
    float input[] = {0.0f, 0.5f, 1.0f, -1.0f};
    int8_t output_q[4];
    float output_f[4];
    
    // Quantize
    // 0.0 -> 0/0.5 - 10 = -10
    // 0.5 -> 1/0.5 - 10 = -9 (Wait, formula is q = r/S + Z)
    // q = 0.0/0.5 + (-10) = -10
    // q = 0.5/0.5 + (-10) = 1 - 10 = -9
    // q = 1.0/0.5 + (-10) = 2 - 10 = -8
    // q = -1.0/0.5 + (-10) = -2 - 10 = -12
    
    eif_layer_quantize(&layer, input, output_q, 4);
    
    assert(output_q[0] == -10);
    assert(output_q[1] == -9);
    assert(output_q[2] == -8);
    assert(output_q[3] == -12);
    
    // Dequantize
    layer.type = EIF_LAYER_DEQUANTIZE;
    layer.params.dequantize.scale = 0.5f;
    layer.params.dequantize.zero_point = -10;
    
    eif_layer_dequantize(&layer, output_q, output_f, 4);
    
    for(int i=0; i<4; i++) {
        if (fabs(output_f[i] - input[i]) > EPSILON) {
            printf("Mismatch at %d: %f vs %f\n", i, output_f[i], input[i]);
            exit(1);
        }
    }
    printf("Quantize/Dequantize Passed.\n");
}

void test_conv2d_int8() {
    printf("Testing Conv2D Int8...\n");
    
    // Input: 3x3x1
    int in_h = 3, in_w = 3, in_c = 1;
    int8_t input[9] = {
        1, 1, 1,
        1, 1, 1,
        1, 1, 1
    };
    
    // Weights: 1 filter, 2x2x1
    int8_t weights[4] = {
        1, 1,
        1, 1
    };
    
    // Bias: 0
    int32_t biases[1] = {0};
    
    eif_layer_t layer;
    memset(&layer, 0, sizeof(layer));
    layer.type = EIF_LAYER_CONV2D;
    layer.params.conv2d.filters = 1;
    layer.params.conv2d.kernel_h = 2;
    layer.params.conv2d.kernel_w = 2;
    layer.params.conv2d.stride_h = 1;
    layer.params.conv2d.stride_w = 1;
    layer.params.conv2d.pad_h = 0;
    layer.params.conv2d.pad_w = 0;
    
    layer.weights = weights;
    layer.biases = biases;
    
    // Quant Params
    // Input ZP = 0 (offset = 0)
    // Output ZP = 0 (offset = 0)
    // Multiplier = 1.0 (represented as integer)
    // Shift = 0
    
    // Multiplier 1.0 in Q31 is 1<<31 (but that overflows int32 positive range)
    // Usually represented as 1<<30 with shift 1?
    // Or just use a smaller multiplier and shift.
    // Let's use multiplier = 1<<30, shift = 1.
    // 1<<30 * x >> 31 -> x/2. Then << 1 -> x.
    
    layer.quant_params.input_offset = 0;
    layer.quant_params.output_offset = 0;
    layer.quant_params.output_multiplier = 1073741824; // 2^30
    layer.quant_params.output_shift = 1; 
    layer.quant_params.quantized_activation_min = -128;
    layer.quant_params.quantized_activation_max = 127;
    
    int out_h, out_w, out_c;
    int8_t output[4]; // 2x2 output
    
    eif_layer_conv2d_int8(&layer, input, output, in_h, in_w, in_c, &out_h, &out_w, &out_c);
    
    // Expected:
    // Each window is 2x2 of 1s. Sum = 4.
    // Acc = 4.
    // Output = 4.
    
    printf("Output: %d %d %d %d\n", output[0], output[1], output[2], output[3]);
    
    assert(output[0] == 4);
    assert(output[1] == 4);
    assert(output[2] == 4);
    assert(output[3] == 4);
    
    printf("Conv2D Int8 Passed.\n");
}

void test_dense_int8() {
    printf("Testing Dense Int8...\n");
    
    // Input: 4 elements
    int input_size = 4;
    int8_t input[4] = {1, 2, 3, 4};
    
    // Weights: 2 units, 4 inputs each
    // Unit 0: 1, 1, 1, 1 -> Sum = 1+2+3+4 = 10
    // Unit 1: 1, 0, 1, 0 -> Sum = 1+0+3+0 = 4
    int8_t weights[8] = {
        1, 1, 1, 1,
        1, 0, 1, 0
    };
    
    // Bias: 0
    int32_t biases[2] = {0, 0};
    
    eif_layer_t layer;
    memset(&layer, 0, sizeof(layer));
    layer.type = EIF_LAYER_DENSE;
    layer.params.dense.units = 2;
    layer.weights = weights;
    layer.biases = biases;
    
    // Quant Params (Identity)
    layer.quant_params.input_offset = 0;
    layer.quant_params.output_offset = 0;
    layer.quant_params.output_multiplier = 1073741824; // 2^30
    layer.quant_params.output_shift = 1; 
    layer.quant_params.quantized_activation_min = -128;
    layer.quant_params.quantized_activation_max = 127;
    
    int8_t output[2];
    
    eif_layer_dense_int8(&layer, input, output, input_size);
    
    printf("Output: %d %d\n", output[0], output[1]);
    
    assert(output[0] == 10);
    assert(output[1] == 4);
    
    printf("Dense Int8 Passed.\n");
}

int main() {
    test_quantize_dequantize();
    test_conv2d_int8();
    test_dense_int8();
    return 0;
}
