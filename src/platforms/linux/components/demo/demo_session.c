#include "linux_demo_session.h"
#include <string.h>

int linux_demo_session_start(linux_demo_session_t *demo, uint32_t session,
                             uint32_t now_ms) {
    if (!demo) return -1;
    *demo = (linux_demo_session_t){
        .running = true, .session = session, .phase_since = now_ms
    };
    return 0;
}

void linux_demo_session_stop(linux_demo_session_t *demo) {
    if (demo) memset(demo, 0, sizeof(*demo));
}

bool linux_demo_session_poll(linux_demo_session_t *demo, uint32_t now_ms,
                             ai_event_t *event) {
    static const uint32_t durations[] = {500, 2000, 2000, 3000};
    static const ai_event_type_t events[] = {
        AI_CONNECTED, AI_PROCESSING, AI_ANSWERING, AI_TURN_FINISHED
    };
    if (!demo || !event || !demo->running || demo->phase >= 4) return false;
    if ((uint32_t)(now_ms - demo->phase_since) < durations[demo->phase]) return false;
    *event = (ai_event_t){.type = events[demo->phase], .session = demo->session};
    demo->phase_since = now_ms;
    demo->phase = demo->phase == 3 ? 1 : demo->phase + 1;
    return true;
}
