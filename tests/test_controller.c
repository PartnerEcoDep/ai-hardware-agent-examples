#include "ai_controller.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int starts, stops, prepares, provisions, texts, prepare_error, start_error;
    bool clock_ready, active;
    char text[128];
} host_t;
static bool ready(void *ctx) { return ((host_t *)ctx)->clock_ready; }
static int prepare(void *ctx) {
    host_t *h = ctx; assert(!h->active); ++h->prepares; return h->prepare_error;
}
static int start(void *ctx, uint32_t session) {
    host_t *h = ctx; assert(session); ++h->starts; h->active = true; return h->start_error;
}
static int stop(void *ctx) {
    host_t *h = ctx; ++h->stops; h->active = false; return 0;
}
static void provision(void *ctx) {
    host_t *h = ctx; assert(!h->active); ++h->provisions;
}
static void describe(void *ctx, char *text, size_t capacity) {
    (void)ctx; snprintf(text, capacity, "Network available");
}
static void network_text(void *ctx, const char *text) {
    host_t *h = ctx; ++h->texts; snprintf(h->text, sizeof(h->text), "%s", text);
}
static void event(ai_controller_t *c, ai_event_type_t type, uint32_t session, uint32_t now) {
    ai_message_t m = {.kind = AI_MESSAGE_EVENT, .data.event = {.type = type, .session = session}};
    ai_controller_receive(c, &m, now);
}
static void command(ai_controller_t *c, ai_command_t command, uint32_t now) {
    ai_message_t m = {.kind = AI_MESSAGE_COMMAND, .data.command = command};
    ai_controller_receive(c, &m, now);
}
int main(void) {
    host_t h = {0};
    ai_controller_t c;
    ai_controller_ops_t ops = {
        .session = {.ctx = &h, .start = start, .stop = stop},
        .time_ready = ready, .prepare = prepare, .provision = provision,
        .describe_network = describe, .network_text = network_text};
    assert(ai_controller_init(NULL, &ops) != 0);
    assert(ai_controller_init(&c, &ops) == 0);
    command(&c, AI_COMMAND_START, 0);
    assert(h.starts == 0 && h.prepares == 0);
    ai_controller_network(&c, true, 1);
    ai_controller_tick(&c, 1);
    assert(strstr(h.text, "Syncing time"));
    command(&c, AI_COMMAND_START, 2);
    assert(c.app.view.error == -8 && h.prepares == 0 && !h.active);
    h.clock_ready = true; h.prepare_error = -71;
    command(&c, AI_COMMAND_START, 3);
    assert(c.app.view.error == -71 && h.starts == 0);
    h.prepare_error = 0;
    command(&c, AI_COMMAND_START, 4);
    uint32_t old = c.app.session;
    assert(h.starts == 1 && h.prepares == 2 && h.active);
    command(&c, AI_COMMAND_START, 5);
    assert(h.starts == 1 && h.prepares == 2 && c.app.session == old);
    event(&c, AI_CONNECTED, old, 6);
    assert(c.app.view.state == AI_LISTENING);
    ai_controller_tick(&c, 501);
    assert(!strstr(h.text, "Syncing time"));
    int texts = h.texts;
    ai_controller_tick(&c, 1001);
    assert(h.texts == texts); /* unchanged text is not resent */
    command(&c, AI_COMMAND_PROVISION, 1002);
    assert(h.provisions == 1 && !h.active && !c.app.view.network_up);
    event(&c, AI_ANSWERING, old, 1003);
    command(&c, AI_COMMAND_START, 1004);
    assert(c.app.view.state == AI_OFFLINE && h.starts == 1);
    ai_controller_network(&c, true, 1005);
    command(&c, AI_COMMAND_START, 1006);
    assert(h.starts == 2);
    ai_controller_overflow(&c, 1007);
    assert(!h.active && c.app.view.error == -9);
    command(&c, AI_COMMAND_STOP, 1008);
    int stops = h.stops;
    command(&c, AI_COMMAND_STOP, 1009);
    assert(h.stops == stops);
    ai_controller_overflow(&c, 1010);
    assert(c.app.view.error == -9); /* idle overflow is visible too */
    command(&c, AI_COMMAND_START, UINT32_MAX - 1000);
    ai_controller_tick(&c, 100);
    assert(h.active);
    ai_controller_tick(&c, 30000);
    assert(!h.active && c.app.view.error == -2);
    h.start_error = -44;
    command(&c, AI_COMMAND_START, 30001);
    assert(!h.active && c.app.view.error == -44);
    h.start_error = 0;
    command(&c, AI_COMMAND_START, 30002);
    ai_controller_network(&c, false, 30003);
    assert(!h.active && c.app.view.state == AI_OFFLINE);
    ai_controller_network(&c, true, 30004);
    assert(!h.active); /* reconnect never auto-starts */
    ops.provision = NULL; /* Windows/other targets without Wi-Fi setup */
    assert(ai_controller_init(&c, &ops) == 0);
    ai_controller_network(&c, true, 0);
    command(&c, AI_COMMAND_START, 1);
    assert(ai_controller_command(&c, AI_COMMAND_PROVISION, 2) != 0 && h.active);
    assert(ai_controller_command(&c, (ai_command_t)999, 2) != 0 && h.active);
    command(&c, AI_COMMAND_STOP, 3);
    puts("controller: start retry, explicit commands, stop-before-provision, stale events, overflow and timeout passed");
    return 0;
}
