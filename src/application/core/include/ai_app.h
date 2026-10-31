#ifndef AI_APP_H
#define AI_APP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AI_OFFLINE, AI_READY, AI_CONNECTING, AI_LISTENING,
    AI_THINKING, AI_SPEAKING, AI_ERROR
} ai_state_t;

typedef enum {
    AI_NETWORK_UP, AI_NETWORK_DOWN, AI_TOGGLE, AI_STOP,
    AI_CONNECTED, AI_DISCONNECTED, AI_FAILED,
    AI_HEARING, AI_PROCESSING, AI_ANSWERING, AI_TURN_FINISHED,
    AI_START, AI_LOCAL_FAILED /* Owner commands/errors, not session callbacks. */
} ai_event_type_t;

typedef struct {
    ai_event_type_t type;
    uint32_t session; /* Required for cloud events; zero for user/network events. */
    int error;
} ai_event_t;

typedef struct {
    ai_state_t state;
    bool network_up;
    bool active;
    int error;
} ai_view_t;

typedef struct {
    void *ctx;
    int (*start)(void *ctx, uint32_t session);
    int (*stop)(void *ctx);
    void (*render)(void *ctx, const ai_view_t *view);
} ai_app_ops_t;

typedef struct {
    ai_app_ops_t ops;
    ai_view_t view;
    uint32_t session;
    uint32_t connecting_since;
} ai_app_t;

/* Single owner/event-loop API, no allocation, OS, SDK or GUI dependencies.
 * start/stop must not call back into this object: enqueue copied events instead.
 * render receives a borrowed snapshot and must copy it if consumed later.
 * stop is a barrier: media/SDK resources must be quiescent before it returns. */
int ai_app_init(ai_app_t *app, const ai_app_ops_t *ops);
void ai_app_dispatch(ai_app_t *app, const ai_event_t *event, uint32_t now_ms);
void ai_app_tick(ai_app_t *app, uint32_t now_ms);
const char *ai_state_name(ai_state_t state);

#ifdef __cplusplus
}
#endif
#endif
