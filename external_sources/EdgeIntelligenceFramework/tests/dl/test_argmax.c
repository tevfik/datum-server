#include "eif_test_runner.h"
#include "eif_dl_internal.h"
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

bool test_argmax_axis1() {
    float32_t input[6] = {1.0f, 5.0f, 2.0f, 
                          9.0f, 3.0f, 4.0f};
    float32_t output[2] = {0}; // Expected output size 2
    
    eif_tensor_shape_t shape = { .dim = {2, 3, 1, 1} }; // 2x3 effectively
    shape.dim[2] = 1;
    shape.dim[3] = 1;
    
    eif_layer_param_t params = {0};
    params.argmax.axis = 1;
    
    eif_status_t status = eif_layer_argmax(input, output, &shape, &params);
    
    TEST_ASSERT_EQUAL_INT((int)status, EIF_STATUS_OK);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, output[0], 0.001f); // 5 is max (index 1)
    TEST_ASSERT_EQUAL_FLOAT(0.0f, output[1], 0.001f); // 9 is max (index 0)
    return true;
}

bool test_argmax_axis0() {
    float32_t input[6] = {1.0f, 5.0f, 2.0f, 
                          9.0f, 3.0f, 4.0f};
    float32_t output[3] = {0}; // Expected output size 3
    
    eif_tensor_shape_t shape = { .dim = {2, 3, 1, 1} };
    shape.dim[2] = 1;
    shape.dim[3] = 1;
    
    eif_layer_param_t params = {0};
    params.argmax.axis = 0;
    
    eif_status_t status = eif_layer_argmax(input, output, &shape, &params);
    
    TEST_ASSERT_EQUAL_INT((int)status, EIF_STATUS_OK);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, output[0], 0.001f); // 9 > 1
    TEST_ASSERT_EQUAL_FLOAT(0.0f, output[1], 0.001f); // 5 > 3
    TEST_ASSERT_EQUAL_FLOAT(1.0f, output[2], 0.001f); // 4 > 2
    return true;
}

#ifdef EIF_STANDALONE_TEST
int main() {
    RUN_TEST(test_argmax_axis1);
    RUN_TEST(test_argmax_axis0);
    return 0;
}
#endif
