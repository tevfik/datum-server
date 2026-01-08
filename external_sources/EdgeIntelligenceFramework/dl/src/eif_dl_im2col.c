#include "eif_dl_internal.h"
#include <stdlib.h>
#include <string.h>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

// Helper: Tiled Im2Col
// Converts a portion of the input image (corresponding to 'y_count' output rows) into a matrix
// Input: [H, W, C]
// Output: [y_count * O_W, K_H * K_W * C]
void eif_im2col_tiled(const float32_t* input, float32_t* col_buffer,
                int in_h, int in_w, int in_c,
                int k_h, int k_w,
                int stride_h, int stride_w,
                int out_w,
                int y_start, int y_count) {
    
    int col_idx = 0;
    for (int y = y_start; y < y_start + y_count; y++) {
        for (int x = 0; x < out_w; x++) {
            // For each output pixel, gather the kernel window
            for (int ky = 0; ky < k_h; ky++) {
                for (int kx = 0; kx < k_w; kx++) {
                    int in_y = y * stride_h + ky;
                    int in_x = x * stride_w + kx;
                    
                    // Copy all channels
                    // Optimization: memcpy for contiguous channels
                    const float32_t* src = input + (in_y * in_w + in_x) * in_c;
                    memcpy(&col_buffer[col_idx], src, in_c * sizeof(float32_t));
                    col_idx += in_c;
                }
            }
        }
    }
}

// Helper: GEMM (General Matrix Multiply)
// C = A * B^T (Since weights are typically stored as [Filters, K])
// A: [M, K]
// B: [N, K] (Weights)
// C: [M, N]
void eif_gemm(const float32_t* A, const float32_t* B, float32_t* C, const float32_t* bias,
              int M, int N, int K) {
    
#if defined(__AVX2__)
    // AVX2 Optimized GEMM
    // Blocked loop to maximize register usage
    // We process 1 row of A (m) and 8 rows of B (n) at a time.
    // C[m, n...n+7] = Dot(A[m, :], B[n...n+7, :])
    
    int n_block = 0;
    for (; n_block <= N - 8; n_block += 8) {
        for (int m = 0; m < M; m++) {
            // Initialize accumulators with bias
            __m256 sum = _mm256_setzero_ps();
            if (bias) {
                sum = _mm256_loadu_ps(&bias[n_block]);
            }
            
            // Since B is [N, K] (Row-Major), B[n, k] and B[n+1, k] are far apart.
            // We cannot load B[n...n+7, k] contiguously.
            // We have to load A[m, k] (scalar) and multiply by B[n...n+7, k] (vector)? NO.
            // B[n...n+7, k] is NOT a vector in memory.
            
            // Correct Strategy for B = [N, K]:
            // We want to compute 8 dot products:
            // C[m, n+0] = A[m,:] . B[n+0,:]
            // ...
            // C[m, n+7] = A[m,:] . B[n+7,:]
            
            // We can iterate k.
            // Load A[m, k] -> Broadcast to vector.
            // Load B[n+0, k], B[n+1, k]... -> This is slow (gather).
            
            // Alternative:
            // If we process 8 columns of K at a time?
            // A[m, k...k+7] is a vector.
            // B[n, k...k+7] is a vector.
            // This is perfect!
            
            // We need 8 accumulators (one for each n in the block).
            __m256 acc0 = _mm256_setzero_ps();
            __m256 acc1 = _mm256_setzero_ps();
            __m256 acc2 = _mm256_setzero_ps();
            __m256 acc3 = _mm256_setzero_ps();
            __m256 acc4 = _mm256_setzero_ps();
            __m256 acc5 = _mm256_setzero_ps();
            __m256 acc6 = _mm256_setzero_ps();
            __m256 acc7 = _mm256_setzero_ps();
            
            int k = 0;
            for (; k <= K - 8; k += 8) {
                __m256 a_vec = _mm256_loadu_ps(&A[m * K + k]);
                
                acc0 = _mm256_fmadd_ps(a_vec, _mm256_loadu_ps(&B[(n_block + 0) * K + k]), acc0);
                acc1 = _mm256_fmadd_ps(a_vec, _mm256_loadu_ps(&B[(n_block + 1) * K + k]), acc1);
                acc2 = _mm256_fmadd_ps(a_vec, _mm256_loadu_ps(&B[(n_block + 2) * K + k]), acc2);
                acc3 = _mm256_fmadd_ps(a_vec, _mm256_loadu_ps(&B[(n_block + 3) * K + k]), acc3);
                acc4 = _mm256_fmadd_ps(a_vec, _mm256_loadu_ps(&B[(n_block + 4) * K + k]), acc4);
                acc5 = _mm256_fmadd_ps(a_vec, _mm256_loadu_ps(&B[(n_block + 5) * K + k]), acc5);
                acc6 = _mm256_fmadd_ps(a_vec, _mm256_loadu_ps(&B[(n_block + 6) * K + k]), acc6);
                acc7 = _mm256_fmadd_ps(a_vec, _mm256_loadu_ps(&B[(n_block + 7) * K + k]), acc7);
            }
            
            // Horizontal reduction of accumulators
            // Each acc contains 8 partial sums for one filter. We need to sum them up.
            
            float res[8];
            
            // Helper macro for horizontal sum of __m256
            #define H_SUM(vec) \
                { \
                    __m128 hi = _mm256_extractf128_ps(vec, 1); \
                    __m128 lo = _mm256_castps256_ps128(vec); \
                    __m128 sum = _mm_add_ps(lo, hi); \
                    sum = _mm_hadd_ps(sum, sum); \
                    sum = _mm_hadd_ps(sum, sum); \
                    _mm_store_ss(&res[i], sum); \
                }

            // Unroll reduction manually
            int i = 0; H_SUM(acc0); C[m * N + n_block + 0] = res[0] + (bias ? bias[n_block + 0] : 0);
            i = 1; H_SUM(acc1); C[m * N + n_block + 1] = res[1] + (bias ? bias[n_block + 1] : 0);
            i = 2; H_SUM(acc2); C[m * N + n_block + 2] = res[2] + (bias ? bias[n_block + 2] : 0);
            i = 3; H_SUM(acc3); C[m * N + n_block + 3] = res[3] + (bias ? bias[n_block + 3] : 0);
            i = 4; H_SUM(acc4); C[m * N + n_block + 4] = res[4] + (bias ? bias[n_block + 4] : 0);
            i = 5; H_SUM(acc5); C[m * N + n_block + 5] = res[5] + (bias ? bias[n_block + 5] : 0);
            i = 6; H_SUM(acc6); C[m * N + n_block + 6] = res[6] + (bias ? bias[n_block + 6] : 0);
            i = 7; H_SUM(acc7); C[m * N + n_block + 7] = res[7] + (bias ? bias[n_block + 7] : 0);
            
            // Handle remaining K
            for (; k < K; k++) {
                float a_val = A[m * K + k];
                for (int j = 0; j < 8; j++) {
                    C[m * N + n_block + j] += a_val * B[(n_block + j) * K + k];
                }
            }
        }
    }
    
    // Handle remaining N (filters)
    for (int n = n_block; n < N; n++) {
        for (int m = 0; m < M; m++) {
            float32_t sum = 0.0f;
            if (bias) sum = bias[n];
            for (int k = 0; k < K; k++) {
                sum += A[m * K + k] * B[n * K + k];
            }
            C[m * N + n] = sum;
        }
    }
#else
    // Scalar Fallback
    for (int m = 0; m < M; m++) {
        for (int n = 0; n < N; n++) {
            float32_t sum = 0.0f;
            if (bias) sum = bias[n];
            
            for (int k = 0; k < K; k++) {
                // A is [M, K] -> A[m*K + k]
                // B is [N, K] -> B[n*K + k] (Weights are OHWI, so Filter n is contiguous)
                sum += A[m * K + k] * B[n * K + k];
            }
            C[m * N + n] = sum;
        }
    }
#endif
}

// Im2Col based Convolution with Tiling
void eif_layer_conv2d_im2col(const eif_layer_t* layer, const float32_t* input, float32_t* output, 
                             int in_h, int in_w, int in_c, int* out_h, int* out_w, int* out_c) {
    int filters = layer->params.conv2d.filters;
    int k_h = layer->params.conv2d.kernel_h;
    int k_w = layer->params.conv2d.kernel_w;
    int stride_h = layer->params.conv2d.stride_h;
    int stride_w = layer->params.conv2d.stride_w;
    int o_h = (in_h - k_h) / stride_h + 1;
    int o_w = (in_w - k_w) / stride_w + 1;
    *out_h = o_h; *out_w = o_w; *out_c = filters;

    const float32_t* weights = (const float32_t*)layer->weights;
    const float32_t* biases = (const float32_t*)layer->biases;

    // Tiling Configuration
    // Process TILE_H output rows at a time to keep buffer size small
    // For 28x28 input, o_w=26. K=9.
    // If TILE_H=4, Buffer = 4 * 26 * 9 * 4 bytes = ~3.7 KB.
    // If TILE_H=1, Buffer = 1 * 26 * 9 * 4 bytes = ~936 bytes.
    // Let's use TILE_H = 4 for a balance of overhead vs memory.
    #define TILE_H 4

    int K = k_h * k_w * in_c;
    int N = filters;
    
    // Allocate buffer for ONE tile
    // Max size needed is TILE_H * o_w * K
    int max_tile_pixels = TILE_H * o_w;
    float32_t* col_buffer = (float32_t*)malloc(max_tile_pixels * K * sizeof(float32_t));
    if (!col_buffer) return; // Error handling

    for (int y = 0; y < o_h; y += TILE_H) {
        // Calculate actual height of this tile (handle last partial tile)
        int current_tile_h = (y + TILE_H > o_h) ? (o_h - y) : TILE_H;
        int current_M = current_tile_h * o_w;
        
        // 1. Im2Col for this tile
        eif_im2col_tiled(input, col_buffer, in_h, in_w, in_c, k_h, k_w, stride_h, stride_w, o_w, y, current_tile_h);
        
        // 2. GEMM for this tile
        // Output pointer needs to be offset to the correct row
        // Output shape is [o_h, o_w, filters] -> flattened [o_h * o_w * filters]
        // We are writing to rows y to y+current_tile_h
        float32_t* output_ptr = output + (y * o_w * filters);
        
        eif_gemm(col_buffer, weights, output_ptr, biases, current_M, N, K);
    }

    free(col_buffer);
}
