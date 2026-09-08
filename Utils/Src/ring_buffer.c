#include "ring_buffer.h"

void ring_buffer_init(ring_buffer_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

uint16_t ring_buffer_available(ring_buffer_t *rb)
{
    return (uint16_t)(rb->head - rb->tail) % RING_BUFFER_SIZE;
}

uint8_t ring_buffer_read(ring_buffer_t *rb)
{
    uint8_t data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RING_BUFFER_SIZE;
    return data;
}

void ring_buffer_write(ring_buffer_t *rb, uint8_t data)
{
    rb->buffer[rb->head] = data;
    rb->head = (rb->head + 1) % RING_BUFFER_SIZE;
    // 若缓冲区满，则覆盖最旧数据（可接受）
    if (rb->head == rb->tail) {
        rb->tail = (rb->tail + 1) % RING_BUFFER_SIZE;
    }
}

bool ring_buffer_is_full(ring_buffer_t *rb)
{
    return ((rb->head + 1) % RING_BUFFER_SIZE) == rb->tail;
}

void ring_buffer_clear(ring_buffer_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

