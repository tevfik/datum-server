#include <stdio.h>
#include "eif_types.h"
#include "eif_memory.h"
#include "eif_neural.h"
#include "eif_dsp.h"
#include "eif_bayesian.h"
#include "eif_data_analysis.h"

#define POOL_SIZE 1024

int main() {
    printf("Edge Intelligence Framework - Hello World\n");

    // Test Memory Pool
    uint8_t buffer[POOL_SIZE];
    eif_memory_pool_t pool;
    eif_status_t status = eif_memory_pool_init(&pool, buffer, POOL_SIZE);

    if (status == EIF_STATUS_OK) {
        printf("Memory Pool Initialized. Available: %zu bytes\n", eif_memory_available(&pool));
    } else {
        printf("Memory Pool Initialization Failed!\n");
    }

    // Test Allocation
    void* ptr = eif_memory_alloc(&pool, 128, 4);
    if (ptr) {
        printf("Allocated 128 bytes. Available: %zu bytes\n", eif_memory_available(&pool));
    } else {
        printf("Allocation Failed!\n");
    }

    return 0;
}
