#ifndef AI_CONTROLLER_H
#define AI_CONTROLLER_H
#include "ai_app.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AI_COMMAND_TOGGLE, AI_COMMAND_START, AI_COMMAND_STOP, AI_COMMAND_PROVISION
} ai_command_t;
typedef enum { AI_MESSAGE_EVENT, AI_MESSAGE_COMMAND } ai_message_kind_t;
typedef struct {
    ai_message_kind_t kind;
    union { ai_event_t event; ai_command_t command; } data;
} ai_message_t;

typedef struct {
    /* All callbacks use session.ctx and run on the application owner. */
    ai_app_ops_t session;
    bool (*time_ready)(void *ctx);
    int (*prepare)(void *ctx); /* Rebuild configuration before every new session. */
    void (*provision)(void *ctx); /* NULL when this target has no Wi-Fi setup. */
    void (*describe_network)(void *ctx, char *text, size_t capacity);
    void (*network_text)(void *ctx, const char *text);
} ai_controller_ops_t;
typedef struct {
    ai_app_t app;
    ai_controller_ops_t ops;
    uint32_t description_at;
    bool description_valid;
    char description[128];
} ai_controller_t;

/* Platform callbacks enqueue copied messages; only the owner calls these APIs.
 * Poll real network state before consuming commands. After overflow, clear the
 * platform mailbox before calling overflow; never replay discarded commands. */
int ai_controller_init(ai_controller_t *controller, const ai_controller_ops_t *ops);
void ai_controller_network(ai_controller_t *controller, bool online, uint32_t now);
int ai_controller_command(ai_controller_t *controller, ai_command_t command, uint32_t now);
void ai_controller_receive(ai_controller_t *controller, const ai_message_t *message, uint32_t now);
void ai_controller_overflow(ai_controller_t *controller, uint32_t now);
void ai_controller_tick(ai_controller_t *controller, uint32_t now);
#ifdef __cplusplus
}
#endif
#endif
