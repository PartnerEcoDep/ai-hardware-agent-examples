#include "ai_app.h"
#include <string.h>

static void render(ai_app_t *app) {
    if (app->ops.render) app->ops.render(app->ops.ctx, &app->view);
}

static void stop(ai_app_t *app, int error) {
    bool active = app->view.active;
    app->view.active = false;
    ++app->session; /* Invalidate callbacks already queued by the previous session. */
    int rc = active ? app->ops.stop(app->ops.ctx) : 0;
    app->view.error = error ? error : rc;
    app->view.state = app->view.error ? AI_ERROR :
        (app->view.network_up ? AI_READY : AI_OFFLINE);
}

int ai_app_init(ai_app_t *app, const ai_app_ops_t *ops) {
    if (!app || !ops || !ops->start || !ops->stop) return -1;
    memset(app, 0, sizeof(*app));
    app->ops = *ops;
    app->view.state = AI_OFFLINE;
    render(app);
    return 0;
}

void ai_app_dispatch(ai_app_t *app, const ai_event_t *event, uint32_t now_ms) {
    if (!app || !event) return;
    if (event->type >= AI_CONNECTED && event->type <= AI_TURN_FINISHED &&
        (!app->view.active || event->session != app->session)) return;
    switch (event->type) {
    case AI_NETWORK_UP:
        app->view.network_up = true;
        if (!app->view.active && app->view.state == AI_OFFLINE)
            app->view.state = AI_READY;
        break;
    case AI_NETWORK_DOWN:
        app->view.network_up = false;
        stop(app, 0);
        break;
    case AI_STOP:
        stop(app, 0);
        break;
    case AI_START:
        if (app->view.active) return; /* Explicit Start never toggles an active session. */
        /* fall through */
    case AI_TOGGLE:
        if (app->view.active) {
            stop(app, 0);
        } else if (app->view.network_up) {
            ++app->session;
            app->view.active = true;
            app->view.error = 0;
            app->view.state = AI_CONNECTING;
            app->connecting_since = now_ms;
            render(app); /* Show connecting even if the SDK start call blocks. */
            int rc = app->ops.start(app->ops.ctx, app->session);
            if (rc) stop(app, rc);
        }
        break;
    case AI_CONNECTED:
    case AI_HEARING:
    case AI_TURN_FINISHED:
        app->view.state = AI_LISTENING;
        break;
    case AI_PROCESSING:
        app->view.state = AI_THINKING;
        break;
    case AI_ANSWERING:
        app->view.state = AI_SPEAKING;
        break;
    case AI_DISCONNECTED:
    case AI_FAILED:
    case AI_LOCAL_FAILED:
        stop(app, event->error ? event->error : -1);
        break;
    default:
        return;
    }
    render(app);
}

void ai_app_tick(ai_app_t *app, uint32_t now_ms) {
    if (app && app->view.state == AI_CONNECTING &&
        (uint32_t)(now_ms - app->connecting_since) >= 30000u) {
        stop(app, -2);
        render(app);
    }
}

const char *ai_state_name(ai_state_t state) {
    static const char *const names[] = {
        "Offline", "Ready", "Connecting", "Listening", "Thinking", "Speaking", "Error"
    };
    return (unsigned)state < sizeof(names) / sizeof(names[0]) ? names[state] : "Unknown";
}
