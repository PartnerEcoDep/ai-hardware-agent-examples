#include "ai_controller.h"
#include "ai_lvgl.h"
#include "ai_screen.h"
#include "linux_demo_session.h"
#include "linux_runtime.h"
#include "linux_network.h"
#include "linux_ui.h"
#include "lvgl.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    ai_controller_t controller;
    linux_demo_session_t demo;
    ai_command_t commands[16];
    unsigned command_count;
    bool overflow, online, force_online;
    uint32_t now;
} linux_app_t;
static linux_app_t host;
static volatile sig_atomic_t exiting;
static void signal_exit(int signal_number) { (void)signal_number; exiting = 1; }

/* SDL/LVGL callbacks only enqueue. The loop below owns controller and app. */
static void ui_command(ai_ui_command_t command) {
    if (host.command_count == sizeof(host.commands)/sizeof(host.commands[0])) {
        host.overflow = true; return;
    }
    host.commands[host.command_count++] = command == AI_UI_TOGGLE ? AI_COMMAND_TOGGLE : AI_COMMAND_PROVISION;
}
static bool time_ready(void *ctx) { (void)ctx; return true; } /* No TLS in demo mode. */
static int prepare(void *ctx) { (void)ctx; return 0; }
static int start(void *ctx, uint32_t session) {
    linux_app_t *h = ctx;
    return linux_demo_session_start(&h->demo, session, h->now);
}
static int stop(void *ctx) {
    linux_demo_session_stop(&((linux_app_t *)ctx)->demo);
    return 0;
}
static void render(void *ctx, const ai_view_t *view) {
    (void)ctx; ai_lvgl_render(view);
    printf("[demo] state=%s active=%d error=%d\n", ai_state_name(view->state), view->active, view->error);
}
static void describe_network(void *ctx, char *text, size_t size) {
    linux_app_t *h = ctx;
    snprintf(text, size, "Demo only - no audio/cloud\n%s", h->force_online ? "Simulated network" :
        h->online ? "Linux IP interface up" : "Linux network offline");
}
static void network_text(void *ctx, const char *text) { (void)ctx; ai_lvgl_network(text); }

static void service(uint32_t now) {
    host.now = now;
    ai_controller_network(&host.controller, host.online, now);
    if (host.overflow) {
        host.command_count = 0; host.overflow = false;
        ai_controller_overflow(&host.controller, now);
    } else {
        for (unsigned i = 0; i < host.command_count; ++i)
            ai_controller_command(&host.controller, host.commands[i], now);
        host.command_count = 0;
    }
    ai_event_t event;
    if (linux_demo_session_poll(&host.demo, now, &event)) {
        ai_message_t message = {.kind = AI_MESSAGE_EVENT, .data.event = event};
        ai_controller_receive(&host.controller, &message, now);
    }
    ai_controller_tick(&host.controller, now);
}

static void test_click(void) {
    linux_ui_test_pointer(AI_SCREEN_LEFT + 40, AI_SCREEN_BUTTON_Y + 20, true);
    linux_ui_test_pointer(AI_SCREEN_LEFT + 40, AI_SCREEN_BUTTON_Y + 20, false);
    linux_ui_pump();
}
static int smoke_test(void) {
    /* Real UI input path and controller, with virtual session time and network. */
    host.online = true; service(1000); lv_refr_now(NULL);
    test_click(); service(1000);
    if (host.controller.app.view.state != AI_CONNECTING) return 1;
    service(1500); if (host.controller.app.view.state != AI_LISTENING) return 2;
    service(3500); if (host.controller.app.view.state != AI_THINKING) return 3;
    service(5500); if (host.controller.app.view.state != AI_SPEAKING) return 4;
    service(8500); if (host.controller.app.view.state != AI_LISTENING) return 5;
    test_click(); service(8600);
    if (host.controller.app.view.state != AI_READY || host.demo.running) return 6;
    host.online = false; service(8700); test_click(); service(8800);
    if (host.controller.app.view.state != AI_OFFLINE || host.demo.running) return 7;
    host.online = true; service(8900); test_click(); service(9000);
    if (host.controller.app.view.state != AI_CONNECTING) return 8;
    host.online = false; service(9100); service(15000);
    if (host.controller.app.view.state != AI_OFFLINE || host.demo.running) return 9;
    puts("Linux SDL/LVGL/controller smoke test passed");
    return 0;
}

int main(int argc, char **argv) {
    unsigned scale = 2;
    bool test = false;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--demo-online")) host.force_online = true;
        else if (!strcmp(argv[i], "--smoke-test")) { test = true; host.force_online = true; }
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) {
            char *end; long value = strtol(argv[++i], &end, 10);
            if (*end || value < 1 || value > 4) { fputs("Scale must be 1..4\n", stderr); return 2; }
            scale = (unsigned)value;
        } else if (!strcmp(argv[i], "--help")) {
            puts("Usage: unified_ai_pi3_model_b [--scale 1..4] [--demo-online] [--smoke-test]\n"
                 "SDK-free simulated dialog. Uses existing Linux networking; no Wi-Fi setup or audio.");
            return 0;
        } else { fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]); return 2; }
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    signal(SIGINT, signal_exit); signal(SIGTERM, signal_exit);
    if (linux_ui_open(LINUX_BOARD_NAME " - DEMO (no SDK)", scale)) return 1;
    ai_lvgl_create(LINUX_BOARD_NAME, ui_command);
    ai_controller_ops_t ops = {
        .session = {.ctx = &host, .start = start, .stop = stop, .render = render},
        .time_ready = time_ready, .prepare = prepare,
        .describe_network = describe_network, .network_text = network_text
    };
    if (ai_controller_init(&host.controller, &ops)) { linux_ui_close(); return 1; }
    int result = 0;
    if (test) result = smoke_test();
    else {
        uint32_t checked = linux_now_ms() - 1000u;
        while (!exiting && linux_ui_pump()) {
            uint32_t now = linux_now_ms();
            if ((uint32_t)(now - checked) >= 1000u) {
                host.online = host.force_online || linux_network_up(); checked = now;
            }
            service(now); linux_sleep_ms(5);
        }
    }
    if (linux_ui_failed() && !result) result = 1;
    ai_controller_command(&host.controller, AI_COMMAND_STOP, host.now);
    linux_ui_close();
    if (result) fprintf(stderr, "Smoke test failed at step %d\n", result);
    return result;
}
