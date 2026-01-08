#include "eif_dl_internal.h"
#include <float.h>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

void eif_layer_dense(const eif_layer_t* layer, const float32_t* input, float32_t* output, int input_size) {
    int units = layer->params.dense.units;
    const float32_t* weights = (const float32_t*)layer->weights;
    const float32_t* biases = (const float32_t*)layer->biases;
    
    #pragma omp parallel for
    for (int i = 0; i < units; i++) {
        float32_t sum = 0.0f;
        if (biases) sum = biases[i];
        for (int j = 0; j < input_size; j++) sum += input[j] * weights[i * input_size + j];
        output[i] = sum;
    }
}

eif_status_t eif_layer_conv2d(const eif_layer_t* layer, const float32_t* input, float32_t* output, 
                             int in_h, int in_w, int in_c, int* out_h, int* out_w, int* out_c) {
    if (!layer || !input || !output || !out_h || !out_w || !out_c) return EIF_STATUS_INVALID_ARGUMENT;

    int filters = layer->params.conv2d.filters;
    int k_h = layer->params.conv2d.kernel_h;
    int k_w = layer->params.conv2d.kernel_w;
    int stride_h = layer->params.conv2d.stride_h;
    int stride_w = layer->params.conv2d.stride_w;
    
    if (stride_h <= 0 || stride_w <= 0) return EIF_STATUS_INVALID_ARGUMENT;
    if (in_h < k_h || in_w < k_w) return EIF_STATUS_INVALID_ARGUMENT;

    int o_h = (in_h - k_h) / stride_h + 1;
    int o_w = (in_w - k_w) / stride_w + 1;
    
    if (o_h <= 0 || o_w <= 0) return EIF_STATUS_INVALID_ARGUMENT;

    *out_h = o_h; *out_w = o_w; *out_c = filters;

    // Check for SIMD support (AVX2)
    #if defined(__AVX2__)
    // Only use SIMD if filters is multiple of 8 (for our simple implementation)
    // and input channels >= 8 (for inner loop optimization if we did that)
    // Our implementation vectorizes over C (inner loop) in blocks of 8.
    // So we need in_c >= 8 for it to be worth it?
    // Actually, the implementation handles remainder.
    // But let's just call it.
    eif_conv2d_simd(layer, input, output, in_h, in_w, in_c, o_h, o_w, filters);
    return EIF_STATUS_OK;
    #endif

    const float32_t* weights = (const float32_t*)layer->weights;
    const float32_t* biases = (const float32_t*)layer->biases;
    
    #pragma omp parallel for collapse(2)
    for (int f = 0; f < filters; f++) {
        for (int y = 0; y < o_h; y++) {
            for (int x = 0; x < o_w; x++) {
                float32_t sum = 0.0f;
                if (biases) sum = biases[f];
                for (int ky = 0; ky < k_h; ky++) {
                    for (int kx = 0; kx < k_w; kx++) {
                        for (int c = 0; c < in_c; c++) {
                            int in_y = y * stride_h + ky;
                            int in_x = x * stride_w + kx;
                            float32_t val = input[(in_y * in_w + in_x) * in_c + c];
                            float32_t w = weights[((f * k_h + ky) * k_w + kx) * in_c + c];
                            sum += val * w;
                        }
                    }
                }
                output[(y * o_w + x) * filters + f] = sum;
            }
        }
    }
    return EIF_STATUS_OK;
}

void eif_layer_depthwise_conv2d(const eif_layer_t* layer, const float32_t* input, float32_t* output,
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
    const float32_t* weights = (const float32_t*)layer->weights;
    const float32_t* biases = (const float32_t*)layer->biases;

#if defined(__AVX2__)
    // AVX2 Optimization for Depthwise Conv2D (Multiplier=1)
    if (depth_multiplier == 1 && in_c % 8 == 0) {
        #pragma omp parallel for collapse(2)
        for (int y = 0; y < o_h; y++) {
            for (int x = 0; x < o_w; x++) {
                // Process 8 channels at a time
                for (int c = 0; c < in_c; c += 8) {
                    __m256 sum = _mm256_setzero_ps();
                    if (biases) {
                        sum = _mm256_loadu_ps(&biases[c]);
                    }
                    
                    for (int ky = 0; ky < k_h; ky++) {
                        for (int kx = 0; kx < k_w; kx++) {
                            int in_y = y * stride_h + ky;
                            int in_x = x * stride_w + kx;
                            
                            // Input index: (in_y * in_w + in_x) * in_c + c
                            // Contiguous 8 floats
                            __m256 in_vec = _mm256_loadu_ps(&input[(in_y * in_w + in_x) * in_c + c]);
                            
                            // Weight index: (ky * k_w + kx) * o_c + c
                            // Contiguous 8 floats (since o_c = in_c)
                            __m256 w_vec = _mm256_loadu_ps(&weights[(ky * k_w + kx) * o_c + c]);
                            
                            sum = _mm256_fmadd_ps(in_vec, w_vec, sum);
                        }
                    }
                    
                    _mm256_storeu_ps(&output[(y * o_w + x) * o_c + c], sum);
                }
            }
        }
        return;
    }
#endif

    for (int c = 0; c < in_c; c++) {
        for (int m = 0; m < depth_multiplier; m++) {
            int out_channel = c * depth_multiplier + m;
            for (int y = 0; y < o_h; y++) {
                for (int x = 0; x < o_w; x++) {
                    float32_t sum = 0.0f;
                    if (biases) sum = biases[out_channel];
                    for (int ky = 0; ky < k_h; ky++) {
                        for (int kx = 0; kx < k_w; kx++) {
                            int in_y = y * stride_h + ky;
                            int in_x = x * stride_w + kx;
                            float32_t val = input[(in_y * in_w + in_x) * in_c + c];
                            float32_t w = weights[(ky * k_w + kx) * o_c + out_channel];
                            sum += val * w;
                        }
                    }
                    output[(y * o_w + x) * o_c + out_channel] = sum;
                }
            }
        }
    }
}

void eif_layer_maxpool2d(const eif_layer_t* layer, const float32_t* input, float32_t* output,
                                int in_h, int in_w, int in_c, int* out_h, int* out_w) {
    int k_h = layer->params.maxpool2d.pool_h;
    int k_w = layer->params.maxpool2d.pool_w;
    int stride_h = layer->params.maxpool2d.stride_h;
    int stride_w = layer->params.maxpool2d.stride_w;
    int o_h = (in_h - k_h) / stride_h + 1;
    int o_w = (in_w - k_w) / stride_w + 1;
    *out_h = o_h; *out_w = o_w;
    for (int c = 0; c < in_c; c++) {
        for (int y = 0; y < o_h; y++) {
            for (int x = 0; x < o_w; x++) {
                float32_t max_val = -INFINITY;
                for (int ky = 0; ky < k_h; ky++) {
                    for (int kx = 0; kx < k_w; kx++) {
                        int in_y = y * stride_h + ky;
                        int in_x = x * stride_w + kx;
                        float32_t val = input[(in_y * in_w + in_x) * in_c + c];
                        if (val > max_val) max_val = val;
                    }
                }
                output[(y * o_w + x) * in_c + c] = max_val;
            }
        }
    }
}

void eif_layer_avgpool2d(const eif_layer_t* layer, const float32_t* input, float32_t* output,
                                int in_h, int in_w, int in_c, int* out_h, int* out_w) {
    int k_h = layer->params.avgpool2d.pool_h;
    int k_w = layer->params.avgpool2d.pool_w;
    int stride_h = layer->params.avgpool2d.stride_h;
    int stride_w = layer->params.avgpool2d.stride_w;
    int o_h = (in_h - k_h) / stride_h + 1;
    int o_w = (in_w - k_w) / stride_w + 1;
    *out_h = o_h; *out_w = o_w;
    float count = (float)(k_h * k_w);
    for (int c = 0; c < in_c; c++) {
        for (int y = 0; y < o_h; y++) {
            for (int x = 0; x < o_w; x++) {
                float32_t sum = 0.0f;
                for (int ky = 0; ky < k_h; ky++) {
                    for (int kx = 0; kx < k_w; kx++) {
                        int in_y = y * stride_h + ky;
                        int in_x = x * stride_w + kx;
                        sum += input[(in_y * in_w + in_x) * in_c + c];
                    }
                }
                output[(y * o_w + x) * in_c + c] = sum / count;
            }
        }
    }
}

void eif_layer_global_avgpool2d(const float32_t* input, float32_t* output, int in_h, int in_w, int in_c) {
    float count = (float)(in_h * in_w);
    for (int c = 0; c < in_c; c++) {
        float32_t sum = 0.0f;
        for (int y = 0; y < in_h; y++) {
            for (int x = 0; x < in_w; x++) {
                sum += input[(y * in_w + x) * in_c + c];
            }
        }
        output[c] = sum / count;
    }
}

// ============================================================================
// Element-wise Operations
// ============================================================================

void eif_layer_add(const float32_t* input1, const float32_t* input2, float32_t* output, int size) {
    #pragma omp parallel for
    for (int i = 0; i < size; i++) {
        output[i] = input1[i] + input2[i];
    }
}

void eif_layer_multiply(const float32_t* input1, const float32_t* input2, float32_t* output, int size) {
    #pragma omp parallel for
    for (int i = 0; i < size; i++) {
        output[i] = input1[i] * input2[i];
    }
}

void eif_layer_leaky_relu(const eif_layer_t* layer, const float32_t* input, float32_t* output, int size) {
    float32_t alpha = layer->params.leaky_relu.alpha;
    #pragma omp parallel for
    for (int i = 0; i < size; i++) {
        float32_t val = input[i];
        output[i] = (val >= 0) ? val : val * alpha;
    }
}

void eif_layer_gelu(const float32_t* input, float32_t* output, int size) {
    const float32_t COEF_A = 0.7978845608f; // sqrt(2/pi)
    const float32_t COEF_B = 0.044715f;
    
    #pragma omp parallel for
    for (int i = 0; i < size; i++) {
        float32_t x = input[i];
        float32_t cdf = 0.5f * (1.0f + tanhf(COEF_A * (x + COEF_B * x * x * x)));
        output[i] = x * cdf;
    }
}

void eif_layer_hard_swish(const float32_t* input, float32_t* output, int size) {
    #pragma omp parallel for
    for (int i = 0; i < size; i++) {
        float32_t x = input[i];
        // h-swish: x * ReLU6(x + 3) / 6
        float32_t relu6_val = x + 3.0f;
        if (relu6_val < 0.0f) relu6_val = 0.0f;
        if (relu6_val > 6.0f) relu6_val = 6.0f;
        output[i] = x * relu6_val * 0.16666667f; // 1/6
    }
}

eif_status_t eif_layer_batch_norm(const float32_t* input, float32_t* output, const eif_tensor_shape_t* shape, const eif_layer_param_t* param, const float32_t* mean, const float32_t* var, const float32_t* gamma, const float32_t* beta) {
    if (!input || !output || !shape || !mean || !var || !gamma || !beta) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Assuming NHWC format, C is the last dimension.
    
    int batch = shape->dim[0];
    int height = shape->dim[1];
    int width = shape->dim[2];
    int channels = shape->dim[3];
    
    float32_t epsilon = 1e-5f;
    if (param) {
        epsilon = param->batch_norm.epsilon;
    }
    
    int total_elements = batch * height * width * channels;
    
    for (int i = 0; i < total_elements; i++) {
        int c = i % channels; // Channel index
        
        float32_t x = input[i];
        float32_t mu = mean[c];
        float32_t sigma2 = var[c];
        float32_t g = gamma[c];
        float32_t b = beta[c];
        
        // y = gamma * (x - mean) / sqrt(var + eps) + beta
        output[i] = g * (x - mu) / sqrtf(sigma2 + epsilon) + b;
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_layer_resize(const float32_t* input, float32_t* output, const eif_tensor_shape_t* in_shape, const eif_tensor_shape_t* out_shape, const eif_layer_param_t* param) {
    if (!input || !output || !in_shape || !out_shape || !param) return EIF_STATUS_INVALID_ARGUMENT;
    
    int batch = in_shape->dim[0];
    int in_h = in_shape->dim[1];
    int in_w = in_shape->dim[2];
    int channels = in_shape->dim[3];
    
    int out_h = out_shape->dim[1];
    int out_w = out_shape->dim[2];
    
    // Nearest Neighbor (Method 0)
    if (param->resize.method == 0) {
        float32_t scale_h = (float32_t)in_h / out_h;
        float32_t scale_w = (float32_t)in_w / out_w;
        
        for (int b = 0; b < batch; b++) {
            for (int y = 0; y < out_h; y++) {
                int in_y = (int)(y * scale_h);
                if (in_y >= in_h) in_y = in_h - 1;
                
                for (int x = 0; x < out_w; x++) {
                    int in_x = (int)(x * scale_w);
                    if (in_x >= in_w) in_x = in_w - 1;
                    
                    for (int c = 0; c < channels; c++) {
                        int out_idx = ((b * out_h + y) * out_w + x) * channels + c;
                        int in_idx = ((b * in_h + in_y) * in_w + in_x) * channels + c;
                        output[out_idx] = input[in_idx];
                    }
                }
            }
        }
    } else {
        // Bilinear (Method 1) - Placeholder: Fallback to Nearest for now or implement later
        // Implementing simple Bilinear
        float32_t scale_h = (float32_t)in_h / out_h;
        float32_t scale_w = (float32_t)in_w / out_w;
        
        for (int b = 0; b < batch; b++) {
            for (int y = 0; y < out_h; y++) {
                float32_t in_y_f = y * scale_h;
                int y0 = (int)in_y_f;
                int y1 = y0 + 1;
                if (y1 >= in_h) y1 = in_h - 1;
                float32_t dy = in_y_f - y0;
                
                for (int x = 0; x < out_w; x++) {
                    float32_t in_x_f = x * scale_w;
                    int x0 = (int)in_x_f;
                    int x1 = x0 + 1;
                    if (x1 >= in_w) x1 = in_w - 1;
                    float32_t dx = in_x_f - x0;
                    
                    for (int c = 0; c < channels; c++) {
                        int idx00 = ((b * in_h + y0) * in_w + x0) * channels + c;
                        int idx01 = ((b * in_h + y0) * in_w + x1) * channels + c;
                        int idx10 = ((b * in_h + y1) * in_w + x0) * channels + c;
                        int idx11 = ((b * in_h + y1) * in_w + x1) * channels + c;
                        
                        float32_t val00 = input[idx00];
                        float32_t val01 = input[idx01];
                        float32_t val10 = input[idx10];
                        float32_t val11 = input[idx11];
                        
                        float32_t top = val00 * (1.0f - dx) + val01 * dx;
                        float32_t bottom = val10 * (1.0f - dx) + val11 * dx;
                        
                        float32_t val = top * (1.0f - dy) + bottom * dy;
                        
                        int out_idx = ((b * out_h + y) * out_w + x) * channels + c;
                        output[out_idx] = val;
                    }
                }
            }
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_layer_dropout(const float32_t* input, float32_t* output, const eif_tensor_shape_t* shape, const eif_layer_param_t* param) {
    if (!input || !output || !shape) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Dropout is identity during inference
    int total_elements = 1;
    for (int i = 0; i < 4; i++) total_elements *= shape->dim[i];
    
    memcpy(output, input, total_elements * sizeof(float32_t));
    
    return EIF_STATUS_OK;
}

void eif_layer_layer_norm(const eif_layer_t* layer, const float32_t* input, float32_t* output, int h, int w, int c) {
    float epsilon = layer->params.layer_norm.epsilon;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float32_t sum = 0.0f;
            float32_t sum_sq = 0.0f;
            int idx_base = (y * w + x) * c;
            for (int k = 0; k < c; k++) {
                float32_t val = input[idx_base + k];
                sum += val;
                sum_sq += val * val;
            }
            float32_t mean = sum / c;
            float32_t variance = (sum_sq / c) - (mean * mean);
            float32_t std_dev = sqrtf(variance + epsilon);
            for (int k = 0; k < c; k++) {
                output[idx_base + k] = (input[idx_base + k] - mean) / std_dev;
            }
        }
    }
}

void eif_layer_conv1d(const eif_layer_t* layer, const float32_t* input, float32_t* output, int in_w, int in_c, int* out_w, int* out_c) {
    int filters = layer->params.conv1d.filters;
    int kernel_size = layer->params.conv1d.kernel_size;
    int stride = layer->params.conv1d.stride;
    int pad = layer->params.conv1d.pad;
    
    // Calculate output width
    // W_out = (W_in - K + 2P) / S + 1
    int output_width = (in_w - kernel_size + 2 * pad) / stride + 1;
    
    if (out_w) *out_w = output_width;
    if (out_c) *out_c = filters;
    
    const float32_t* weights = (const float32_t*)layer->weights; // Shape: [filters, kernel_size, in_c]
    const float32_t* biases = (const float32_t*)layer->biases;   // Shape: [filters]
    
    for (int w = 0; w < output_width; w++) {
        for (int f = 0; f < filters; f++) {
            float32_t sum = 0.0f;
            if (biases) sum = biases[f];
            
            int in_start_w = w * stride - pad;
            
            for (int k = 0; k < kernel_size; k++) {
                int in_idx_w = in_start_w + k;
                
                if (in_idx_w >= 0 && in_idx_w < in_w) {
                    for (int c = 0; c < in_c; c++) {
                        float32_t val = input[in_idx_w * in_c + c];
                        // Weights layout: [filters, kernel_size, in_c]
                        // Index: f * (kernel_size * in_c) + k * in_c + c
                        float32_t weight = weights[f * (kernel_size * in_c) + k * in_c + c];
                        sum += val * weight;
                    }
                }
            }
            
            // Activation
            if (layer->activation == EIF_ACT_RELU) {
                if (sum < 0) sum = 0;
            } else if (layer->activation == EIF_ACT_RELU6) {
                if (sum < 0) sum = 0;
                if (sum > 6) sum = 6;
            }
            
            output[w * filters + f] = sum;
        }
    }
}

void eif_layer_transpose_conv2d(const eif_layer_t* layer, const float32_t* input, float32_t* output, int in_h, int in_w, int in_c, int* out_h, int* out_w, int* out_c) {
    int filters = layer->params.transpose_conv2d.filters;
    int k_h = layer->params.transpose_conv2d.kernel_h;
    int k_w = layer->params.transpose_conv2d.kernel_w;
    int stride_h = layer->params.transpose_conv2d.stride_h;
    int stride_w = layer->params.transpose_conv2d.stride_w;
    int pad_h = layer->params.transpose_conv2d.pad_h;
    int pad_w = layer->params.transpose_conv2d.pad_w;
    
    // Output dimensions for Transpose Conv (Deconvolution)
    // H_out = (H_in - 1) * stride + K - 2*pad
    int o_h = (in_h - 1) * stride_h + k_h - 2 * pad_h;
    int o_w = (in_w - 1) * stride_w + k_w - 2 * pad_w;
    
    if (out_h) *out_h = o_h;
    if (out_w) *out_w = o_w;
    if (out_c) *out_c = filters;
    
    // Initialize output to 0 (accumulation)
    memset(output, 0, o_h * o_w * filters * sizeof(float32_t));
    
    const float32_t* weights = (const float32_t*)layer->weights; // Shape: [filters, k_h, k_w, in_c]
    const float32_t* biases = (const float32_t*)layer->biases;
    
    // Add biases first
    if (biases) {
        for (int y = 0; y < o_h; y++) {
            for (int x = 0; x < o_w; x++) {
                for (int f = 0; f < filters; f++) {
                    output[(y * o_w + x) * filters + f] = biases[f];
                }
            }
        }
    }
    
    // Transpose Convolution Loop
    for (int y = 0; y < in_h; y++) {
        for (int x = 0; x < in_w; x++) {
            for (int c = 0; c < in_c; c++) {
                float32_t val = input[(y * in_w + x) * in_c + c];
                
                // Distribute val to output region
                int out_y_start = y * stride_h - pad_h;
                int out_x_start = x * stride_w - pad_w;
                
                for (int ky = 0; ky < k_h; ky++) {
                    for (int kx = 0; kx < k_w; kx++) {
                        int out_y = out_y_start + ky;
                        int out_x = out_x_start + kx;
                        
                        if (out_y >= 0 && out_y < o_h && out_x >= 0 && out_x < o_w) {
                            for (int f = 0; f < filters; f++) {
                                // Weight index: f, ky, kx, c
                                float32_t w = weights[((f * k_h + ky) * k_w + kx) * in_c + c];
                                output[(out_y * o_w + out_x) * filters + f] += val * w;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Activation
    int total_size = o_h * o_w * filters;
    if (layer->activation == EIF_ACT_RELU) {
        for (int i = 0; i < total_size; i++) {
            if (output[i] < 0) output[i] = 0;
        }
    } else if (layer->activation == EIF_ACT_RELU6) {
        for (int i = 0; i < total_size; i++) {
            if (output[i] < 0) output[i] = 0;
            if (output[i] > 6) output[i] = 6;
        }
    }
}

// Quantize/Dequantize moved to eif_dl_quantization.c

// --- Advanced Layers ---

eif_status_t eif_layer_attention(const float32_t* query, const float32_t* key, const float32_t* value, float32_t* output, int batch, int seq_len, int embed_dim) {
    if (!query || !key || !value || !output) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Output = Softmax(Q * K^T / sqrt(d)) * V
    // Dimensions:
    // Q: [B, S, E]
    // K: [B, S, E]
    // V: [B, S, E]
    // Scores: [B, S, S]
    
    // We need a scratch buffer for scores.
    // Ideally passed in or allocated from pool.
    // Since this is internal API, we assume caller handles memory or we use stack if small.
    // But S*S can be large.
    // Let's assume for now S is small (e.g. < 64) or we fail.
    // Or better, we just implement a naive loop without full matrix materialization if possible?
    // No, Softmax needs the full row.
    
    // Let's allocate on stack for small S, or error.
    // MAX_SEQ_LEN = 128
    #define MAX_SEQ_LEN 128
    if (seq_len > MAX_SEQ_LEN) return EIF_STATUS_NOT_IMPLEMENTED; // Too big for stack
    
    float32_t scores[MAX_SEQ_LEN]; 
    float32_t scale = 1.0f / sqrtf((float32_t)embed_dim);
    
    for (int b = 0; b < batch; b++) {
        for (int i = 0; i < seq_len; i++) { // For each query vector
            // 1. Calculate Scores (Q[i] dot K[j])
            for (int j = 0; j < seq_len; j++) {
                float32_t dot = 0;
                for (int k = 0; k < embed_dim; k++) {
                    int q_idx = b * seq_len * embed_dim + i * embed_dim + k;
                    int k_idx = b * seq_len * embed_dim + j * embed_dim + k;
                    dot += query[q_idx] * key[k_idx];
                }
                scores[j] = dot * scale;
            }
            
            // 2. Softmax
            eif_layer_softmax(scores, scores, seq_len);
            
            // 3. Multiply by V (Weighted Sum)
            for (int k = 0; k < embed_dim; k++) {
                float32_t sum = 0;
                for (int j = 0; j < seq_len; j++) {
                    int v_idx = b * seq_len * embed_dim + j * embed_dim + k;
                    sum += scores[j] * value[v_idx];
                }
                int out_idx = b * seq_len * embed_dim + i * embed_dim + k;
                output[out_idx] = sum;
            }
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_layer_embedding(const float32_t* input, float32_t* output, const float32_t* weights, int batch, int seq_len, int vocab_size, int embed_dim) {
    if (!input || !output || !weights) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Input contains indices (as floats, usually)
    int total_tokens = batch * seq_len;
    
    for (int i = 0; i < total_tokens; i++) {
        int idx = (int)input[i];
        if (idx < 0 || idx >= vocab_size) return EIF_STATUS_ERROR; // Index out of bounds
        
        // Copy embedding vector
        const float32_t* embed_vec = &weights[idx * embed_dim];
        float32_t* out_vec = &output[i * embed_dim];
        
        memcpy(out_vec, embed_vec, embed_dim * sizeof(float32_t));
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Fixed-Point (Int8) Kernels
// ============================================================================

#include "eif_fixedpoint.h"

// Simple Int8 Dense Layer (Reference Implementation)
// Assumes weights are [output_channels, input_channels]
// Assumes input is [input_channels]
// Assumes output is [output_channels]
void eif_layer_dense_q7(const eif_layer_t* layer, const q7_t* input, q7_t* output, int input_size) {
    int units = layer->params.dense.units;
    const q7_t* weights = (const q7_t*)layer->weights;
    const q31_t* biases = (const q31_t*)layer->biases; // Biases are typically int32 in quantized models
    
    int32_t input_offset = layer->quant_params.input_offset;
    int32_t output_offset = layer->quant_params.output_offset;
    int32_t act_min = layer->quant_params.quantized_activation_min;
    int32_t act_max = layer->quant_params.quantized_activation_max;

    for (int i = 0; i < units; i++) {
        q31_t acc = 0;
        if (biases) acc = biases[i];
        
        const q7_t* weight_row = weights + i * input_size;
        
        // Optimized loop
        if (input_offset == 0) {
            for (int j = 0; j < input_size; j++) {
                acc += (int32_t)input[j] * weight_row[j];
            }
        } else {
            for (int j = 0; j < input_size; j++) {
                acc += ((int32_t)input[j] + input_offset) * weight_row[j];
            }
        }
        
        // Requantize (Simplified)
        acc += output_offset;
        if (acc > act_max) acc = act_max;
        if (acc < act_min) acc = act_min;
        
        output[i] = (q7_t)acc;
    }
}

// Simple Int8 Conv2D Layer (Reference Implementation)
void eif_layer_conv2d_q7(const eif_layer_t* layer, const q7_t* input, q7_t* output, 
                             int in_h, int in_w, int in_c, int* out_h, int* out_w, int* out_c) {
    int filters = layer->params.conv2d.filters;
    int k_h = layer->params.conv2d.kernel_h;
    int k_w = layer->params.conv2d.kernel_w;
    int stride_h = layer->params.conv2d.stride_h;
    int stride_w = layer->params.conv2d.stride_w;
    int o_h = (in_h - k_h) / stride_h + 1;
    int o_w = (in_w - k_w) / stride_w + 1;
    *out_h = o_h; *out_w = o_w; *out_c = filters;

    const q7_t* weights = (const q7_t*)layer->weights;
    const q31_t* biases = (const q31_t*)layer->biases;
    
    int32_t input_offset = layer->quant_params.input_offset;
    int32_t output_offset = layer->quant_params.output_offset;
    int32_t act_min = layer->quant_params.quantized_activation_min;
    int32_t act_max = layer->quant_params.quantized_activation_max;

    for (int f = 0; f < filters; f++) {
        for (int y = 0; y < o_h; y++) {
            for (int x = 0; x < o_w; x++) {
                q31_t acc = 0;
                if (biases) acc = biases[f];
                
                for (int ky = 0; ky < k_h; ky++) {
                    for (int kx = 0; kx < k_w; kx++) {
                        int in_y = y * stride_h + ky;
                        int in_x = x * stride_w + kx;
                        
                        // Pre-calculate row offset
                        const q7_t* input_row = input + (in_y * in_w + in_x) * in_c;
                        const q7_t* weight_row = weights + ((f * k_h + ky) * k_w + kx) * in_c;
                        
                        for (int c = 0; c < in_c; c++) {
                            q31_t input_val = (int32_t)input_row[c] + input_offset;
                            q31_t weight_val = (int32_t)weight_row[c];
                            acc += input_val * weight_val;
                        }
                    }
                }
                
                acc += output_offset;
                if (acc > act_max) acc = act_max;
                if (acc < act_min) acc = act_min;
                
                output[(y * o_w + x) * filters + f] = (q7_t)acc;
            }
        }
    }
}

// ============================================================================
// New Operators Implementation (Priority 1, 2, 3)
// ============================================================================

void eif_layer_sub(const float32_t* input1, const float32_t* input2, float32_t* output, int size) {
    #pragma omp parallel for
    for (int i = 0; i < size; i++) {
        output[i] = input1[i] - input2[i];
    }
}

void eif_layer_div(const float32_t* input1, const float32_t* input2, float32_t* output, int size) {
    #pragma omp parallel for
    for (int i = 0; i < size; i++) {
        output[i] = input1[i] / (input2[i] + 1e-9f); // Avoid division by zero
    }
}

void eif_layer_exp(const float32_t* input, float32_t* output, int size) {
    #pragma omp parallel for
    for (int i = 0; i < size; i++) {
        output[i] = expf(input[i]);
    }
}

void eif_layer_log(const float32_t* input, float32_t* output, int size) {
    #pragma omp parallel for
    for (int i = 0; i < size; i++) {
        output[i] = logf(input[i] > 1e-9f ? input[i] : 1e-9f);
    }
}

void eif_layer_sqrt(const float32_t* input, float32_t* output, int size) {
    #pragma omp parallel for
    for (int i = 0; i < size; i++) {
        output[i] = sqrtf(input[i] > 0 ? input[i] : 0);
    }
}

eif_status_t eif_layer_split(const float32_t* input, void** outputs, const eif_tensor_shape_t* in_shape, const eif_layer_param_t* param, int num_outputs) {
    if (!input || !outputs || !in_shape || !param) return EIF_STATUS_INVALID_ARGUMENT;
    
    int axis = param->split.axis;
    if (axis < 0) axis += 4; // Assuming 4 dims max
    
    // Calculate strides
    int outer_size = 1;
    for (int i = 0; i < axis; i++) outer_size *= in_shape->dim[i];
    
    int axis_dim = in_shape->dim[axis];
    
    int inner_size = 1;
    for (int i = axis + 1; i < 4; i++) inner_size *= in_shape->dim[i];
    
    // Assuming equal split for now if sizes not provided (simplified)
    // In a real implementation, we'd need a list of split sizes.
    // Here we assume axis_dim is divisible by num_outputs
    int split_size = axis_dim / num_outputs;
    
    for (int i = 0; i < outer_size; i++) {
        for (int n = 0; n < num_outputs; n++) {
            float32_t* out_ptr = (float32_t*)outputs[n];
            const float32_t* in_ptr = input + i * axis_dim * inner_size + n * split_size * inner_size;
            
            // Copy chunk
            memcpy(out_ptr + i * split_size * inner_size, in_ptr, split_size * inner_size * sizeof(float32_t));
        }
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_layer_pad(const float32_t* input, float32_t* output, const eif_tensor_shape_t* in_shape, const eif_tensor_shape_t* out_shape, const eif_layer_param_t* param) {
    // Simplified 4D padding
    // pads: [top, bottom, left, right] usually for H and W
    // We assume N and C are not padded for this basic implementation
    
    int N = in_shape->dim[0];
    int H = in_shape->dim[1];
    int W = in_shape->dim[2];
    int C = in_shape->dim[3];
    
    int pad_t = param->pad.pads[0];
    int pad_b = param->pad.pads[1];
    int pad_l = param->pad.pads[2];
    int pad_r = param->pad.pads[3];
    
    int out_H = H + pad_t + pad_b;
    int out_W = W + pad_l + pad_r;
    
    float32_t pad_val = param->pad.constant_value;
    
    // Fill with pad value first
    int total_out = N * out_H * out_W * C;
    for(int i=0; i<total_out; i++) output[i] = pad_val;
    
    // Copy input
    for (int n = 0; n < N; n++) {
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                const float32_t* src = input + ((n * H + y) * W + x) * C;
                float32_t* dst = output + ((n * out_H + (y + pad_t)) * out_W + (x + pad_l)) * C;
                memcpy(dst, src, C * sizeof(float32_t));
            }
        }
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_layer_gather(const float32_t* input, const float32_t* indices, float32_t* output, const eif_tensor_shape_t* in_shape, const eif_tensor_shape_t* out_shape, const eif_layer_param_t* param) {
    // Gather slices from input along axis according to indices
    int axis = param->gather.axis;
    
    // Simplified: Axis 0 (Batch/Row) Gather
    // Input: [DataSize, Dim...]
    // Indices: [NumIndices]
    // Output: [NumIndices, Dim...]
    
    if (axis != 0) return EIF_STATUS_NOT_IMPLEMENTED; // Only axis 0 supported for now
    
    int num_indices = out_shape->dim[0]; // Output batch size matches indices count
    int slice_size = 1;
    for (int i = 1; i < 4; i++) slice_size *= in_shape->dim[i];
    
    for (int i = 0; i < num_indices; i++) {
        int idx = (int)indices[i];
        if (idx < 0 || idx >= in_shape->dim[0]) return EIF_STATUS_ERROR;
        
        memcpy(output + i * slice_size, input + idx * slice_size, slice_size * sizeof(float32_t));
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_layer_matmul(const float32_t* a, const float32_t* b, float32_t* output, int M, int K, int N) {
    // C = A * B
    // A: [M, K]
    // B: [K, N]
    // C: [M, N]
    
    #pragma omp parallel for
    for (int m = 0; m < M; m++) {
        for (int n = 0; n < N; n++) {
            float32_t sum = 0.0f;
            for (int k = 0; k < K; k++) {
                sum += a[m * K + k] * b[k * N + n];
            }
            output[m * N + n] = sum;
        }
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_layer_reduce_mean(const float32_t* input, float32_t* output, const eif_tensor_shape_t* in_shape, const eif_tensor_shape_t* out_shape, const eif_layer_param_t* param) {
    // Simplified: Reduce along axis 1 and 2 (Spatial) -> Global Average Pooling equivalent
    // Or generic axis reduction.
    // Let's implement generic reduction for a single axis.
    
    int axis = param->reduce.axis;
    int outer = 1;
    for(int i=0; i<axis; i++) outer *= in_shape->dim[i];
    int dim = in_shape->dim[axis];
    int inner = 1;
    for(int i=axis+1; i<4; i++) inner *= in_shape->dim[i];
    
    for (int o = 0; o < outer; o++) {
        for (int i = 0; i < inner; i++) {
            float32_t sum = 0.0f;
            for (int d = 0; d < dim; d++) {
                sum += input[o * dim * inner + d * inner + i];
            }
            output[o * inner + i] = sum / dim;
        }
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_layer_reduce_sum(const float32_t* input, float32_t* output, const eif_tensor_shape_t* in_shape, const eif_tensor_shape_t* out_shape, const eif_layer_param_t* param) {
    int axis = param->reduce.axis;
    int outer = 1;
    for(int i=0; i<axis; i++) outer *= in_shape->dim[i];
    int dim = in_shape->dim[axis];
    int inner = 1;
    for(int i=axis+1; i<4; i++) inner *= in_shape->dim[i];
    
    for (int o = 0; o < outer; o++) {
        for (int i = 0; i < inner; i++) {
            float32_t sum = 0.0f;
            for (int d = 0; d < dim; d++) {
                sum += input[o * dim * inner + d * inner + i];
            }
            output[o * inner + i] = sum;
        }
    }
    return EIF_STATUS_OK;
}

// Helper struct for TopK
typedef struct {
    float32_t val;
    int idx;
} eif_val_idx_t;

int compare_val_idx(const void* a, const void* b) {
    float32_t diff = ((eif_val_idx_t*)b)->val - ((eif_val_idx_t*)a)->val; // Descending
    return (diff > 0) - (diff < 0);
}

eif_status_t eif_layer_topk(const float32_t* input, float32_t* values, float32_t* indices, const eif_tensor_shape_t* in_shape, const eif_layer_param_t* param) {
    // TopK along last axis
    int k = param->topk.k;
    int last_dim = in_shape->dim[3]; // Assuming NHWC and reducing C, or generic last dim
    // Actually let's assume flattened last dim logic
    int outer_size = 1;
    for(int i=0; i<3; i++) outer_size *= in_shape->dim[i];
    
    // We need a buffer for sorting
    // Since we can't alloc, we assume last_dim is small or use stack
    #define MAX_TOPK_DIM 1024
    if (last_dim > MAX_TOPK_DIM) return EIF_STATUS_NOT_IMPLEMENTED;
    
    eif_val_idx_t buffer[MAX_TOPK_DIM];
    
    for (int i = 0; i < outer_size; i++) {
        const float32_t* row = input + i * last_dim;
        for (int j = 0; j < last_dim; j++) {
            buffer[j].val = row[j];
            buffer[j].idx = j;
        }
        
        // Sort (qsort is slow but standard)
        qsort(buffer, last_dim, sizeof(eif_val_idx_t), compare_val_idx);
        
        for (int j = 0; j < k; j++) {
            values[i * k + j] = buffer[j].val;
            indices[i * k + j] = (float32_t)buffer[j].idx;
        }
    }
    return EIF_STATUS_OK;
}

void eif_layer_clip(const eif_layer_t* layer, const float32_t* input, float32_t* output, int size) {
    float min_val = layer->params.clip.min_val;
    float max_val = layer->params.clip.max_val;
    
    #pragma omp parallel for
    for(int i = 0; i < size; i++) {
        float val = input[i];
        if (val < min_val) val = min_val;
        if (val > max_val) val = max_val;
        output[i] = val;
    }
}

void eif_layer_flatten(const float32_t* input, float32_t* output, int size) {
    if (input != output) {
        memcpy(output, input, size * sizeof(float32_t));
    }
}

void eif_layer_reshape(const float32_t* input, float32_t* output, int size) {
    if (input != output) {
        memcpy(output, input, size * sizeof(float32_t));
    }
}

eif_status_t eif_layer_argmax(const float32_t* input, float32_t* output, const eif_tensor_shape_t* in_shape, const eif_layer_param_t* param) {
    int axis = param->argmax.axis;
    if (axis < 0) axis += 4;
    
    int outer = 1;
    for(int i=0; i<axis; i++) outer *= in_shape->dim[i];
    int dim = in_shape->dim[axis];
    int inner = 1;
    for(int i=axis+1; i<4; i++) inner *= in_shape->dim[i];
    
    for (int o = 0; o < outer; o++) {
        for (int i = 0; i < inner; i++) {
            float32_t max_val = -FLT_MAX;
            int max_idx = 0;
            for (int d = 0; d < dim; d++) {
                float32_t val = input[o * dim * inner + d * inner + i];
                if (val > max_val) {
                    max_val = val;
                    max_idx = d;
                }
            }
            output[o * inner + i] = (float32_t)max_idx;
        }
    }
    return EIF_STATUS_OK;
}

void eif_layer_minimum(const float32_t* input1, const float32_t* input2, float32_t* output, int size) {
    for (int i = 0; i < size; i++) {
        output[i] = input1[i] < input2[i] ? input1[i] : input2[i];
    }
}

void eif_layer_maximum(const float32_t* input1, const float32_t* input2, float32_t* output, int size) {
    for (int i = 0; i < size; i++) {
        output[i] = input1[i] > input2[i] ? input1[i] : input2[i];
    }
}
