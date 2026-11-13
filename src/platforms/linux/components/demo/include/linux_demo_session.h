#ifndef LINUX_DEMO_SESSION_H
#define LINUX_DEMO_SESSION_H

#include "ai_app.h"

/* SDK-free UI demonstration, not microphone recognition or cloud audio.
 * All calls belong to the application owner. Times are monotonic milliseconds.
 * Zero initialization is sufficient; polling emits at most one copied event.
 * A delayed poll starts the next phase from that poll, avoiding catch-up bursts.
 */
typedef struct {
    bool running;
    uint32_t session;
    unsigned phase;
    uint32_t phase_since;
} linux_demo_session_t;

int linux_demo_session_start(linux_demo_session_t *demo, uint32_t session,
                             uint32_t now_ms);
void linux_demo_session_stop(linux_demo_session_t *demo);
bool linux_demo_session_poll(linux_demo_session_t *demo, uint32_t now_ms,
                             ai_event_t *event);

#endif
