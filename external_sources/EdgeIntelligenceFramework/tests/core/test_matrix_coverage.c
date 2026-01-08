#include "../framework/eif_test_runner.h"
#include "eif_matrix.h"
#include "eif_generic.h"
#include "eif_memory.h"

// Test for eif_mat_scale_f32
bool test_matrix_scale() {
    float32_t src[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float32_t dst[4];
    float32_t scale = 2.5f;
    
    // Correct signature: (A, scale, C, rows, cols)
    eif_status_t status = eif_mat_scale_f32(src, scale, dst, 1, 4);
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    TEST_ASSERT_EQUAL_FLOAT(2.5f, dst[0], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, dst[1], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(7.5f, dst[2], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, dst[3], 0.001f);
    
    return true;
}

// Test for eif_mat_inverse_f32 with singular matrix
bool test_matrix_inverse_singular() {
    // Singular matrix (determinant is 0)
    // [[1, 2], [2, 4]] -> 1*4 - 2*2 = 0
    float32_t src[] = {1.0f, 2.0f, 2.0f, 4.0f};
    float32_t dst[4];
    
    // Setup memory pool
    static uint8_t pool_buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Correct signature: (A, Inv, n, pool)
    eif_status_t status = eif_mat_inverse_f32(src, dst, 2, &pool);
    
    // Should return error for singular matrix
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_ERROR, status);
    
    return true;
}

BEGIN_TEST_SUITE(run_matrix_coverage_tests)
    RUN_TEST(test_matrix_scale);
    RUN_TEST(test_matrix_inverse_singular);
END_TEST_SUITE()
