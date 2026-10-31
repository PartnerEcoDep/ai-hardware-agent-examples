#ifndef AI_CLOUD_H
#define AI_CLOUD_H
#include "ai_app.h"
#include "ai_media.h"
#include <stddef.h>
#include <stdint.h>


typedef struct {
    const char *config_json;
    const char *agent_id;
    const char *startup_json;
    ai_media_ops_t media;
    void *event_ctx;
    /* Must copy/enqueue the event; never call ai_app_dispatch inline. */
    void (*post)(void *ctx, const ai_event_t *event);
} ai_cloud_config_t;

/* One cloud session per device. Init/config pointers live for the application
 * lifetime. Lifecycle calls are serialized by the app owner; no GUI/RTOS API.
 * The selected SDK port must join callback threads before destroy returns. */
int ai_cloud_init(const ai_cloud_config_t *config);
int ai_cloud_start(void *unused, uint32_t session);
int ai_cloud_stop(void *unused);
#endif
