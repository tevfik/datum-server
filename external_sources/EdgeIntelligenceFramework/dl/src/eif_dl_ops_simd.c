#include "eif_dl_internal.h"
#include <immintrin.h>
#include <string.h>

// Check for AVX2 support
#if defined(__AVX2__)
    #define EIF_USE_AVX2
#endif

void eif_conv2d_simd(const eif_layer_t* layer, const float32_t* input, float32_t* output, 
                     int in_h, int in_w, int in_c, int out_h, int out_w, int out_c) {
    
    int k_h = layer->params.conv2d.kernel_h;
    int k_w = layer->params.conv2d.kernel_w;
    int stride_h = layer->params.conv2d.stride_h;
    int stride_w = layer->params.conv2d.stride_w;
    
    const float32_t* weights = (const float32_t*)layer->weights;
    const float32_t* biases = (const float32_t*)layer->biases;
    
#if defined(EIF_USE_AVX2)
    // Vectorize over C strategy for each filter.
    // This is easier with the current weight layout.
    
    for (int f = 0; f < out_c; f++) {
        for (int y = 0; y < out_h; y++) {
            for (int x = 0; x < out_w; x++) {
                float32_t sum = 0.0f;
                if (biases) sum = biases[f];
                
                // Accumulator vector for inner loop
                __m256 vsum = _mm256_setzero_ps();
                
                for (int ky = 0; ky < k_h; ky++) {
                    for (int kx = 0; kx < k_w; kx++) {
                        int in_y = y * stride_h + ky;
                        int in_x = x * stride_w + kx;
                        int in_idx_base = (in_y * in_w + in_x) * in_c;
                        int w_idx_base = ((f * k_h + ky) * k_w + kx) * in_c;
                        
                        int c = 0;
                        for (; c <= in_c - 8; c += 8) {
                            __m256 v_in = _mm256_loadu_ps(&input[in_idx_base + c]);
                            __m256 v_w = _mm256_loadu_ps(&weights[w_idx_base + c]);
                            vsum = _mm256_fmadd_ps(v_in, v_w, vsum);
                        }
                        
                        // Handle remaining channels
                        for (; c < in_c; c++) {
                            sum += input[in_idx_base + c] * weights[w_idx_base + c];
                        }
                    }
                }
                
                // Reduce vsum
                float32_t temp[8];
                _mm256_storeu_ps(temp, vsum);
                for (int i = 0; i < 8; i++) sum += temp[i];
                
                output[(y * out_w + x) * out_c + f] = sum;
            }
        }
    }

#else
    // Fallback to scalar (reference implementation)
    // ...
    // Actually, we should call the original function or implement scalar here.
    // Since this function is "simd", we can just assume caller won't call it if not supported?
    // Or we implement scalar fallback.
    // Let's implement scalar fallback to be safe.
    
    for (int f = 0; f < out_c; f++) {
        for (int y = 0; y < out_h; y++) {
            for (int x = 0; x < out_w; x++) {
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
                output[(y * out_w + x) * out_c + f] = sum;
            }
        }
    }
#endif
}
