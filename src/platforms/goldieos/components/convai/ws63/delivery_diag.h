#ifndef WS63_DELIVERY_DIAG_H
#define WS63_DELIVERY_DIAG_H

#include <stdbool.h>
#include <stdint.h>

/* Temporary, passive counters owned by the SDK I/O thread. No TLS pointers or
 * payloads are retained. pending is a snapshot after the latest TLS read, NOT
 * socket readiness; it must never be used to change poll results. */
typedef struct {
    uint32_t polls, read_ready, write_ready, pending_wait;
    uint32_t bio_rx, bio_tx, plain_rx, plain_tx;
    uint32_t reads, wants, errors, read_max_ms;
} ws63_delivery_window_t;

typedef struct {
    ws63_delivery_window_t window;
    uint32_t report_at, read_at, pending;
    bool read_seen;
} ws63_delivery_diag_t;

static inline void ws63_delivery_read(ws63_delivery_diag_t *d, uint32_t now,
                                     uint32_t elapsed, int result,
                                     bool would_block, uint32_t pending) {
    ++d->window.reads;
    if (result > 0) d->window.plain_rx += (uint32_t)result;
    else if (would_block) ++d->window.wants;
    else ++d->window.errors; /* Includes peer close (zero bytes). */
    if (elapsed > d->window.read_max_ms) d->window.read_max_ms = elapsed;
    d->read_seen = true;
    d->read_at = now;
    d->pending = pending;
}

static inline void ws63_delivery_poll(ws63_delivery_diag_t *d, bool requested_read,
                                     bool read_ready, bool write_ready) {
    ++d->window.polls;
    if (read_ready) ++d->window.read_ready;
    if (write_ready) ++d->window.write_ready;
    if (requested_read && !read_ready && d->read_seen && d->pending)
        ++d->window.pending_wait;
}

static inline bool ws63_delivery_due(const ws63_delivery_diag_t *d, uint32_t now) {
    return (uint32_t)(now - d->report_at) >= 1000;
}

static inline void ws63_delivery_reported(ws63_delivery_diag_t *d, uint32_t now) {
    d->window = (ws63_delivery_window_t){0};
    d->report_at = now;
    /* Preserve the last-read snapshot so a quiet interval remains visible. */
}

#endif
