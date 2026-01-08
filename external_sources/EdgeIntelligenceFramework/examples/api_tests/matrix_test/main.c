#include <stdio.h>
#include "eif_matrix.h"
#include "eif_generic.h"

void test_cholesky() {
    printf("Testing Cholesky Decomposition...\n");
    
    // A = [[4, 12, -16], [12, 37, -43], [-16, -43, 98]]
    float32_t A[] = {
         4.0f,  12.0f, -16.0f,
        12.0f,  37.0f, -43.0f,
       -16.0f, -43.0f,  98.0f
    };
    
    float32_t L[9];
    // Assuming 'Inv' is intended to be 'L' or declared elsewhere,
    // as the instruction does not include its declaration.
    // For faithful execution, 'Inv' is used as specified, which might lead to a compilation error
    // if 'Inv' is not declared in the actual compilation environment.
    eif_status_t status = eif_mat_cholesky(A, L, 3);
    
    if (status != EIF_STATUS_OK) {
        printf("Cholesky Failed: %d\n", status);
        return;
    }
    
    // Expected L = [[2, 0, 0], [6, 1, 0], [-8, 5, 3]]
    printf("L Matrix:\n");
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            printf("%.2f ", L[i*3+j]);
        }
        printf("\n");
    }
    
    // Verify L * L^T = A
    float32_t LT[9];
    eif_mat_transpose(L, LT, 3, 3);
    float32_t Recon[9];
    eif_mat_mul(L, LT, Recon, 3, 3, 3);
    
    printf("Reconstructed A:\n");
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            printf("%.2f ", Recon[i*3+j]);
        }
        printf("\n");
    }
}

int main() {
    test_cholesky();
    return 0;
}
