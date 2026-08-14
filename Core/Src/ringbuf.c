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
    rb->overrun_count = 0;
    rb->oversize_count = 0;
    rb->discard_until_lf = 0;
    rb->overrun_pending = 0;
    rb->oversize_pending = 0;
}

bool ringbuf_push_isr(ringbuf_t *rb, uint8_t c)
{
    if (rb->discard_until_lf) {
        if (c == '\n') rb->discard_until_lf = 0;
        return false;
    }

    size_t n = rb_next(rb, rb->head);
    if (n == rb->tail) {
        rb->overrun_count++;
        rb->overrun_pending = 1;
        rb->tail = rb->head;  /* ISR preempts the sole consumer: invalidate partial frame. */
        rb->discard_until_lf = (c == '\n') ? 0u : 1u;
        return false;
    }
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
    size_t line_len = 0;
    bool found = false;
    while (t != rb->head) {
        uint8_t c = rb->buf[t];
        t = (t + 1u) % rb->size;
        if (c == '\n') { found = true; break; }
        if (c != '\r') line_len++;
    }
    if (!found) return false;

    if (line_len >= out_max) {
        uint8_t c;
        do {
            if (!ringbuf_pop(rb, &c)) break;
        } while (c != '\n');
        rb->oversize_count++;
        rb->oversize_pending = 1;
        out[0] = 0;
        return false;
    }

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

void ringbuf_abort_frame_isr(ringbuf_t *rb)
{
    rb->tail = rb->head;
    rb->discard_until_lf = 1;
}

void ringbuf_get_diag(ringbuf_t *rb, ringbuf_diag_t *diag, bool clear_pending)
{
    if (!diag) return;
    diag->overrun_count = rb->overrun_count;
    diag->oversize_count = rb->oversize_count;
    diag->overrun_pending = rb->overrun_pending;
    diag->oversize_pending = rb->oversize_pending;
    if (clear_pending) {
        if (rb->overrun_count == diag->overrun_count) rb->overrun_pending = 0;
        if (rb->oversize_count == diag->oversize_count) rb->oversize_pending = 0;
    }
}
