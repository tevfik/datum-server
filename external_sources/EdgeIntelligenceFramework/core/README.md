# Core Module

The **Core Module** provides the foundational building blocks for the Edge Intelligence Framework. It handles memory management, basic data structures, mathematical operations, and SIMD optimizations.

## Features

*   **Memory Management**:
    *   `eif_memory_pool_t`: Static memory pool allocator for deterministic memory usage.
    *   Arena-based allocation for efficient lifecycle management.
*   **Matrix Operations**:
    *   `eif_matrix_ops.h`: Basic matrix addition, multiplication, transposition, and inversion.
    *   **SIMD Support**: Optimized implementations using AVX2/FMA intrinsics for accelerated performance on supported hardware.
    *   `eif_matrix_fixed.h`: Fixed-point arithmetic support for MCUs without FPU.
*   **Utilities**:
    *   Common type definitions (`eif_types.h`).
    *   Status codes and error handling (`eif_status.h`).
    *   Generic macros and helper functions (`eif_generic.h`).

## Usage

### Memory Pool Initialization
```c
#include "eif_memory.h"

static uint8_t buffer[1024];
eif_memory_pool_t pool;

void init() {
    eif_memory_pool_init(&pool, buffer, sizeof(buffer));
    void* ptr = eif_memory_alloc(&pool, 100, 4);
}
```

### Matrix Operations
```c
#include "eif_matrix_ops.h"

float32_t A[4] = {1, 2, 3, 4};
float32_t B[4] = {5, 6, 7, 8};
float32_t C[4];

// C = A + B
eif_matrix_add(A, B, C, 2, 2);
```
