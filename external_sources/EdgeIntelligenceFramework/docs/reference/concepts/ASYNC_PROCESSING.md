# EIF Async Processing & DMA Guide

This guide covers asynchronous operations and DMA support for efficient data pipelines.

## Overview

EIF provides async processing for:
- **Double buffering** - Ping-pong buffers for continuous data streaming
- **DMA transfers** - Non-blocking memory copies
- **Async inference** - Background model execution

## Async Handles

```c
#include "eif_async.h"

eif_async_handle_t handle;
eif_async_init(&handle);

// Start async operation
eif_dma_memcpy_async(dst, src, size, &handle);

// Do other work...
process_previous_data();

// Wait for completion
eif_status_t status = eif_dma_wait(&handle, 1000);  // 1s timeout
```

### Callbacks

```c
void my_callback(void* context, eif_status_t status) {
    int* counter = (int*)context;
    (*counter)++;
    printf("Transfer complete: %d\n", status);
}

// Set callback
handle.on_complete = my_callback;
handle.context = &my_counter;

eif_dma_memcpy_async(dst, src, size, &handle);
// Callback fires automatically on completion
```

## Double Buffering

Perfect for sensor streaming:

```c
// Declare double buffer (static allocation)
EIF_DECLARE_DOUBLE_BUFFER(sensor_buf, float, 256);

// Continuous streaming loop
while (1) {
    // Get buffer for writing (producer)
    float* write_buf = eif_double_buffer_get_write(&sensor_buf);
    read_sensor_data(write_buf, 256);
    
    // Get buffer for reading (consumer)
    float* read_buf = eif_double_buffer_get_read(&sensor_buf);
    process_data(read_buf, 256);
    
    // Swap buffers
    eif_double_buffer_swap(&sensor_buf);
}
```

### Manual Double Buffer Setup

```c
float buf_a[256];
float buf_b[256];
eif_double_buffer_t db;

eif_double_buffer_init(&db, buf_a, buf_b, sizeof(buf_a));
```

## DMA Memory Copy

```c
// Async memcpy (uses DMA where available)
eif_async_handle_t handle;
eif_async_init(&handle);

uint8_t src[1024], dst[1024];
eif_dma_memcpy_async(dst, src, sizeof(src), &handle);

// Non-blocking check
if (eif_async_done(&handle)) {
    printf("Done!\n");
}

// Blocking wait with timeout
eif_status_t result = eif_dma_wait(&handle, 100);  // 100ms timeout
if (result == EIF_STATUS_TIMEOUT) {
    printf("Timeout!\n");
}
```

## Platform Support

| Platform | DMA Available | Notes |
|----------|---------------|-------|
| ESP32 | ✅ | SPI DMA |
| STM32F4 | ✅ | DMA2 Stream 0 |
| STM32H7 | ✅ | DMA2 + MDMA |
| Generic PC | ❌ | memcpy fallback |

Check availability:
```c
if (eif_dma_available()) {
    // Use async path
} else {
    // Use synchronous path
}
```

## Inference Pipeline Example

```c
// Double buffer for input/output
EIF_DECLARE_DOUBLE_BUFFER(input_db, float, 256);
EIF_DECLARE_DOUBLE_BUFFER(output_db, float, 10);

eif_async_handle_t infer_handle;

while (sensor_active) {
    // 1. Fill write buffer with new sensor data
    float* input_write = eif_double_buffer_get_write(&input_db);
    read_sensors(input_write, 256);
    
    // 2. Start inference on read buffer
    float* input_read = eif_double_buffer_get_read(&input_db);
    float* output_write = eif_double_buffer_get_write(&output_db);
    
    eif_inference_async_start(&model, input_read, output_write, &infer_handle);
    
    // 3. Process previous output while inference runs
    float* output_read = eif_double_buffer_get_read(&output_db);
    send_results(output_read);
    
    // 4. Wait for inference
    eif_inference_async_wait(&infer_handle, 100);
    
    // 5. Swap all buffers
    eif_double_buffer_swap(&input_db);
    eif_double_buffer_swap(&output_db);
}
```

## Best Practices

1. **Pre-allocate handles** - Avoid stack allocation in tight loops
2. **Use timeouts** - Always specify timeout to prevent deadlocks
3. **Check return values** - Handle EIF_STATUS_TIMEOUT and EIF_STATUS_ERROR
4. **Align buffers** - Use 32-byte alignment for DMA efficiency

```c
// Aligned buffer for DMA
__attribute__((aligned(32))) uint8_t dma_buffer[1024];
```
