# EIF Memory Guard & Safety Guide

Protect against buffer overflows and memory corruption in embedded applications.

## Overview

EIF Memory Guard provides:
- **Canary values** - Detect buffer overflow/underflow
- **Guard zones** - Padding around allocations
- **Stack guards** - Detect stack corruption
- **Compile-time checks** - Verify buffer sizes

## Quick Start

```c
#include "eif_memory_guard.h"

// Declare guarded pool
EIF_GUARDED_POOL(my_pool, 4096);

void init(void) {
    eif_guard_pool_init_static(my_pool);
}

void process(void) {
    // Allocate with guard zones
    float* data = EIF_GUARD_ALLOC_ARRAY(&my_pool, float, 256);
    
    // Use data...
    
    // Check for corruption
    if (!eif_guard_pool_check(&my_pool)) {
        printf("Memory corruption detected!\n");
    }
}
```

## Guarded Pools

### Configuration

```c
// Guard zone size (default: 8 bytes)
#define EIF_GUARD_ZONE_SIZE 16

// Max tracked allocations (default: 32)
#define EIF_MAX_GUARD_ALLOCS 64
```

### Create Pool

```c
// Option 1: Static declaration with macro
EIF_GUARDED_POOL(sensor_pool, 2048);
eif_guard_pool_init_static(sensor_pool);

// Option 2: Manual setup
uint8_t buffer[4096];
eif_guard_pool_t pool;
eif_guard_pool_init(&pool, buffer, sizeof(buffer));
```

### Allocate Memory

```c
// Type-safe array allocation
float* weights = EIF_GUARD_ALLOC_ARRAY(&pool, float, 128);
int16_t* samples = EIF_GUARD_ALLOC_ARRAY(&pool, int16_t, 512);

// Raw allocation
void* ptr = eif_guard_alloc(&pool, 256);
```

### Check Integrity

```c
// Returns false if corruption detected
if (!eif_guard_pool_check(&pool)) {
    // Handle error - buffer overflow detected
    eif_guard_pool_print(&pool);  // Debug info
}
```

## Stack Guards

Protect against stack overflow in recursive or deep call stacks:

```c
void recursive_function(int depth, eif_stack_guard_t* guard) {
    // Check guard at each level
    if (!EIF_STACK_GUARD_CHECK(*guard)) {
        printf("Stack corrupted at depth %d!\n", depth);
        return;
    }
    
    if (depth > 0) {
        recursive_function(depth - 1, guard);
    }
}

void main(void) {
    eif_stack_guard_t guard;
    EIF_STACK_GUARD_INIT(guard);
    
    recursive_function(100, &guard);
    
    // Final check
    if (!EIF_STACK_GUARD_CHECK(guard)) {
        printf("Stack corruption detected!\n");
    }
}
```

## How It Works

### Canary Pattern

```
┌─────────────────────────────────────────────────┐
│ Header Canary │ Guard Zone │ User Data │ Guard Zone │ Back Canary │
│  0xDEADBEEF   │  0xCDCDCD  │   ...     │  0xCDCDCD  │  0xDEADBEEF │
└─────────────────────────────────────────────────┘
```

If any canary or guard zone is modified, corruption is detected.

### Memory Layout

```
[Front Guard 8 bytes] [User Data N bytes] [Back Guard 8 bytes]
        │                     │                    │
        ▼                     ▼                    ▼
    Filled with           Returned to         Filled with
    0xCD pattern          user as ptr         0xCD pattern
```

## Debug Mode

Enable validation for development:

```c
// Compile with
// cc -DEIF_ENABLE_VALIDATION ...

// Print pool status
eif_guard_pool_print(&pool);
```

Output:
```
=== Guard Pool Status ===
Header canary: OK
Footer canary: OK
Active allocations: 5/32
Overflows detected: 0
Pool integrity: OK
```

## Best Practices

### 1. Check Periodically

```c
void main_loop(void) {
    while (1) {
        process_sensors();
        run_inference();
        
        // Check every N iterations
        if (loop_count % 100 == 0) {
            if (!eif_guard_pool_check(&pool)) {
                handle_corruption();
            }
        }
    }
}
```

### 2. Use Stack Guards for ISRs

```c
void __attribute__((interrupt)) timer_isr(void) {
    static eif_stack_guard_t isr_guard;
    EIF_STACK_GUARD_INIT(isr_guard);
    
    // ISR work...
    
    if (!EIF_STACK_GUARD_CHECK(isr_guard)) {
        // Stack overflow in ISR - critical error
        while(1);  // Halt
    }
}
```

### 3. Combine with Assertions

```c
#include "eif_assert.h"
#include "eif_memory_guard.h"

eif_status_t safe_process(float* data, int size) {
    EIF_VALIDATE_PTR(data);
    EIF_VALIDATE_RANGE(size, 1, 1024);
    
    // Allocate with guards
    float* temp = EIF_GUARD_ALLOC_ARRAY(&pool, float, size);
    EIF_CRITICAL_PTR(temp);
    
    // Process...
    
    // Integrity check
    EIF_CRITICAL_CHECK(eif_guard_pool_check(&pool), 
                       EIF_STATUS_INTERNAL_ERROR);
    
    return EIF_STATUS_OK;
}
```

## Performance Impact

| Mode | Overhead | Use Case |
|------|----------|----------|
| Disabled | 0% | Production (release) |
| Canaries only | ~2% | Light validation |
| Full guards | ~5-10% | Development/debug |

## Limitations

- Fixed max allocations (EIF_MAX_GUARD_ALLOCS)
- No individual free (pool-based only)
- Guard zones consume extra memory
