#include <stdio.h>
#include "eif_generic.h"
#include "eif_memory.h"

void test_generic_matrix_add() {
    printf("Testing Generic Matrix Add...\n");
    
    // Float
    float A_f[] = {1.0, 2.0};
    float B_f[] = {0.5, 0.5};
    float C_f[2];
    eif_mat_add(A_f, B_f, C_f, 1, 2);
    printf("Float: %.2f (Expected 1.5)\n", C_f[0]);
    
    // Fixed
    q15_t A_q[] = {16384, 32767}; // 0.5, 1.0
    q15_t B_q[] = {8192, 0};      // 0.25, 0.0
    q15_t C_q[2];
    eif_mat_add(A_q, B_q, C_q, 1, 2);
    printf("Fixed: %d (Expected ~24576 -> 0.75)\n", C_q[0]);
}

void test_generic_dsp_rms() {
    printf("Testing Generic RMS...\n");
    
    // Float
    float data_f[] = {1.0, -1.0};
    float rms_f = eif_dsp_rms(data_f, 2);
    printf("Float RMS: %.2f (Expected 1.0)\n", rms_f);
    
    // Fixed
    q15_t data_q[] = {16384, -16384}; // 0.5, -0.5
    q15_t rms_q = eif_dsp_rms(data_q, 2);
    printf("Fixed RMS: %d (Expected ~16384 -> 0.5)\n", rms_q);
}

int main() {
    test_generic_matrix_add();
    test_generic_dsp_rms();
    return 0;
}
