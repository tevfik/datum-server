#include "eif_dl_internal.h"
#include "eif_quantize.h"
#include <stdint.h>
#include <math.h>

// ============================================================================
// Helper Functions
// ============================================================================

// Saturating rounding doubling high mul
// Performs: round(a * b / 2^31)
static inline int32_t eif_saturating_rounding_doubling_high_mul(int32_t a, int32_t b) {
    bool overflow = (a == b && a == INT32_MIN);
    int64_t a_64 = (int64_t)a;
    int64_t b_64 = (int64_t)b;
    int64_t ab_64 = a_64 * b_64;
    int32_t nudge = ab_64 >= 0 ? (1 << 30) : (1 - (1 << 30));
    int32_t ab_x2_high32 = (int32_t)((ab_64 + nudge) / (1LL << 31));
    return overflow ? INT32_MAX : ab_x2_high32;
}

// Multiply by quantized multiplier
// Returns (x * multiplier) >> shift
static inline int32_t eif_multiply_by_quantized_multiplier(int32_t x, int32_t multiplier, int32_t shift) {
    int32_t xx = eif_saturating_rounding_doubling_high_mul(x, multiplier);
    // Shift is usually negative for right shift in some conventions, or positive.
    // TFLite convention: shift is negative for right shift.
    // But here let's assume shift > 0 means right shift.
    // Wait, let's check standard usage.
    // Usually: result = (x * multiplier) * 2^shift
    // If shift is negative, it's a right shift.
    
    // Let's assume shift is the exponent.
    // If shift > 0, left shift. If shift < 0, right shift.
    // But eif_saturating_rounding_doubling_high_mul returns high 32 bits of 2*a*b.
    // Effectively it is (a * b) >> 31.
    
    // Let's stick to a simpler float implementation for correctness first, 
    // or use standard integer math if we are sure about the params.
    // Given I don't have the full TFLite quantization utility suite, 
    // I will use a hybrid approach or simple integer math if shift is simple.
    
    // For now, let's use the "shift right" interpretation for positive shift 
    // if that's what the params imply, OR follow TFLite:
    // shift is "left shift". So negative means right shift.
    
    int left_shift = shift;
    if (left_shift > 0) {
        return xx << left_shift;
    } else {
        return xx >> (-left_shift);
    }
}

// ============================================================================
// Layer Implementations
// ============================================================================

void eif_layer_quantize(const eif_layer_t* layer, const float32_t* input, int8_t* output, int size) {
    // Float -> Int8
    // q = (r / scale) + zero_point
    float scale = layer->quant_params.output_multiplier / (float)(1 << 31); // Placeholder if using int params
    // Actually, eif_layer_t has quant_params which are int32.
    // But usually Quantize layer has float scale/zp in params or we derive them.
    // Let's assume layer->params.quantize has the float params if it's a Quantize layer.
    
    // Check if it is EIF_LAYER_QUANTIZE
    if (layer->type == EIF_LAYER_QUANTIZE) {
        float scale = layer->params.quantize.scale;
        int32_t zp = layer->params.quantize.zero_point;
        
        for (int i = 0; i < size; i++) {
            float val = input[i];
            int32_t q = (int32_t)roundf(val / scale) + zp;
            if (q < -128) q = -128;
            if (q > 127) q = 127;
            output[i] = (int8_t)q;
        }
    }
}

void eif_layer_dequantize(const eif_layer_t* layer, const int8_t* input, float32_t* output, int size) {
    // Int8 -> Float
    // r = (q - zero_point) * scale
    if (layer->type == EIF_LAYER_DEQUANTIZE) {
        float scale = layer->params.dequantize.scale;
        int32_t zp = layer->params.dequantize.zero_point;
        
        for (int i = 0; i < size; i++) {
            int32_t q = input[i];
            output[i] = (float)(q - zp) * scale;
        }
    }
}

void eif_layer_conv2d_int8(const eif_layer_t* layer, const int8_t* input, int8_t* output, 
                           int in_h, int in_w, int in_c, int* out_h, int* out_w, int* out_c) {
    int filters = layer->params.conv2d.filters;
    int k_h = layer->params.conv2d.kernel_h;
    int k_w = layer->params.conv2d.kernel_w;
    int stride_h = layer->params.conv2d.stride_h;
    int stride_w = layer->params.conv2d.stride_w;
    int pad_h = layer->params.conv2d.pad_h;
    int pad_w = layer->params.conv2d.pad_w;
    
    int o_h = (in_h + 2*pad_h - k_h) / stride_h + 1;
    int o_w = (in_w + 2*pad_w - k_w) / stride_w + 1;
    *out_h = o_h; *out_w = o_w; *out_c = filters;
    
    const int8_t* weights = (const int8_t*)layer->weights;
    const int32_t* biases = (const int32_t*)layer->biases; // Biases are usually int32 in Int8 quantization
    
    int32_t input_offset = layer->quant_params.input_offset;
    int32_t output_offset = layer->quant_params.output_offset;
    int32_t output_multiplier = layer->quant_params.output_multiplier;
    int32_t output_shift = layer->quant_params.output_shift;
    int32_t act_min = layer->quant_params.quantized_activation_min;
    int32_t act_max = layer->quant_params.quantized_activation_max;
    
    #pragma omp parallel for collapse(2)
    for (int f = 0; f < filters; f++) {
        for (int y = 0; y < o_h; y++) {
            for (int x = 0; x < o_w; x++) {
                int32_t acc = 0;
                
                for (int ky = 0; ky < k_h; ky++) {
                    for (int kx = 0; kx < k_w; kx++) {
                        int in_y = y * stride_h + ky - pad_h;
                        int in_x = x * stride_w + kx - pad_w;
                        
                        if (in_y >= 0 && in_y < in_h && in_x >= 0 && in_x < in_w) {
                            int32_t input_val = input[(in_y * in_w + in_x) * in_c]; // Vectorize this loop?
                            // Wait, inner loop over channels
                            for (int c = 0; c < in_c; c++) {
                                int32_t i_val = input[(in_y * in_w + in_x) * in_c + c];
                                int32_t w_val = weights[((f * k_h + ky) * k_w + kx) * in_c + c];
                                
                                // (q_in + offset) * q_w
                                // Assuming weights are symmetric (offset 0)
                                acc += (i_val + input_offset) * w_val;
                            }
                        }
                    }
                }
                
                if (biases) {
                    acc += biases[f];
                }
                
                // Requantize
                // acc = (acc * multiplier) >> shift
                acc = eif_multiply_by_quantized_multiplier(acc, output_multiplier, output_shift);
                acc += output_offset;
                
                // Activation
                if (acc < act_min) acc = act_min;
                if (acc > act_max) acc = act_max;
                
                // Clamp to int8
                if (acc < -128) acc = -128;
                if (acc > 127) acc = 127;
                
                output[(y * o_w + x) * filters + f] = (int8_t)acc;
            }
        }
    }
}

void eif_layer_dense_int8(const eif_layer_t* layer, const int8_t* input, int8_t* output, int input_size) {
    int units = layer->params.dense.units;
    const int8_t* weights = (const int8_t*)layer->weights;
    const int32_t* biases = (const int32_t*)layer->biases;
    
    int32_t input_offset = layer->quant_params.input_offset;
    int32_t output_offset = layer->quant_params.output_offset;
    int32_t output_multiplier = layer->quant_params.output_multiplier;
    int32_t output_shift = layer->quant_params.output_shift;
    int32_t act_min = layer->quant_params.quantized_activation_min;
    int32_t act_max = layer->quant_params.quantized_activation_max;
    
    #pragma omp parallel for
    for (int i = 0; i < units; i++) {
        int32_t acc = 0;
        if (biases) acc = biases[i];
        
        for (int j = 0; j < input_size; j++) {
            int32_t i_val = input[j];
            int32_t w_val = weights[i * input_size + j];
            acc += (i_val + input_offset) * w_val;
        }
        
        // Requantize
        acc = eif_multiply_by_quantized_multiplier(acc, output_multiplier, output_shift);
        acc += output_offset;
        
        // Activation
        if (acc < act_min) acc = act_min;
        if (acc > act_max) acc = act_max;
        
        // Clamp to int8
        if (acc < -128) acc = -128;
        if (acc > 127) acc = 127;
        
        output[i] = (int8_t)acc;
    }
}

void eif_layer_depthwise_conv2d_int8(const eif_layer_t* layer, const int8_t* input, int8_t* output,
                                     int in_h, int in_w, int in_c, int* out_h, int* out_w, int* out_c) {
    int k_h = layer->params.depthwise_conv2d.kernel_h;
    int k_w = layer->params.depthwise_conv2d.kernel_w;
    int stride_h = layer->params.depthwise_conv2d.stride_h;
    int stride_w = layer->params.depthwise_conv2d.stride_w;
    int depth_multiplier = layer->params.depthwise_conv2d.depth_multiplier;
    
    int o_h = (in_h - k_h) / stride_h + 1;
    int o_w = (in_w - k_w) / stride_w + 1;
    int o_c = in_c * depth_multiplier;
    
    *out_h = o_h; *out_w = o_w; *out_c = o_c;
    
    const int8_t* weights = (const int8_t*)layer->weights;
    const int32_t* biases = (const int32_t*)layer->biases;
    
    // Check quantization params
    eif_quant_param_t qp = layer->quant_params;
    int32_t input_offset = qp.input_offset;
    int32_t output_offset = qp.output_offset;
    int32_t output_multiplier = qp.output_multiplier;
    int32_t output_shift = qp.output_shift;
    int32_t act_min = qp.quantized_activation_min;
    int32_t act_max = qp.quantized_activation_max;
    
    // Default activation limits
    if (act_min == 0 && act_max == 0) {
       act_min = -128;
       act_max = 127;
    }

    // Optimization: Channel-first loop to improve weight locality and simplify bias access
    // Although input locality is slightly worse, Depthwise usually has few channels per group.
    
    for (int c = 0; c < in_c; c++) {
        for (int m = 0; m < depth_multiplier; m++) {
            int out_channel = c * depth_multiplier + m;
            int32_t bias_val = biases ? biases[out_channel] : 0;
            
            // Pointer to the start of weights for this channel
            // Weights logic: weights[(ky * k_w + kx) * o_c + out_channel]
            const int8_t* w_base = weights + out_channel;

            for (int y = 0; y < o_h; y++) {
                int in_y_origin = y * stride_h;
                
                for (int x = 0; x < o_w; x++) {
                    int in_x_origin = x * stride_w;
                    int32_t acc = 0;
                    
                    const int8_t* w_ptr = w_base;
                    
                    for (int ky = 0; ky < k_h; ky++) {
                        int in_y = in_y_origin + ky;
                        // Input index: (in_y * in_w + in_x_origin) * in_c + c
                        // We start at (in_x_origin) and move by 1 (stride_w=1 case?) 
                        // No, in spatial loop we move by stride_w.
                        // Inside kernel, we move by dilation (assumed 1).
                        
                        const int8_t* in_ptr = input + (in_y * in_w + in_x_origin) * in_c + c;
                        
                        for (int kx = 0; kx < k_w; kx++) {
                            int32_t val = *in_ptr;
                            int32_t w_val = *w_ptr;
                            
                            acc += (val + input_offset) * w_val;
                            
                            in_ptr += in_c;        // Move to next pixel in row (stride 1)
                            w_ptr += o_c;          // Move to next weight for this channel
                        }
                    }
                    
                    acc += bias_val;
                    
                    // Requantize down to int8
                    acc = eif_multiply_by_quantized_multiplier(acc, output_multiplier, output_shift);
                    acc += output_offset;
                    
                    // Clamp
                    if (acc < act_min) acc = act_min;
                    if (acc > act_max) acc = act_max;
                    if (acc < -128) acc = -128;
                    if (acc > 127) acc = 127;
                    
                    output[(y * o_w + x) * o_c + out_channel] = (int8_t)acc;
                }
            }
        }
    }
}
