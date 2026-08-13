/*
 * ringbuf.h
 *
 *  Created on: 2026. 1. 16.
 *      Author: VIEW
 */

#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint8_t *buf;
    size_t   size;
    volatile size_t head;
    volatile size_t tail;
} ringbuf_t;

void   ringbuf_init(ringbuf_t *rb, uint8_t *storage, size_t size);
bool   ringbuf_push_isr(ringbuf_t *rb, uint8_t c);
bool   ringbuf_pop(ringbuf_t *rb, uint8_t *out);
bool   ringbuf_readline(ringbuf_t *rb, char *out, size_t out_max);

#endif

