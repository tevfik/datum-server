#include "eif_ringbuffer.h"
#include <string.h>

void eif_ringbuffer_init(eif_ringbuffer_t *rb, uint8_t *buffer, size_t size, bool overwrite) {
    if (!rb || !buffer || size == 0) return;
    
    rb->buffer = buffer;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->overwrite = overwrite;
}

size_t eif_ringbuffer_write(eif_ringbuffer_t *rb, const uint8_t *data, size_t length) {
    if (!rb || !data || length == 0) return 0;

    size_t space_avail = rb->size - rb->count;
    
    // If not overwrite mode and not enough space, write only what fits
    if (!rb->overwrite && length > space_avail) {
        length = space_avail;
    }
    
    if (length == 0) return 0;

    size_t new_count = rb->count + length;
    if (new_count > rb->size && rb->overwrite) {
        // If we overflow in overwrite mode, we will lose (new_count - size) bytes
        // Move tail forward by overlap amount
        size_t overflow = new_count - rb->size;
        rb->tail = (rb->tail + overflow) % rb->size;
        rb->count = rb->size;
    } else {
        rb->count = new_count;
    }

    // Write logic with wrapping
    size_t first_chunk = rb->size - rb->head;
    if (length <= first_chunk) {
        memcpy(&rb->buffer[rb->head], data, length);
        rb->head = (rb->head + length) % rb->size;
    } else {
        memcpy(&rb->buffer[rb->head], data, first_chunk);
        size_t second_chunk = length - first_chunk;
        memcpy(&rb->buffer[0], data + first_chunk, second_chunk);
        rb->head = second_chunk;
    }

    return length;
}

size_t eif_ringbuffer_read(eif_ringbuffer_t *rb, uint8_t *data, size_t length) {
    if (!rb || !data || length == 0 || rb->count == 0) return 0;

    if (length > rb->count) {
        length = rb->count;
    }

    size_t first_chunk = rb->size - rb->tail;
    if (length <= first_chunk) {
        memcpy(data, &rb->buffer[rb->tail], length);
        rb->tail = (rb->tail + length) % rb->size;
    } else {
        memcpy(data, &rb->buffer[rb->tail], first_chunk);
        size_t second_chunk = length - first_chunk;
        memcpy(data + first_chunk, &rb->buffer[0], second_chunk);
        rb->tail = second_chunk;
    }
    
    rb->count -= length;
    return length;
}

size_t eif_ringbuffer_peek(eif_ringbuffer_t *rb, uint8_t *data, size_t length) {
    if (!rb || !data || length == 0 || rb->count == 0) return 0;

    if (length > rb->count) {
        length = rb->count;
    }

    size_t temp_tail = rb->tail;
    size_t first_chunk = rb->size - temp_tail;
    
    if (length <= first_chunk) {
        memcpy(data, &rb->buffer[temp_tail], length);
    } else {
        memcpy(data, &rb->buffer[temp_tail], first_chunk);
        size_t second_chunk = length - first_chunk;
        memcpy(data + first_chunk, &rb->buffer[0], second_chunk);
    }
    
    return length;
}

size_t eif_ringbuffer_skip(eif_ringbuffer_t *rb, size_t length) {
    if (!rb || length == 0 || rb->count == 0) return 0;

    if (length > rb->count) {
        length = rb->count;
    }

    rb->tail = (rb->tail + length) % rb->size;
    rb->count -= length;
    
    return length;
}

size_t eif_ringbuffer_available_write(eif_ringbuffer_t *rb) {
    if (!rb) return 0;
    return rb->size - rb->count;
}

size_t eif_ringbuffer_available_read(eif_ringbuffer_t *rb) {
    if (!rb) return 0;
    return rb->count;
}

void eif_ringbuffer_reset(eif_ringbuffer_t *rb) {
    if (!rb) return;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}
