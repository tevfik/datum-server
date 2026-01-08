#ifndef EIF_MATRIX_FIXED_H
#define EIF_MATRIX_FIXED_H

#include "eif_fixedpoint.h"
#include "eif_status.h"
#include "eif_memory.h"

// Fixed-Point Matrix Operations (Q15)
// Matrices are flattened 1D arrays (Row-Major)

// C = A + B
eif_status_t eif_mat_add_q15(const q15_t* A, const q15_t* B, q15_t* C, int rows, int cols);

// C = A - B
eif_status_t eif_mat_sub_q15(const q15_t* A, const q15_t* B, q15_t* C, int rows, int cols);

// C = A * B
eif_status_t eif_mat_mul_q15(const q15_t* A, const q15_t* B, q15_t* C, int rowsA, int colsA, int colsB);

// AT = A^T
eif_status_t eif_mat_transpose_q15(const q15_t* A, q15_t* AT, int rows, int cols);

// Inv = A^-1 (Gauss-Jordan)
// Note: Inversion in fixed-point is tricky due to dynamic range.
// We assume the result fits in Q15.
// Matrix Inverse (Gauss-Jordan)
// Note: Uses memory pool for temporary buffers.
// Matrix Inverse (Gauss-Jordan)
// Note: Uses memory pool for temporary buffers.
eif_status_t eif_mat_inverse_q15(const q15_t* A, q15_t* Inv, int n, eif_memory_pool_t* pool);

// L = Cholesky(A)
// A must be n x n, symmetric, positive-definite
// L is lower triangular (n x n) such that A = L * L^T
// Note: Requires sqrt, so output precision depends on eif_q15_sqrt.
eif_status_t eif_mat_cholesky_q15(const q15_t* A, q15_t* L, int n);

// C = scale * A
eif_status_t eif_mat_scale_q15(const q15_t* A, q15_t scale, q15_t* C, int rows, int cols);

// Identity Matrix
eif_status_t eif_mat_identity_q15(q15_t* I, int n);

// Copy Matrix
eif_status_t eif_mat_copy_q15(const q15_t* Src, q15_t* Dst, int rows, int cols);

#endif // EIF_MATRIX_FIXED_H
