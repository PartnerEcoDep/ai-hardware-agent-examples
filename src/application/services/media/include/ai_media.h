#ifndef AI_MEDIA_H
#define AI_MEDIA_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* send result: 0 accepted; +1 temporarily paused; negative = failure. */
typedef int (*ai_uplink_fn)(void *ctx, const uint8_t *data, size_t size);
typedef struct {
    void *ctx;
    /* enable(false) is an uplink barrier: no send callback may run afterwards.
     * reset discards queued playback; push copies borrowed G711A data.
     * Operations must be safe from the SDK callback thread. */
    void (*enable)(void *ctx, bool enabled, ai_uplink_fn send, void *send_ctx);
    void (*reset)(void *ctx);
    int (*push)(void *ctx, const uint8_t *data, size_t size);
    void (*finish)(void *ctx);
} ai_media_ops_t;
#endif
