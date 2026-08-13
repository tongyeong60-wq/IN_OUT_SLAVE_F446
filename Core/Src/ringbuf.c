/*
 * ringbuf.c
 *
 *  Created on: 2026. 1. 16.
 *      Author: VIEW
 */


#include "ringbuf.h"

static inline size_t rb_next(const ringbuf_t *rb, size_t v) {
    return (v + 1u) % rb->size;
}

void ringbuf_init(ringbuf_t *rb, uint8_t *storage, size_t size)
{
    rb->buf = storage;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
}

bool ringbuf_push_isr(ringbuf_t *rb, uint8_t c)
{
    size_t n = rb_next(rb, rb->head);
    if (n == rb->tail) return false; // overflow drop
    rb->buf[rb->head] = c;
    rb->head = n;
    return true;
}

bool ringbuf_pop(ringbuf_t *rb, uint8_t *out)
{
    if (rb->tail == rb->head) return false;
    *out = rb->buf[rb->tail];
    rb->tail = rb_next(rb, rb->tail);
    return true;
}

bool ringbuf_readline(ringbuf_t *rb, char *out, size_t out_max)
{
    if (!out || out_max < 2) return false;

    // '\n'이 있는지 먼저 스캔
    size_t t = rb->tail;
    bool found = false;
    while (t != rb->head) {
        uint8_t c = rb->buf[t];
        t = (t + 1u) % rb->size;
        if (c == '\n') { found = true; break; }
    }
    if (!found) return false;

    size_t i = 0;
    while (i < out_max - 1) {
        uint8_t c;
        if (!ringbuf_pop(rb, &c)) break;
        if (c == '\n') break;
        if (c == '\r') continue;
        out[i++] = (char)c;
    }
    out[i] = 0;
    return true;
}

