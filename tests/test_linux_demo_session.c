#include "linux_demo_session.h"
#include <assert.h>
#include <stdio.h>

typedef struct {
    linux_demo_session_t demo;
    uint32_t now;
} host_t;

static int start(void *ctx, uint32_t session) {
    host_t *host = ctx;
    return linux_demo_session_start(&host->demo, session, host->now);
}

static int stop(void *ctx) {
    linux_demo_session_stop(&((host_t *)ctx)->demo);
    return 0;
}

static void expect(linux_demo_session_t *demo, uint32_t now,
                   ai_event_type_t type, uint32_t session) {
    ai_event_t event;
    assert(linux_demo_session_poll(demo, now, &event));
    assert(event.type == type && event.session == session && event.error == 0);
    assert(!linux_demo_session_poll(demo, now, &event));
}

int main(void) {
    linux_demo_session_t demo = {0};
    ai_event_t event = {.type = AI_FAILED, .error = -123};
    assert(!linux_demo_session_poll(&demo, 0, &event));
    assert(linux_demo_session_start(NULL, 1, 0) == -1);
    assert(!linux_demo_session_poll(NULL, 0, &event));
    linux_demo_session_stop(NULL);

    assert(linux_demo_session_start(&demo, 7, 100) == 0);
    assert(!linux_demo_session_poll(&demo, 599, &event));
    assert(event.type == AI_FAILED && event.error == -123);
    assert(!linux_demo_session_poll(&demo, 600, NULL));
    expect(&demo, 600, AI_CONNECTED, 7);
    expect(&demo, 2600, AI_PROCESSING, 7);
    expect(&demo, 4600, AI_ANSWERING, 7);
    assert(!linux_demo_session_poll(&demo, 7599, &event));
    expect(&demo, 7600, AI_TURN_FINISHED, 7);
    expect(&demo, 9600, AI_PROCESSING, 7);

    linux_demo_session_stop(&demo);
    assert(!demo.running);
    assert(!linux_demo_session_poll(&demo, 50000, &event));
    assert(linux_demo_session_start(&demo, 8, 50000) == 0);
    assert(!linux_demo_session_poll(&demo, 50499, &event));
    expect(&demo, 50500, AI_CONNECTED, 8);

    /* Unsigned elapsed timing crosses the monotonic clock's 32-bit wrap. */
    assert(linux_demo_session_start(&demo, 9, UINT32_MAX - 249u) == 0);
    assert(!linux_demo_session_poll(&demo, 249, &event));
    expect(&demo, 250, AI_CONNECTED, 9);
    expect(&demo, 100000, AI_PROCESSING, 9);
    assert(!linux_demo_session_poll(&demo, 100001, &event));

    /* Exercise actual app session filtering across Stop/Start, including a
     * previously copied demo event that remained in a caller's event queue. */
    host_t host = {0};
    ai_app_t app;
    ai_app_ops_t ops = {.ctx = &host, .start = start, .stop = stop};
    assert(ai_app_init(&app, &ops) == 0);
    ai_event_t online = {.type = AI_NETWORK_UP}, toggle = {.type = AI_TOGGLE};
    ai_app_dispatch(&app, &online, 0);
    ai_app_dispatch(&app, &toggle, 0);
    assert(app.view.state == AI_CONNECTING);
    assert(linux_demo_session_poll(&host.demo, 500, &event));
    ai_event_t stale = event;
    ai_app_dispatch(&app, &event, 500);
    assert(app.view.state == AI_LISTENING);
    ai_app_dispatch(&app, &toggle, 501);
    assert(app.view.state == AI_READY && !host.demo.running);
    host.now = 502;
    ai_app_dispatch(&app, &toggle, host.now);
    ai_app_dispatch(&app, &stale, 503);
    assert(app.view.state == AI_CONNECTING);
    assert(linux_demo_session_poll(&host.demo, 1002, &event));
    assert(event.session != stale.session);
    ai_app_dispatch(&app, &event, 1002);
    assert(app.view.state == AI_LISTENING);
    ai_event_t offline = {.type = AI_NETWORK_DOWN};
    ai_app_dispatch(&app, &offline, 1003);
    assert(app.view.state == AI_OFFLINE && !host.demo.running);

    puts("Linux demo timing, cancellation and app session isolation passed");
    return 0;
}
