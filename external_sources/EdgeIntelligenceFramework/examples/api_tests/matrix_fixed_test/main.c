#include <stdio.h>
#include "eif_matrix_fixed.h"
#include "eif_memory.h"
#include <stdlib.h>

void test_matrix_mul_q15() {
    printf("Testing Q15 Matrix Multiplication...\n");
    
    // A = [0.5, 0.5; 0.2, 0.8]
    q15_t A[] = {EIF_FLOAT_TO_Q15(0.5f), EIF_FLOAT_TO_Q15(0.5f),
                 EIF_FLOAT_TO_Q15(0.2f), EIF_FLOAT_TO_Q15(0.8f)};
                 
    // B = [1.0; 0.0]
    q15_t B[] = {EIF_FLOAT_TO_Q15(1.0f), 0};
    
    // C = A * B = [0.5; 0.2]
    q15_t C[2];
    
    eif_mat_mul_q15(A, B, C, 2, 2, 1);
    
    printf("Result: [%.4f, %.4f]\n", EIF_Q15_TO_FLOAT(C[0]), EIF_Q15_TO_FLOAT(C[1]));
    
    if (C[0] > EIF_FLOAT_TO_Q15(0.49f) && C[0] < EIF_FLOAT_TO_Q15(0.51f)) {
        printf("SUCCESS: Mul Q15\n");
    } else {
        printf("FAILURE: Mul Q15\n");
    }
}

void test_matrix_inv_q15() {
    printf("Testing Q15 Matrix Inverse (MCU Optimized)...\n");
    
    // Memory Pool Init
    uint8_t buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, buffer, sizeof(buffer));
    
    // A = [0.5, 0; 0, 0.5] -> Inv = [2.0, 0; 0, 2.0]
    // Note: 2.0 is not representable in Q15 (max ~1.0).
    // So Inv will saturate to 1.0 (EIF_Q15_MAX).
    // This test case is bad for Q15 range.
    // Let's use A = [0.8, 0; 0, 0.8] -> Inv = [1.25, 0; 0, 1.25] -> Saturates to 1.0?
    // Q15 range is [-1, 1).
    // We need A such that Inv is within [-1, 1).
    // So A must be > 1.0? No, A can be anything, but Inv must be < 1.0.
    // So A elements must be > 1.0?
    // If A = 2.0 (invalid Q15).
    // If A = 0.5, Inv = 2.0 (invalid).
    // If A = -0.5, Inv = -2.0 (invalid).
    // We need |Inv| <= 1.
    // So |A| >= 1.
    // But Q15 max is < 1.
    // So we CANNOT represent the inverse of any valid Q15 number < 1 in Q15, unless it's 1.0 (which is also not quite representable).
    // Wait, Q15 represents [-1, 0.9999].
    // If A = 0.5, Inv = 2.0. Overflow.
    // If A = 0.8, Inv = 1.25. Overflow.
    // If A = 1.0 (approx), Inv = 1.0.
    // If A = -1.0, Inv = -1.0.
    
    // So matrix inversion in Q15 is only valid for matrices with eigenvalues magnitude >= 1.
    // But we can't represent values >= 1 in Q15 input either (except -1).
    // So we are stuck.
    // Fixed-point usually requires block scaling (exponent).
    // Our library is simple Q15.
    // Let's test with Identity (Inv = Identity).
    // Or A = -Identity (Inv = -Identity).
    
    q15_t A[] = {EIF_Q15_MAX, 0, 0, EIF_Q15_MAX}; // approx 1.0
    q15_t Inv[4];
    
    if (eif_mat_inverse_q15(A, Inv, 2, &pool) == EIF_STATUS_OK) {
        printf("Inv: [%.4f, %.4f; %.4f, %.4f]\n", 
               EIF_Q15_TO_FLOAT(Inv[0]), EIF_Q15_TO_FLOAT(Inv[1]),
               EIF_Q15_TO_FLOAT(Inv[2]), EIF_Q15_TO_FLOAT(Inv[3]));
               
        // Expect approx 1.0
        if (Inv[0] > 32000) {
             printf("SUCCESS: Inv Q15\n");
        } else {
             printf("FAILURE: Inv Q15 Value Mismatch\n");
        }
    } else {
        printf("FAILURE: Inv Q15 Error\n");
    }
}

void test_cholesky_q15() {
    printf("Testing Q15 Cholesky...\n");
    // A = [0.4, 0.2; 0.2, 0.2]
    // L = [0.632, 0; 0.316, 0.316]
    
    q15_t A[] = {13107, 6554, 6554, 6554}; // 0.4, 0.2, 0.2, 0.2 approx
    q15_t L[4];
    
    if (eif_mat_cholesky_q15(A, L, 2) == EIF_STATUS_OK) {
        printf("L: [%d, %d; %d, %d]\n", L[0], L[1], L[2], L[3]);
        // Expected: ~20724, 0, ~10362, ~10362
        if (L[0] > 20000 && L[3] > 10000) {
            printf("SUCCESS: Cholesky Q15\n");
        } else {
            printf("FAILURE: Cholesky Q15 Value Mismatch\n");
        }
    } else {
        printf("FAILURE: Cholesky Q15 Error\n");
    }
}

int main() {
    test_matrix_mul_q15();
    test_matrix_inv_q15();
    test_cholesky_q15();
    return 0;
}
