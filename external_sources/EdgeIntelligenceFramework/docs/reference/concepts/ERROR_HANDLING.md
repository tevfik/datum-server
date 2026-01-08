# EIF Error Handling & Validation Guide

Safe error handling for mission-critical embedded applications.

## Status Codes

```c
typedef enum {
    EIF_STATUS_OK = 0,
    EIF_STATUS_ERROR = -1,
    EIF_STATUS_INVALID_ARGUMENT = -2,
    EIF_STATUS_OUT_OF_MEMORY = -3,
    EIF_STATUS_NOT_IMPLEMENTED = -4,
    EIF_STATUS_NOT_SUPPORTED = -5,
    EIF_STATUS_TIMEOUT = -6,
    EIF_STATUS_INTERNAL_ERROR = -7
} eif_status_t;
```

## Validation Macros

### Debug vs Release

```c
// Debug build: cc -DEIF_ENABLE_VALIDATION ...
// Release build: cc ... (validation stripped, zero overhead)
```

### Available Macros

| Macro | Debug | Release | Use Case |
|-------|-------|---------|----------|
| `EIF_VALIDATE_PTR(ptr)` | Checks NULL | No-op | Input validation |
| `EIF_VALIDATE_RANGE(v, min, max)` | Checks bounds | No-op | Parameter bounds |
| `EIF_CRITICAL_PTR(ptr)` | Checks NULL | Checks NULL | Always-on safety |
| `EIF_TRY(expr)` | Propagates | Propagates | Error propagation |

## Usage Examples

### Basic Validation

```c
#include "eif_assert.h"

eif_status_t process_buffer(const float* data, int size) {
    // Stripped in release builds
    EIF_VALIDATE_PTR(data);
    EIF_VALIDATE_RANGE(size, 1, 1024);
    
    // Always-on critical checks
    EIF_CRITICAL_CHECK(size > 0, EIF_STATUS_INVALID_ARGUMENT);
    
    // Process...
    return EIF_STATUS_OK;
}
```

### Error Propagation

```c
eif_status_t inner_function(void) {
    // May return error
    return EIF_STATUS_OK;
}

eif_status_t outer_function(void) {
    // If inner fails, immediately return its error
    EIF_TRY(inner_function());
    
    // Continue if successful
    return EIF_STATUS_OK;
}
```

### With Cleanup

```c
eif_status_t allocate_and_process(void) {
    void* buffer = malloc(1024);
    
    // If check fails, free buffer then return error
    EIF_TRY_OR(validate_input(), {
        free(buffer);
    });
    
    process(buffer);
    free(buffer);
    return EIF_STATUS_OK;
}
```

## Compile-Time Assertions

```c
// Verify struct sizes at compile time
EIF_STATIC_ASSERT(sizeof(eif_layer_t) == 64, "layer_size_changed");

// Verify configuration
EIF_STATIC_ASSERT(EIF_MAX_LAYERS >= 16, "need_more_layers");
```

## Debug Helpers

### Breakpoint

```c
#ifdef EIF_ENABLE_VALIDATION
    // Triggers debugger on ARM or x86
    EIF_BREAKPOINT();
#endif
```

### Assertions

```c
// Only in debug builds
EIF_ASSERT(buffer != NULL);
EIF_ASSERT(count > 0 && count < 1000);
```

## Best Practices

### 1. Use CRITICAL for Safety-Critical Paths

```c
eif_status_t control_motor(int speed) {
    // Always check - motor control is safety-critical
    EIF_CRITICAL_CHECK(speed >= -100 && speed <= 100, 
                       EIF_STATUS_INVALID_ARGUMENT);
    set_pwm(speed);
    return EIF_STATUS_OK;
}
```

### 2. Use VALIDATE for Development Checks

```c
eif_status_t optimize_weights(float* weights, int count) {
    // Only check during development
    EIF_VALIDATE_PTR(weights);
    EIF_VALIDATE_RANGE(count, 1, 10000);
    
    // Optimization logic...
    return EIF_STATUS_OK;
}
```

### 3. Always Check Return Values

```c
// Use [[nodiscard]] attribute on functions
EIF_WARN_UNUSED_RESULT
eif_status_t critical_init(void);

// Compiler will warn if return value ignored
critical_init();  // Warning!
```

## Configuration

```c
// eif_assert.h configuration

// Enable all validation (debug)
#define EIF_ENABLE_VALIDATION

// Disable critical checks (dangerous!)
#define EIF_DISABLE_CRITICAL
```

## Binary Size Impact

| Mode | Size Impact | Performance |
|------|-------------|-------------|
| Release (default) | 0 bytes | No overhead |
| Debug (VALIDATION) | ~4-8 bytes/check | Minimal |
| CRITICAL only | ~4 bytes/check | Minimal |
