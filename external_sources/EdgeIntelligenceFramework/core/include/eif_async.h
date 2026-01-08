/**
 * @file eif_async.h
 * @brief EIF Asynchronous Processing Interface
 *
 * Provides non-blocking inference and DMA support for:
 * - Double buffering (sensor data in, inference out)
 * - DMA memory transfers
 * - Callback-based completion
 *
 * Target platforms: STM32F4, STM32H7, ESP32
 *
 * Usage:
 * @code
 * eif_async_handle_t handle;
 * eif_inference_async_start(&model, input, output, &handle);
 *
 * // Do other work while inference runs...
 *
 * // Wait for completion
 * eif_inference_async_wait(&handle, 1000);
 * @endcode
 */

#ifndef EIF_ASYNC_H
#define EIF_ASYNC_H

#include "eif_status.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Platform Detection
// =============================================================================

#if defined(STM32H7) || defined(STM32H743xx) || defined(STM32H750xx)
#define EIF_ASYNC_STM32H7 1
#elif defined(STM32F4) || defined(STM32F407xx) || defined(STM32F446xx)
#define EIF_ASYNC_STM32F4 1
#elif defined(ESP32) || defined(ESP_PLATFORM)
#define EIF_ASYNC_ESP32 1
#else
#define EIF_ASYNC_GENERIC 1
#endif

// =============================================================================
// Types
// =============================================================================

/**
 * @brief Async operation state
 */
typedef enum {
  EIF_ASYNC_IDLE = 0,
  EIF_ASYNC_RUNNING,
  EIF_ASYNC_COMPLETE,
  EIF_ASYNC_ERROR
} eif_async_state_t;

/**
 * @brief Callback function type
 */
typedef void (*eif_async_callback_t)(void *context, eif_status_t status);

/**
 * @brief Async operation handle
 */
typedef struct {
  volatile eif_async_state_t state;
  eif_status_t result;
  void *context;
  eif_async_callback_t on_complete;
  uint32_t start_time_ms;
} eif_async_handle_t;

/**
 * @brief Double buffer configuration
 */
typedef struct {
  void *buffer_a;
  void *buffer_b;
  size_t buffer_size;
  volatile int active_buffer; // 0 = A, 1 = B
  volatile bool buffer_ready[2];
} eif_double_buffer_t;

// =============================================================================
// Async Handle API
// =============================================================================

/**
 * @brief Initialize async handle
 */
static inline void eif_async_init(eif_async_handle_t *handle) {
  handle->state = EIF_ASYNC_IDLE;
  handle->result = EIF_STATUS_OK;
  handle->context = NULL;
  handle->on_complete = NULL;
  handle->start_time_ms = 0;
}

/**
 * @brief Check if async operation is done
 */
static inline bool eif_async_done(const eif_async_handle_t *handle) {
  return handle->state == EIF_ASYNC_COMPLETE ||
         handle->state == EIF_ASYNC_ERROR;
}

/**
 * @brief Get async operation result
 */
static inline eif_status_t
eif_async_get_result(const eif_async_handle_t *handle) {
  return handle->result;
}

/**
 * @brief Mark async operation as complete
 */
static inline void eif_async_complete(eif_async_handle_t *handle,
                                      eif_status_t status) {
  handle->result = status;
  handle->state =
      (status == EIF_STATUS_OK) ? EIF_ASYNC_COMPLETE : EIF_ASYNC_ERROR;

  if (handle->on_complete) {
    handle->on_complete(handle->context, status);
  }
}

// =============================================================================
// Double Buffer API
// =============================================================================

/**
 * @brief Initialize double buffer
 */
static inline eif_status_t eif_double_buffer_init(eif_double_buffer_t *db,
                                                  void *buffer_a,
                                                  void *buffer_b, size_t size) {
  if (!db || !buffer_a || !buffer_b || size == 0) {
    return EIF_STATUS_INVALID_ARGUMENT;
  }

  db->buffer_a = buffer_a;
  db->buffer_b = buffer_b;
  db->buffer_size = size;
  db->active_buffer = 0;
  db->buffer_ready[0] = false;
  db->buffer_ready[1] = false;

  return EIF_STATUS_OK;
}

/**
 * @brief Get buffer for writing (producer side)
 */
static inline void *eif_double_buffer_get_write(eif_double_buffer_t *db) {
  int write_idx = 1 - db->active_buffer;
  return (write_idx == 0) ? db->buffer_a : db->buffer_b;
}

/**
 * @brief Get buffer for reading (consumer side)
 */
static inline void *eif_double_buffer_get_read(eif_double_buffer_t *db) {
  return (db->active_buffer == 0) ? db->buffer_a : db->buffer_b;
}

/**
 * @brief Swap buffers (call after write complete)
 */
static inline void eif_double_buffer_swap(eif_double_buffer_t *db) {
  db->active_buffer = 1 - db->active_buffer;
}

// =============================================================================
// DMA Memory Operations
// =============================================================================

/**
 * @brief Non-blocking memory copy (DMA where available)
 * @param dst Destination buffer
 * @param src Source buffer
 * @param size Bytes to copy
 * @param handle Async handle for tracking
 * @return Status
 */
eif_status_t eif_dma_memcpy_async(void *dst, const void *src, size_t size,
                                  eif_async_handle_t *handle);

/**
 * @brief Wait for DMA transfer completion
 */
eif_status_t eif_dma_wait(eif_async_handle_t *handle, uint32_t timeout_ms);

/**
 * @brief Check if DMA is available
 */
bool eif_dma_available(void);

// =============================================================================
// Async Inference (Forward Declaration)
// =============================================================================

// Model type forward declaration
struct eif_model_s;

/**
 * @brief Start async inference
 * @param model Trained model
 * @param input Input buffer
 * @param output Output buffer
 * @param handle Async handle
 * @return Status
 */
eif_status_t eif_inference_async_start(const struct eif_model_s *model,
                                       const void *input, void *output,
                                       eif_async_handle_t *handle);

/**
 * @brief Wait for async inference completion
 */
eif_status_t eif_inference_async_wait(eif_async_handle_t *handle,
                                      uint32_t timeout_ms);

// =============================================================================
// Platform-Specific Helpers
// =============================================================================

#if defined(EIF_ASYNC_STM32F4) || defined(EIF_ASYNC_STM32H7)

/**
 * @brief Get STM32 DMA channel for memory transfers
 */
void *eif_stm32_get_dma_channel(void);

/**
 * @brief Configure DMA for memory-to-memory
 */
eif_status_t eif_stm32_dma_config_m2m(void);

#endif

#if defined(EIF_ASYNC_ESP32)

/**
 * @brief ESP32 SPI DMA transfer
 */
eif_status_t eif_esp32_spi_dma_transfer(void *dst, const void *src,
                                        size_t size);

/**
 * @brief ESP32 async task creation
 */
eif_status_t eif_esp32_create_inference_task(const struct eif_model_s *model,
                                             const void *input, void *output,
                                             eif_async_handle_t *handle);

#endif

// =============================================================================
// Utility Macros
// =============================================================================

/**
 * @brief Declare static double buffers
 */
#define EIF_DECLARE_DOUBLE_BUFFER(name, type, count)                           \
  static type name##_buf_a[count];                                             \
  static type name##_buf_b[count];                                             \
  static eif_double_buffer_t name = {.buffer_a = name##_buf_a,                 \
                                     .buffer_b = name##_buf_b,                 \
                                     .buffer_size = sizeof(name##_buf_a),      \
                                     .active_buffer = 0,                       \
                                     .buffer_ready = {false, false}}

#ifdef __cplusplus
}
#endif

#endif // EIF_ASYNC_H
