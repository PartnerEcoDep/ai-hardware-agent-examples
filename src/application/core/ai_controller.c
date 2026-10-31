#include "ai_controller.h"
#include <stdio.h>
#include <string.h>

static int start(void *ctx, uint32_t session) {
    ai_controller_t *c = ctx;
    void *host = c->ops.session.ctx;
    if (!c->ops.time_ready(host)) return -8;
    int rc = c->ops.prepare(host);
    return rc ? rc : c->ops.session.start(host, session);
}
static int stop(void *ctx) {
    ai_controller_t *c = ctx;
    return c->ops.session.stop(c->ops.session.ctx);
}
static void render(void *ctx, const ai_view_t *view) {
    ai_controller_t *c = ctx;
    if (c->ops.session.render) c->ops.session.render(c->ops.session.ctx, view);
}
int ai_controller_init(ai_controller_t *c, const ai_controller_ops_t *ops) {
    if (!c || !ops || !ops->session.start || !ops->session.stop ||
        !ops->time_ready || !ops->prepare) return -1;
    memset(c, 0, sizeof(*c));
    c->ops = *ops;
    ai_app_ops_t app_ops = {.ctx = c, .start = start, .stop = stop, .render = render};
    return ai_app_init(&c->app, &app_ops);
}
void ai_controller_network(ai_controller_t *c, bool online, uint32_t now) {
    if (c->app.view.network_up == online) return;
    ai_event_t event = {.type = online ? AI_NETWORK_UP : AI_NETWORK_DOWN};
    ai_app_dispatch(&c->app, &event, now);
    c->description_valid = false;
}
int ai_controller_command(ai_controller_t *c, ai_command_t command, uint32_t now) {
    ai_event_t event = {0};
    switch (command) {
    case AI_COMMAND_TOGGLE: event.type = AI_TOGGLE; break;
    case AI_COMMAND_START: event.type = AI_START; break;
    case AI_COMMAND_STOP: event.type = AI_STOP; break;
    case AI_COMMAND_PROVISION:
        if (!c->ops.provision) return -1;
        /* Stop/join the session before the backend changes network interfaces. */
        event.type = AI_NETWORK_DOWN;
        ai_app_dispatch(&c->app, &event, now);
        c->ops.provision(c->ops.session.ctx);
        c->description_valid = false;
        return 0;
    default: return -1;
    }
    ai_app_dispatch(&c->app, &event, now);
    return 0;
}
void ai_controller_receive(ai_controller_t *c, const ai_message_t *message, uint32_t now) {
    if (!message) return;
    if (message->kind == AI_MESSAGE_COMMAND)
        ai_controller_command(c, message->data.command, now);
    else if (message->kind == AI_MESSAGE_EVENT)
        ai_app_dispatch(&c->app, &message->data.event, now);
}
void ai_controller_overflow(ai_controller_t *c, uint32_t now) {
    ai_event_t event = {.type = AI_LOCAL_FAILED, .error = -9};
    ai_app_dispatch(&c->app, &event, now);
}
void ai_controller_tick(ai_controller_t *c, uint32_t now) {
    ai_app_tick(&c->app, now);
    if (!c->ops.describe_network || !c->ops.network_text) return;
    if (c->description_valid && (uint32_t)(now - c->description_at) < 500) return;
    c->description_at = now;
    char text[sizeof(c->description)] = {0};
    c->ops.describe_network(c->ops.session.ctx, text, sizeof(text));
    text[sizeof(text) - 1] = 0;
    if (c->app.view.network_up && !c->ops.time_ready(c->ops.session.ctx)) {
        size_t used = strlen(text);
        snprintf(text + used, sizeof(text) - used, "\nSyncing time...");
    }
    if (!c->description_valid || strcmp(text, c->description)) {
        memcpy(c->description, text, sizeof(text));
        c->ops.network_text(c->ops.session.ctx, text);
    }
    c->description_valid = true;
}
