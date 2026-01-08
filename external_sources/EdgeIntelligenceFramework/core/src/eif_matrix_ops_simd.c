#include "eif_matrix.h"
#include <immintrin.h> // AVX
#include <string.h>

// Check for AVX2 support
#if defined(__AVX2__)
    #define EIF_USE_AVX2
#endif

// Check for NEON support
#if defined(__ARM_NEON)
    #include <arm_neon.h>
    #define EIF_USE_NEON
#endif

eif_status_t eif_matrix_mul_simd(const eif_matrix_t* A, const eif_matrix_t* B, eif_matrix_t* C) {
    if (!A || !B || !C) return EIF_STATUS_INVALID_ARGUMENT;
    if (A->cols != B->rows || A->rows != C->rows || B->cols != C->cols) return EIF_STATUS_INVALID_ARGUMENT;

    // Initialize C to 0
    memset(C->data, 0, C->rows * C->cols * sizeof(float32_t));

#if defined(EIF_USE_AVX2)
    // AVX2 Implementation (8 floats at a time)
    // C[i][j] += A[i][k] * B[k][j]
    // Optimized: Loop order i, k, j to access C[i][j] and B[k][j] contiguously?
    // No, standard is i, j, k.
    // Better: i, k, j (Row-major friendly for C and B)
    // For each i, for each k:
    //   Load A[i][k] (scalar)
    //   Broadcast A[i][k] to vector
    //   Load B[k][j...j+7] (vector)
    //   FMA: C[i][j...j+7] += A_vec * B_vec
    //   Store C

    int M = A->rows;
    int N = B->cols;
    int K = A->cols;

    for (int i = 0; i < M; i++) {
        for (int k = 0; k < K; k++) {
            float32_t a_val = A->data[i * A->cols + k];
            __m256 a_vec = _mm256_set1_ps(a_val);

            int j = 0;
            // Process 8 elements at a time
            for (; j <= N - 8; j += 8) {
                // Load C[i][j]
                __m256 c_vec = _mm256_loadu_ps(&C->data[i * C->cols + j]);
                // Load B[k][j]
                __m256 b_vec = _mm256_loadu_ps(&B->data[k * B->cols + j]);
                // FMA
                c_vec = _mm256_fmadd_ps(a_vec, b_vec, c_vec);
                // Store C
                _mm256_storeu_ps(&C->data[i * C->cols + j], c_vec);
            }

            // Handle remaining elements
            for (; j < N; j++) {
                C->data[i * C->cols + j] += a_val * B->data[k * B->cols + j];
            }
        }
    }
    return EIF_STATUS_OK;

#elif defined(EIF_USE_NEON)
    // NEON Implementation (4 floats at a time)
    int M = A->rows;
    int N = B->cols;
    int K = A->cols;

    for (int i = 0; i < M; i++) {
        for (int k = 0; k < K; k++) {
            float32_t a_val = A->data[i * A->cols + k];
            float32x4_t a_vec = vdupq_n_f32(a_val);

            int j = 0;
            for (; j <= N - 4; j += 4) {
                float32x4_t c_vec = vld1q_f32(&C->data[i * C->cols + j]);
                float32x4_t b_vec = vld1q_f32(&B->data[k * B->cols + j]);
                c_vec = vmlaq_f32(c_vec, a_vec, b_vec); // C + A * B
                vst1q_f32(&C->data[i * C->cols + j], c_vec);
            }

            for (; j < N; j++) {
                C->data[i * C->cols + j] += a_val * B->data[k * B->cols + j];
            }
        }
    }
    return EIF_STATUS_OK;

#else
    // Fallback to scalar if no SIMD
    // But we should probably warn or just run scalar.
    // Let's run optimized scalar (i, k, j loop order)
    int M = A->rows;
    int N = B->cols;
    int K = A->cols;

    for (int i = 0; i < M; i++) {
        for (int k = 0; k < K; k++) {
            float32_t a_val = A->data[i * A->cols + k];
            for (int j = 0; j < N; j++) {
                C->data[i * C->cols + j] += a_val * B->data[k * B->cols + j];
            }
        }
    }
    return EIF_STATUS_OK;
#endif
}
