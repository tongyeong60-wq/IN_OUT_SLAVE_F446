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
    volatile uint32_t overrun_count;
    volatile uint32_t oversize_count;
    volatile uint8_t discard_until_lf;
    volatile uint8_t overrun_pending;
    volatile uint8_t oversize_pending;
} ringbuf_t;

typedef struct {
    uint32_t overrun_count;
    uint32_t oversize_count;
    uint8_t overrun_pending;
    uint8_t oversize_pending;
} ringbuf_diag_t;

void   ringbuf_init(ringbuf_t *rb, uint8_t *storage, size_t size);
bool   ringbuf_push_isr(ringbuf_t *rb, uint8_t c);
bool   ringbuf_pop(ringbuf_t *rb, uint8_t *out);
bool   ringbuf_readline(ringbuf_t *rb, char *out, size_t out_max);
void   ringbuf_abort_frame_isr(ringbuf_t *rb);
void   ringbuf_get_diag(ringbuf_t *rb, ringbuf_diag_t *diag, bool clear_pending);

#endif
