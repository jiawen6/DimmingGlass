#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#define RING_BUFFER_SIZE 1024   // 可调整

typedef struct {
    uint8_t buffer[RING_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} ring_buffer_t;

void     ring_buffer_init(ring_buffer_t *rb);
uint16_t ring_buffer_available(ring_buffer_t *rb);
uint8_t  ring_buffer_read(ring_buffer_t *rb);
void     ring_buffer_write(ring_buffer_t *rb, uint8_t data);
bool     ring_buffer_is_full(ring_buffer_t *rb);
void     ring_buffer_clear(ring_buffer_t *rb);

#endif

