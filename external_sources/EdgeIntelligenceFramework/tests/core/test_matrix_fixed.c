#include "../framework/eif_test_runner.h"
#include "eif_matrix_fixed.h"
#include "eif_generic.h"
#include <math.h>

// =============================================================================
// Tests
// =============================================================================

bool test_mat_add_q15() {
    q15_t A[] = {100, 200, 300, 400};
    q15_t B[] = {50, 60, 70, 80};
    q15_t C[4];
    
    eif_status_t status = eif_mat_add_q15(A, B, C, 2, 2);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    TEST_ASSERT_EQUAL_INT(150, C[0]);
    TEST_ASSERT_EQUAL_INT(260, C[1]);
    TEST_ASSERT_EQUAL_INT(370, C[2]);
    TEST_ASSERT_EQUAL_INT(480, C[3]);
    
    // Saturation test
    q15_t A_sat[] = {32700};
    q15_t B_sat[] = {100};
    q15_t C_sat[1];
    eif_mat_add_q15(A_sat, B_sat, C_sat, 1, 1);
    TEST_ASSERT_EQUAL_INT(32767, C_sat[0]); // EIF_Q15_MAX
    
    return true;
}

bool test_mat_sub_q15() {
    q15_t A[] = {100, 200};
    q15_t B[] = {50, 300};
    q15_t C[2];
    
    eif_status_t status = eif_mat_sub_q15(A, B, C, 1, 2);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    TEST_ASSERT_EQUAL_INT(50, C[0]);
    TEST_ASSERT_EQUAL_INT(-100, C[1]);
    
    return true;
}

bool test_mat_mul_q15() {
    // A = [[0.5, 0.5], [0.2, 0.2]] -> Q15: [[16384, 16384], [6554, 6554]]
    // B = [[0.5], [0.5]] -> Q15: [[16384], [16384]]
    // C = A * B = [[0.5], [0.2]] -> Q15: [[16384], [6554]]
    
    q15_t A[] = {16384, 16384, 6554, 6554};
    q15_t B[] = {16384, 16384};
    q15_t C[2];
    
    eif_status_t status = eif_mat_mul_q15(A, B, C, 2, 2, 1);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // 0.5 * 0.5 + 0.5 * 0.5 = 0.25 + 0.25 = 0.5 -> 16384
    // Allow some error due to fixed point rounding
    TEST_ASSERT_EQUAL_INT(16384, C[0]); // Exact match might fail, check tolerance?
    // 16384 * 16384 >> 15 = 8192. 8192 + 8192 = 16384. Should be exact.
    
    // 0.2 * 0.5 + 0.2 * 0.5 = 0.1 + 0.1 = 0.2 -> 6554
    // 6554 * 16384 >> 15 = 3277. 3277 + 3277 = 6554.
    TEST_ASSERT_EQUAL_INT(6554, C[1]);
    
    return true;
}

bool test_mat_scale_q15() {
    q15_t A[] = {1000, -2000};
    q15_t C[2];
    q15_t scale = 16384; // 0.5
    
    eif_status_t status = eif_mat_scale_q15(A, scale, C, 1, 2);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    TEST_ASSERT_EQUAL_INT(500, C[0]);
    TEST_ASSERT_EQUAL_INT(-1000, C[1]);
    
    return true;
}

bool test_mat_inverse_q15() {
    // Identity matrix
    q15_t A[] = {32767, 0, 0, 32767};
    q15_t Inv[4];
    
    uint8_t buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, buffer, sizeof(buffer));
    
    eif_status_t status = eif_mat_inverse_q15(A, Inv, 2, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    TEST_ASSERT_EQUAL_INT(32767, Inv[0]);
    TEST_ASSERT_EQUAL_INT(0, Inv[1]);
    TEST_ASSERT_EQUAL_INT(0, Inv[2]);
    TEST_ASSERT_EQUAL_INT(32767, Inv[3]);
    
    return true;
}

bool test_mat_cholesky_q15() {
    // A = [[1.0, 0], [0, 1.0]] -> L = [[1.0, 0], [0, 1.0]]
    q15_t A[] = {32767, 0, 0, 32767};
    q15_t L[4];
    
    eif_status_t status = eif_mat_cholesky_q15(A, L, 2);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Sqrt(32767) in Q15?
    // 1.0 in Q15 is 32767. Sqrt(1.0) = 1.0 = 32767.
    // eif_q15_sqrt(32767) should be 32767.
    
    TEST_ASSERT_EQUAL_INT(32767, L[0]);
    TEST_ASSERT_EQUAL_INT(0, L[1]);
    TEST_ASSERT_EQUAL_INT(0, L[2]);
    TEST_ASSERT_EQUAL_INT(32767, L[3]);
    
    return true;
}

bool test_mat_transpose_q15() {
    q15_t A[] = {1, 2, 3, 4, 5, 6};
    q15_t AT[6];
    
    // 2x3 -> 3x2
    eif_status_t status = eif_mat_transpose_q15(A, AT, 2, 3);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Expected: {1, 4, 2, 5, 3, 6}
    TEST_ASSERT_EQUAL_INT(1, AT[0]);
    TEST_ASSERT_EQUAL_INT(4, AT[1]);
    TEST_ASSERT_EQUAL_INT(2, AT[2]);
    TEST_ASSERT_EQUAL_INT(5, AT[3]);
    TEST_ASSERT_EQUAL_INT(3, AT[4]);
    TEST_ASSERT_EQUAL_INT(6, AT[5]);
    
    return true;
}

bool test_mat_identity_q15() {
    q15_t I[9];
    
    eif_status_t status = eif_mat_identity_q15(I, 3);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Diagonal should be MAX, others 0
    TEST_ASSERT_EQUAL_INT(EIF_Q15_MAX, I[0]);
    TEST_ASSERT_EQUAL_INT(0, I[1]);
    TEST_ASSERT_EQUAL_INT(0, I[2]);
    TEST_ASSERT_EQUAL_INT(0, I[3]);
    TEST_ASSERT_EQUAL_INT(EIF_Q15_MAX, I[4]);
    TEST_ASSERT_EQUAL_INT(0, I[5]);
    TEST_ASSERT_EQUAL_INT(0, I[6]);
    TEST_ASSERT_EQUAL_INT(0, I[7]);
    TEST_ASSERT_EQUAL_INT(EIF_Q15_MAX, I[8]);
    
    return true;
}

bool test_mat_copy_q15() {
    q15_t Src[] = {10, 20, 30, 40};
    q15_t Dst[4];
    
    eif_status_t status = eif_mat_copy_q15(Src, Dst, 2, 2);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    TEST_ASSERT_EQUAL_INT(10, Dst[0]);
    TEST_ASSERT_EQUAL_INT(20, Dst[1]);
    TEST_ASSERT_EQUAL_INT(30, Dst[2]);
    TEST_ASSERT_EQUAL_INT(40, Dst[3]);
    
    return true;
}

BEGIN_TEST_SUITE(run_matrix_fixed_tests)
    RUN_TEST(test_mat_add_q15);
    RUN_TEST(test_mat_sub_q15);
    RUN_TEST(test_mat_mul_q15);
    RUN_TEST(test_mat_scale_q15);
    RUN_TEST(test_mat_inverse_q15);
    RUN_TEST(test_mat_cholesky_q15);
    RUN_TEST(test_mat_transpose_q15);
    RUN_TEST(test_mat_identity_q15);
    RUN_TEST(test_mat_copy_q15);
END_TEST_SUITE()
