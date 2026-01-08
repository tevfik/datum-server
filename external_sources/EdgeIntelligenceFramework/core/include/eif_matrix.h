#ifndef EIF_MATRIX_H
#define EIF_MATRIX_H

#include "eif_types.h"
#include "eif_status.h"

// Matrix Operations
// Matrices are flattened 1D arrays (Row-Major)

typedef struct {
    int rows;
    int cols;
    float32_t* data;
} eif_matrix_t;

void eif_matrix_init(eif_matrix_t* mat, int rows, int cols, float32_t* data);

// C = A + B
eif_status_t eif_mat_add_f32(const float32_t* A, const float32_t* B, float32_t* C, int rows, int cols);

// C = A - B
eif_status_t eif_mat_sub_f32(const float32_t* A, const float32_t* B, float32_t* C, int rows, int cols);

// C = A * B
eif_status_t eif_mat_mul_f32(const float32_t* A, const float32_t* B, float32_t* C, int rowsA, int colsA, int colsB);

// AT = A^T
eif_status_t eif_mat_transpose_f32(const float32_t* A, float32_t* AT, int rows, int cols);

// Inv = A^-1 (Gauss-Jordan)
#include "eif_memory.h"
eif_status_t eif_mat_inverse_f32(const float32_t* A, float32_t* Inv, int n, eif_memory_pool_t* pool);

// L = Cholesky(A)
eif_status_t eif_mat_cholesky_f32(const float32_t* A, float32_t* L, int n);

// C = scale * A
eif_status_t eif_mat_scale_f32(const float32_t* A, float32_t scale, float32_t* C, int rows, int cols);

// Identity Matrix
eif_status_t eif_mat_identity_f32(float32_t* I, int n);

// Copy Matrix
eif_status_t eif_mat_copy_f32(const float32_t* Src, float32_t* Dst, int rows, int cols);

// SIMD Operations
eif_status_t eif_matrix_mul_simd(const eif_matrix_t* A, const eif_matrix_t* B, eif_matrix_t* C);

#endif // EIF_MATRIX_H
