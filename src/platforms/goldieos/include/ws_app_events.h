#ifndef WS_APP_EVENTS_H
#define WS_APP_EVENTS_H
#include "ai_controller.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Application-owned mailbox; callbacks enqueue copied events. */
void ws_post(void *ctx, const ai_event_t *event);
void ws_command(ai_command_t command);
#ifdef __cplusplus
}
#endif
#endif
