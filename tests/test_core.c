#include "ai_app.h"
#include <assert.h>
#include <stdio.h>
static int starts, stops, result;
static uint32_t started_session;
static int start(void *ctx, uint32_t session) {
    (void)ctx; ++starts; started_session = session; return result;
}
static int stop(void *ctx) { (void)ctx; ++stops; return 0; }
static void dispatch(ai_app_t *app, ai_event_type_t type, uint32_t session, uint32_t now) {
    ai_event_t event = {.type = type, .session = session};
    ai_app_dispatch(app, &event, now);
}
int main(void) {
    ai_app_t app;
    ai_app_ops_t ops = {.start = start, .stop = stop};
    assert(ai_app_init(NULL, &ops) != 0);
    assert(ai_app_init(&app, &ops) == 0);
    dispatch(&app, AI_TOGGLE, 0, 0);
    assert(!starts && app.view.state == AI_OFFLINE);
    dispatch(&app, AI_NETWORK_UP, 0, 0);
    dispatch(&app, AI_TOGGLE, 0, 1);
    uint32_t first = started_session;
    assert(starts == 1 && app.view.state == AI_CONNECTING);
    dispatch(&app, AI_CONNECTED, first, 2);
    dispatch(&app, AI_PROCESSING, first, 3);
    assert(app.view.state == AI_THINKING);
    dispatch(&app, AI_ANSWERING, first, 4);
    assert(app.view.state == AI_SPEAKING);
    dispatch(&app, AI_TURN_FINISHED, first, 5);
    assert(app.view.state == AI_LISTENING);
    dispatch(&app, AI_TOGGLE, 0, 6);
    assert(stops == 1 && !app.view.active && app.view.state == AI_READY);
    dispatch(&app, AI_CONNECTED, first, 7);
    assert(app.view.state == AI_READY);
    dispatch(&app, AI_TOGGLE, 0, 8);
    uint32_t second = started_session;
    dispatch(&app, AI_FAILED, first, 9);
    assert(app.view.active && app.view.state == AI_CONNECTING);
    dispatch(&app, AI_CONNECTED, second, 10);
    dispatch(&app, AI_NETWORK_DOWN, 0, 11);
    assert(stops == 2 && !app.view.active && app.view.state == AI_OFFLINE);
    dispatch(&app, AI_NETWORK_UP, 0, 12);
    result = -22;
    dispatch(&app, AI_TOGGLE, 0, 13);
    assert(app.view.state == AI_ERROR && app.view.error == -22 && stops == 3);
    result = 0;
    dispatch(&app, AI_TOGGLE, 0, UINT32_MAX - 1000);
    ai_app_tick(&app, 100);
    assert(app.view.state == AI_CONNECTING);
    ai_app_tick(&app, 30000);
    assert(app.view.state == AI_ERROR && app.view.error == -2 && stops == 4);
    for (int i = 0; i < 100; ++i) {
        dispatch(&app, AI_TOGGLE, 0, 0);
        dispatch(&app, AI_CONNECTED, started_session, 0);
        dispatch(&app, AI_STOP, 0, 0);
        dispatch(&app, AI_ANSWERING, started_session, 0);
        assert(!app.view.active && app.view.state == AI_READY);
    }
    puts("core lifecycle, stale callbacks, disconnect, failure, timeout wrap and 100 restarts passed");
}
