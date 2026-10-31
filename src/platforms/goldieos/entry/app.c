#include "ws_media.h"
#include "ws_ui.h"
#include "ws_app_events.h"
#include "ws_network.h"
#include "ws_config.h"
#include "goldie_osal.h"
#include "runtime.h"
#include <stdatomic.h>
#include <stdio.h>
extern unsigned int sys_now(void);
static goldie_mutex mailbox;
static ai_message_t events[32];
static unsigned head, count;
static bool overflow;
static atomic_bool time_ready;

static void enqueue(const ai_message_t *message) {
    goldie_mutex_lock(&mailbox);
    if (count == 32) overflow = true;
    else { events[(head + count) % 32] = *message; ++count; }
    goldie_mutex_unlock(&mailbox);
}
void ws_post(void *ctx, const ai_event_t *event) {
    (void)ctx;
    ai_message_t message = {.kind = AI_MESSAGE_EVENT, .data.event = *event};
    enqueue(&message);
}
void ws_command(ai_command_t command) {
    ai_message_t message = {.kind = AI_MESSAGE_COMMAND, .data.command = command};
    enqueue(&message);
}
static bool clock_ready(void *ctx) { (void)ctx; return atomic_load(&time_ready); }
static int prepare(void *ctx) {
    (void)ctx;
    ai_cloud_config_t cfg = {.media = ws_media_ops(), .post = ws_post};
    int rc = ws_cloud_config(&cfg);
    return rc ? rc : ai_cloud_init(&cfg);
}
static int start(void *ctx, uint32_t session) {
    (void)ctx;
    ws_media_session(session);
    return ai_cloud_start(NULL, session);
}
#ifndef AI_UI_NO_PROVISION
static void provision(void *ctx) { (void)ctx; ws_network_provision(); }
#endif
static void describe(void *ctx, char *text, size_t capacity) {
    (void)ctx; ws_network_description(text, capacity);
}
static void network_text(void *ctx, const char *text) { (void)ctx; ws_ui_network(text); }
static int sync_clock(void *ctx) {
    (void)ctx;
    for (;;) {
        if (ws_network_connected() && !atomic_load(&time_ready) && ws_runtime_sync_time() == 0)
            atomic_store(&time_ready, true);
        goldie_msleep(30000);
    }
    return 0;
}
static void require(int rc, const char *step) {
    if (!rc) return;
    printf("[unified] %s failed: %d\n", step, rc);
    char message[128];
    snprintf(message, sizeof(message), "Initialization failed: %s (%d)", step, rc);
    ws_ui_network(message);
    for (;;) goldie_msleep(1000);
}
static int app_task(void *ctx) {
    (void)ctx;
    goldie_mutex_init(&mailbox);
    ws_runtime_prepare();
    ws_media_prepare();
    require(ws_ui_init(), "UI");
    require(ws_runtime_init(), "runtime");
    require(ws_media_init(), "media");
    require(ws_runtime_sdk_init(), "SDK HAL");
    int config_error = prepare(NULL);
    if (config_error) printf("[unified] Cloud configuration deferred; retry with Start (%d)\n", config_error);
    ai_controller_t controller;
    ai_controller_ops_t ops = {
        .session = {.start = start, .stop = ai_cloud_stop, .render = ws_ui_render},
        .time_ready = clock_ready, .prepare = prepare,
#ifndef AI_UI_NO_PROVISION
        .provision = provision,
#endif
        .describe_network = describe, .network_text = network_text};
    require(ai_controller_init(&controller, &ops), "application");
    ws_network_enable(true);
    require(ws_network_init(), "network");
    require(goldie_thread_create(sync_clock, NULL, "ai_time", 4096) ? 0 : -1, "time task");
    for (;;) {
        uint32_t now = sys_now();
        ai_controller_network(&controller, ws_network_connected(), now);
        ai_message_t message; bool have = false, lost;
        goldie_mutex_lock(&mailbox);
        lost = overflow;
        if (lost) { overflow = false; count = 0; head = 0; }
        else if (count) { message = events[head]; head = (head + 1) % 32; --count; have = true; }
        goldie_mutex_unlock(&mailbox);
        if (lost) ai_controller_overflow(&controller, now);
        if (have) ai_controller_receive(&controller, &message, now);
        ai_controller_tick(&controller, sys_now());
        goldie_msleep(20);
    }
    return 0;
}
static void standalone_entry(void) {
    if (!goldie_thread_create(app_task, NULL, "unified_ai", 8192))
        printf("[unified] Cannot create application task\n");
}
GOLDIE_INIT_CALL_(standalone_entry);
