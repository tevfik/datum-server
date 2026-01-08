#include "eif_matrix.h"
#include "eif_test_runner.h"
#include <math.h>

bool test_matrix_mul_simd() {
    // Test 8x8 Matrix Multiplication
    // A: 8x8, B: 8x8, C: 8x8
    // Initialize A and B with known values
    
    eif_matrix_t A, B, C;
    float32_t dataA[64], dataB[64], dataC[64];
    
    eif_matrix_init(&A, 8, 8, dataA);
    eif_matrix_init(&B, 8, 8, dataB);
    eif_matrix_init(&C, 8, 8, dataC);
    
    for (int i = 0; i < 64; i++) {
        dataA[i] = (float32_t)(i % 5);
        dataB[i] = (float32_t)(i % 3);
    }
    
    eif_matrix_mul_simd(&A, &B, &C);
    
    // Verify with scalar multiplication (manual check for a few elements)
    // C[0][0] = row 0 of A * col 0 of B
    // Row 0 of A: 0, 1, 2, 3, 4, 0, 1, 2
    // Col 0 of B: 0, 0, 1, 2, 0, 1, 2, 0 (indices 0, 8, 16...)
    // Let's just calculate C[0][0] manually
    float32_t expected_c00 = 0;
    for (int k = 0; k < 8; k++) {
        expected_c00 += dataA[0 * 8 + k] * dataB[k * 8 + 0];
    }
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_c00, dataC[0]);
    
    // Verify C[7][7]
    float32_t expected_c77 = 0;
    for (int k = 0; k < 8; k++) {
        expected_c77 += dataA[7 * 8 + k] * dataB[k * 8 + 7];
    }
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_c77, dataC[63]);
    
    return true;
}

bool test_matrix_mul_simd_large() {
    // Test 32x32 Matrix Multiplication (Multiple of 8)
    eif_matrix_t A, B, C;
    float32_t dataA[1024], dataB[1024], dataC[1024];
    
    eif_matrix_init(&A, 32, 32, dataA);
    eif_matrix_init(&B, 32, 32, dataB);
    eif_matrix_init(&C, 32, 32, dataC);
    
    for (int i = 0; i < 1024; i++) {
        dataA[i] = 1.0f;
        dataB[i] = 1.0f;
    }
    
    eif_matrix_mul_simd(&A, &B, &C);
    
    // C[i][j] should be 32.0f
    for (int i = 0; i < 1024; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 32.0f, dataC[i]);
    }
    
    return true;
}

BEGIN_TEST_SUITE(run_matrix_simd_tests)
    RUN_TEST(test_matrix_mul_simd);
    RUN_TEST(test_matrix_mul_simd_large);
END_TEST_SUITE()
