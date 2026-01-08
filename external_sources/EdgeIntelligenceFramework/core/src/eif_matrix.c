#include "eif_matrix.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

eif_status_t eif_mat_add_f32(const float32_t* A, const float32_t* B, float32_t* C, int rows, int cols) {
    if (!A || !B || !C) return EIF_STATUS_INVALID_ARGUMENT;
    for (int i = 0; i < rows * cols; i++) {
        C[i] = A[i] + B[i];
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_mat_sub_f32(const float32_t* A, const float32_t* B, float32_t* C, int rows, int cols) {
    if (!A || !B || !C) return EIF_STATUS_INVALID_ARGUMENT;
    for (int i = 0; i < rows * cols; i++) {
        C[i] = A[i] - B[i];
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_mat_mul_f32(const float32_t* A, const float32_t* B, float32_t* C, int rowsA, int colsA, int colsB) {
    if (!A || !B || !C) return EIF_STATUS_INVALID_ARGUMENT;
    for (int i = 0; i < rowsA; i++) {
        for (int j = 0; j < colsB; j++) {
            float32_t sum = 0.0f;
            for (int k = 0; k < colsA; k++) {
                sum += A[i * colsA + k] * B[k * colsB + j];
            }
            C[i * colsB + j] = sum;
        }
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_mat_transpose_f32(const float32_t* A, float32_t* AT, int rows, int cols) {
    if (!A || !AT) return EIF_STATUS_INVALID_ARGUMENT;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            AT[j * rows + i] = A[i * cols + j];
        }
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_mat_inverse_f32(const float32_t* A, float32_t* Inv, int n, eif_memory_pool_t* pool) {
    if (!A || !Inv || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Create augmented matrix [A | I]
    float32_t* temp = (float32_t*)eif_memory_alloc(pool, n * 2 * n * sizeof(float32_t), sizeof(float32_t));
    if (!temp) return EIF_STATUS_OUT_OF_MEMORY;
    
    // Initialize [A | I]
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            temp[i * (2 * n) + j] = A[i * n + j];
            temp[i * (2 * n) + (j + n)] = (i == j) ? 1.0f : 0.0f;
        }
    }
    
    // Gauss-Jordan
    for (int i = 0; i < n; i++) {
        // Pivot
        float32_t pivot = temp[i * (2 * n) + i];
        if (fabsf(pivot) < 1e-6f) {
            return EIF_STATUS_ERROR; // Singular
        }
        
        // Normalize row i
        for (int j = 0; j < 2 * n; j++) {
            temp[i * (2 * n) + j] /= pivot;
        }
        
        // Eliminate other rows
        for (int k = 0; k < n; k++) {
            if (k != i) {
                float32_t factor = temp[k * (2 * n) + i];
                for (int j = 0; j < 2 * n; j++) {
                    temp[k * (2 * n) + j] -= factor * temp[i * (2 * n) + j];
                }
            }
        }
    }
    
    // Extract Inverse
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            Inv[i * n + j] = temp[i * (2 * n) + (j + n)];
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_mat_cholesky_f32(const float32_t* A, float32_t* L, int n) {
    if (!A || !L) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Initialize L to 0
    memset(L, 0, n * n * sizeof(float32_t));
    
    for (int i = 0; i < n; i++) {
        for (int j = 0; j <= i; j++) {
            float32_t sum = 0.0f;
            for (int k = 0; k < j; k++) {
                sum += L[i * n + k] * L[j * n + k];
            }
            
            if (i == j) {
                float32_t val = A[i * n + i] - sum;
                if (val <= 0.0f) return EIF_STATUS_ERROR; // Not positive definite
                L[i * n + j] = sqrtf(val);
            } else {
                L[i * n + j] = (1.0f / L[j * n + j]) * (A[i * n + j] - sum);
            }
        }
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_mat_scale_f32(const float32_t* A, float32_t scale, float32_t* C, int rows, int cols) {
    if (!A || !C) return EIF_STATUS_INVALID_ARGUMENT;
    for (int i = 0; i < rows * cols; i++) {
        C[i] = A[i] * scale;
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_mat_identity_f32(float32_t* I, int n) {
    if (!I) return EIF_STATUS_INVALID_ARGUMENT;
    memset(I, 0, n * n * sizeof(float32_t));
    for (int i = 0; i < n; i++) {
        I[i * n + i] = 1.0f;
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_mat_copy_f32(const float32_t* Src, float32_t* Dst, int rows, int cols) {
    if (!Src || !Dst) return EIF_STATUS_INVALID_ARGUMENT;
    memcpy(Dst, Src, rows * cols * sizeof(float32_t));
    return EIF_STATUS_OK;
}

void eif_matrix_init(eif_matrix_t* mat, int rows, int cols, float32_t* data) {
    if (mat) {
        mat->rows = rows;
        mat->cols = cols;
        mat->data = data;
    }
}
