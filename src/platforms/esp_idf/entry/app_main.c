#include "ai_controller.h"
#include "ai_cloud.h"
#include "ai_ui.h"
#include "board.h"
#include "esp_media.h"
#include "esp_network.h"
#include "esp_config.h"
#include "convai_platform_esp32.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdlib.h>

static QueueHandle_t events;
static atomic_bool overflow;

static void enqueue(const ai_message_t *message) {
    if (xQueueSend(events, message, 0) != pdTRUE) atomic_store(&overflow, true);
}
static void post(void *ctx, const ai_event_t *event) {
    (void)ctx;
    ai_message_t message = {.kind = AI_MESSAGE_EVENT, .data.event = *event};
    enqueue(&message);
}
static void ui_command(ai_ui_command_t command) {
    ai_message_t message = {.kind = AI_MESSAGE_COMMAND,
        .data.command = command == AI_UI_PROVISION ? AI_COMMAND_PROVISION : AI_COMMAND_TOGGLE};
    enqueue(&message);
}
static bool time_ready(void *ctx) { (void)ctx; return esp_network_time_ready(); }
static int prepare(void *ctx) {
    (void)ctx;
    ai_cloud_config_t config = {.media = esp_media_ops(), .post = post};
    int rc = esp_cloud_config(&config);
    return rc ? rc : ai_cloud_init(&config);
}
static int start(void *ctx, uint32_t session) {
    (void)ctx;
    esp_media_set_session(session);
    return ai_cloud_start(NULL, session);
}
static void provision(void *ctx) { (void)ctx; esp_network_provision(); }
static void describe(void *ctx, char *text, size_t capacity) {
    (void)ctx; esp_network_description(text, capacity);
}
static void network_text(void *ctx, const char *text) { (void)ctx; ai_ui_network(text); }
static void require_ok(int rc, const char *step) {
    if (!rc) return;
    ESP_LOGE("app", "%s failed: %d", step, rc);
    /* Hardware/resource failures cannot leave a half-initialized app running. */
    abort();
}

void app_main(void) {
    events = xQueueCreate(32, sizeof(ai_message_t));
    require_ok(events ? 0 : -1, "event queue");
    require_ok(board_init(), "board");
    require_ok(ai_ui_init(ui_command), "UI");
    require_ok(esp_media_init(post, NULL), "media");
    require_ok(convai_platform_esp32_init(), "SDK platform");
    int config_error = prepare(NULL);
    if (config_error) ESP_LOGW("app", "Cloud configuration deferred; retry with Start (%d)", config_error);
    ai_controller_t controller;
    ai_controller_ops_t ops = {
        .session = {.start = start, .stop = ai_cloud_stop, .render = ai_ui_render},
        .time_ready = time_ready, .prepare = prepare, .provision = provision,
        .describe_network = describe, .network_text = network_text};
    require_ok(ai_controller_init(&controller, &ops), "application");
    require_ok(esp_network_init(), "network");
    bool was_down = false;
    uint32_t pressed_at = 0;
    for (;;) {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        esp_network_poll();
        ai_controller_network(&controller, esp_network_connected(), now);
        if (atomic_exchange(&overflow, false)) {
            /* Never silently lose a stop/disconnect when the queue is saturated. */
            xQueueReset(events);
            ai_controller_overflow(&controller, now);
        }
        ai_message_t message;
        if (xQueueReceive(events, &message, pdMS_TO_TICKS(20)) == pdTRUE)
            ai_controller_receive(&controller, &message, (uint32_t)(esp_timer_get_time() / 1000));
        bool down = board_button_down();
        if (down && !was_down) pressed_at = now;
        if (!down && was_down && (uint32_t)(now - pressed_at) >= 50)
            ai_controller_command(&controller, AI_COMMAND_TOGGLE, now);
        was_down = down;
        ai_controller_tick(&controller, (uint32_t)(esp_timer_get_time() / 1000));
    }
}
