#include "eif_matrix_fixed.h"
#include "eif_utils.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

eif_status_t eif_mat_add_q15(const q15_t* A, const q15_t* B, q15_t* C, int rows, int cols) {
    if (!A || !B || !C) return EIF_STATUS_INVALID_ARGUMENT;
    for (int i = 0; i < rows * cols; i++) {
        C[i] = eif_q15_add(A[i], B[i]);
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_mat_sub_q15(const q15_t* A, const q15_t* B, q15_t* C, int rows, int cols) {
    if (!A || !B || !C) return EIF_STATUS_INVALID_ARGUMENT;
    for (int i = 0; i < rows * cols; i++) {
        C[i] = eif_q15_sub(A[i], B[i]);
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_mat_mul_q15(const q15_t* A, const q15_t* B, q15_t* C, int rowsA, int colsA, int colsB) {
    if (!A || !B || !C) return EIF_STATUS_INVALID_ARGUMENT;
    for (int i = 0; i < rowsA; i++) {
        for (int j = 0; j < colsB; j++) {
            q31_t sum = 0; // Use Q31 accumulator
            for (int k = 0; k < colsA; k++) {
                sum += ((q31_t)A[i * colsA + k] * B[k * colsB + j]);
            }
            // Result is in Q30 (Q15*Q15), need to shift right by 15 to get Q15
            // Also handle saturation
            sum = sum >> 15;
            if (sum > EIF_Q15_MAX) C[i * colsB + j] = EIF_Q15_MAX;
            else if (sum < EIF_Q15_MIN) C[i * colsB + j] = EIF_Q15_MIN;
            else C[i * colsB + j] = (q15_t)sum;
        }
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_mat_transpose_q15(const q15_t* A, q15_t* AT, int rows, int cols) {
    if (!A || !AT) return EIF_STATUS_INVALID_ARGUMENT;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            AT[j * rows + i] = A[i * cols + j];
        }
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_mat_scale_q15(const q15_t* A, q15_t scale, q15_t* C, int rows, int cols) {
    if (!A || !C) return EIF_STATUS_INVALID_ARGUMENT;
    for (int i = 0; i < rows * cols; i++) {
        C[i] = eif_q15_mul(A[i], scale);
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_mat_identity_q15(q15_t* I, int n) {
    if (!I) return EIF_STATUS_INVALID_ARGUMENT;
    memset(I, 0, n * n * sizeof(q15_t));
    for (int i = 0; i < n; i++) {
        I[i * n + i] = EIF_Q15_MAX; // Represents approx 1.0
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_mat_copy_q15(const q15_t* Src, q15_t* Dst, int rows, int cols) {
    if (!Src || !Dst) return EIF_STATUS_INVALID_ARGUMENT;
    memcpy(Dst, Src, rows * cols * sizeof(q15_t));
    return EIF_STATUS_OK;
}

eif_status_t eif_mat_inverse_q15(const q15_t* A, q15_t* Inv, int n, eif_memory_pool_t* pool) {
    if (!A || !Inv || n <= 0 || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Allocate temp buffers from pool
    size_t float_size = n * n * sizeof(float);
    float* fA = (float*)eif_memory_alloc(pool, float_size, 4);
    float* fInv = (float*)eif_memory_alloc(pool, float_size, 4);
    
    if (!fA || !fInv) return EIF_STATUS_OUT_OF_MEMORY;
    
    // Convert A to float
    for(int i=0; i<n*n; i++) fA[i] = EIF_Q15_TO_FLOAT(A[i]);
    
    // Initialize fInv to Identity
    EIF_MEMSET(fInv, 0, float_size);
    for(int i=0; i<n; i++) fInv[i*n+i] = 1.0f;
    
    // Gauss-Jordan
    for (int i = 0; i < n; i++) {
        // Pivot
        float pivot = fA[i*n + i];
        if (pivot == 0.0f) {
            // Singular
            return EIF_STATUS_ERROR; 
        }
        
        // Scale row i
        for (int j = 0; j < n; j++) {
            fA[i*n + j] /= pivot;
            fInv[i*n + j] /= pivot;
        }
        
        // Eliminate other rows
        for (int k = 0; k < n; k++) {
            if (k != i) {
                float factor = fA[k*n + i];
                for (int j = 0; j < n; j++) {
                    fA[k*n + j] -= factor * fA[i*n + j];
                    fInv[k*n + j] -= factor * fInv[i*n + j];
                }
            }
        }
    }
    
    // Convert back to Q15 with saturation
    for(int i=0; i<n*n; i++) {
        float val = fInv[i] * 32768.0f;
        if (val >= 32767.0f) Inv[i] = EIF_Q15_MAX;
        else if (val <= -32768.0f) Inv[i] = EIF_Q15_MIN;
        else Inv[i] = (q15_t)(val + 0.5f);
    }
    
    // No free() needed for pool, user resets it.
    return EIF_STATUS_OK;
}

eif_status_t eif_mat_cholesky_q15(const q15_t* A, q15_t* L, int n) {
    if (!A || !L || n <= 0) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Clear L
    EIF_MEMSET(L, 0, n * n * sizeof(q15_t));
    
    for (int i = 0; i < n; i++) {
        for (int j = 0; j <= i; j++) {
            q31_t sum = 0;
            
            if (j == i) {
                // Diagonal: L[i,i] = sqrt(A[i,i] - sum(L[i,k]^2))
                for (int k = 0; k < j; k++) {
                    q15_t val = L[i*n + k];
                    sum += ((q31_t)val * val) >> 15;
                }
                
                q31_t diag_val = A[i*n + i] - sum;
                if (diag_val <= 0) return EIF_STATUS_ERROR; // Not positive definite
                
                L[i*n + j] = eif_q15_sqrt((q15_t)diag_val);
            } else {
                // Off-diagonal: L[i,j] = (A[i,j] - sum(L[i,k]*L[j,k])) / L[j,j]
                for (int k = 0; k < j; k++) {
                    q15_t val_i = L[i*n + k];
                    q15_t val_j = L[j*n + k];
                    sum += ((q31_t)val_i * val_j) >> 15;
                }
                
                q31_t val = A[i*n + j] - sum;
                q15_t div = L[j*n + j];
                
                // Division: val / div
                // Q15 division: (val << 15) / div
                // Check overflow
                if (div == 0) return EIF_STATUS_ERROR;
                
                q31_t res = (val << 15) / div;
                
                // Saturate
                if (res > 32767) res = 32767;
                else if (res < -32768) res = -32768;
                
                L[i*n + j] = (q15_t)res;
            }
        }
    }
    return EIF_STATUS_OK;
}
