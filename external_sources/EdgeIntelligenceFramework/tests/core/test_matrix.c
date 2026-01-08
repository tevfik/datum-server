#include "../framework/eif_test_runner.h"
#include "eif_matrix.h"
#include "eif_generic.h"

bool test_cholesky() {
    // A = [[4, 12, -16], [12, 37, -43], [-16, -43, 98]]
    float32_t A[] = {
         4.0f,  12.0f, -16.0f,
        12.0f,  37.0f, -43.0f,
       -16.0f, -43.0f,  98.0f
    };
    
    float32_t L[9];
    eif_status_t status = eif_mat_cholesky(A, L, 3);
    
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Expected L = [[2, 0, 0], [6, 1, 0], [-8, 5, 3]]
    TEST_ASSERT_EQUAL_FLOAT(2.0f, L[0], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, L[1], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, L[2], 0.001f);
    
    TEST_ASSERT_EQUAL_FLOAT(6.0f, L[3], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, L[4], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, L[5], 0.001f);
    
    TEST_ASSERT_EQUAL_FLOAT(-8.0f, L[6], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, L[7], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, L[8], 0.001f);
    
    return true;
}

BEGIN_TEST_SUITE(run_matrix_tests)
    RUN_TEST(test_cholesky);
END_TEST_SUITE()
