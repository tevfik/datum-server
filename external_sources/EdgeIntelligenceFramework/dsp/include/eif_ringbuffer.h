#ifndef EIF_RINGBUFFER_H
#define EIF_RINGBUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Circular Buffer Structure
 */
typedef struct {
    uint8_t *buffer;    ///< Pointer to data buffer
    size_t size;        ///< Total size of buffer
    size_t head;        ///< Write index
    size_t tail;        ///< Read index
    size_t count;       ///< Current number of bytes utilized
    bool overwrite;     ///< If true, allows writing over old data when full
} eif_ringbuffer_t;

/**
 * @brief Initialize ring buffer
 * @param rb Pointer to ring buffer structure
 * @param buffer Storage array allocated by user
 * @param size Size of the storage array
 * @param overwrite Allow overwriting old data if buffer is full
 */
void eif_ringbuffer_init(eif_ringbuffer_t *rb, uint8_t *buffer, size_t size, bool overwrite);

/**
 * @brief Write data to ring buffer
 * @param rb Pointer to ring buffer
 * @param data Data to write
 * @param length Length of data
 * @return Number of bytes written
 */
size_t eif_ringbuffer_write(eif_ringbuffer_t *rb, const uint8_t *data, size_t length);

/**
 * @brief Read data from ring buffer
 * @param rb Pointer to ring buffer
 * @param data Buffer to store read data
 * @param length Number of bytes to read
 * @return Number of bytes actually read
 */
size_t eif_ringbuffer_read(eif_ringbuffer_t *rb, uint8_t *data, size_t length);

/**
 * @brief Peek data without advancing tail
 */
size_t eif_ringbuffer_peek(eif_ringbuffer_t *rb, uint8_t *data, size_t length);

/**
 * @brief Skip bytes (advance tail)
 */
size_t eif_ringbuffer_skip(eif_ringbuffer_t *rb, size_t length);

/**
 * @brief Get available space for writing
 */
size_t eif_ringbuffer_available_write(eif_ringbuffer_t *rb);

/**
 * @brief Get available data for reading
 */
size_t eif_ringbuffer_available_read(eif_ringbuffer_t *rb);

/**
 * @brief Reset buffer (clear contents)
 */
void eif_ringbuffer_reset(eif_ringbuffer_t *rb);

#ifdef __cplusplus
}
#endif

#endif // EIF_RINGBUFFER_H
