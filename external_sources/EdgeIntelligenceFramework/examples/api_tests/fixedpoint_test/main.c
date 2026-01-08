#include <stdio.h>
#include "eif_fixedpoint.h"

void test_q7() {
    printf("Testing Q7 Arithmetic...\n");
    
    // 0.5 + 0.25 = 0.75
    q7_t a = EIF_FLOAT_TO_Q7(0.5f);   // 64
    q7_t b = EIF_FLOAT_TO_Q7(0.25f);  // 32
    q7_t c = eif_q7_add(a, b);        // 96
    printf("0.5 + 0.25 = %.4f (Expected 0.75)\n", EIF_Q7_TO_FLOAT(c));
    
    // Saturation: 0.8 + 0.8 = 1.6 -> 1.0 (approx)
    q7_t d = EIF_FLOAT_TO_Q7(0.8f);   // 102
    q7_t e = eif_q7_add(d, d);        // 204 -> 127 (Max)
    printf("0.8 + 0.8 = %.4f (Expected ~0.99, Saturation)\n", EIF_Q7_TO_FLOAT(e));
    
    // Mul: 0.5 * -0.5 = -0.25
    q7_t f = EIF_FLOAT_TO_Q7(-0.5f);
    q7_t g = eif_q7_mul(a, f);
    printf("0.5 * -0.5 = %.4f (Expected -0.25)\n", EIF_Q7_TO_FLOAT(g));
}

void test_q15() {
    printf("Testing Q15 Arithmetic...\n");
    
    q15_t a = EIF_FLOAT_TO_Q15(0.5f);
    q15_t b = EIF_FLOAT_TO_Q15(0.5f);
    q15_t c = eif_q15_mul(a, b);
    printf("0.5 * 0.5 = %.4f (Expected 0.25)\n", EIF_Q15_TO_FLOAT(c));
}

void test_q31() {
    printf("Testing Q31 Arithmetic...\n");
    
    q31_t a = EIF_FLOAT_TO_Q31(0.99f);
    q31_t b = EIF_FLOAT_TO_Q31(0.5f);
    q31_t c = eif_q31_mul(a, b);
    printf("0.99 * 0.5 = %.4f (Expected 0.495)\n", EIF_Q31_TO_FLOAT(c));
}

int main() {
    test_q7();
    test_q15();
    test_q31();
    return 0;
}
