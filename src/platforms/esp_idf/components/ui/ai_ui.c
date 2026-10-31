#include "ai_ui.h"
#include "ai_lvgl.h"
#include "board.h"
#include "board_display.h"
#include "lvgl.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

typedef struct { bool pressed; int x, y; } touch_state_t;
typedef struct { char text[128]; } network_text_t;
static QueueHandle_t view_queue, network_queue, touch_queue;
static void (*on_command)(ai_ui_command_t);

static uint32_t tick_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
static bool flush_done(esp_lcd_panel_io_handle_t io,
                       esp_lcd_panel_io_event_data_t *event, void *ctx) {
    (void)io; (void)event;
    /* Called after DMA completion, not immediately after queuing the transfer. */
    lv_display_flush_ready((lv_display_t *)ctx);
    return false;
}
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels) {
    if (esp_lcd_panel_draw_bitmap(board_display_panel(), area->x1, area->y1,
        area->x2 + 1, area->y2 + 1, pixels) != ESP_OK)
        lv_display_flush_ready(display);
}
static void read_touch(lv_indev_t *input, lv_indev_data_t *data) {
    (void)input;
    touch_state_t touch = {0};
    xQueuePeek(touch_queue, &touch, 0);
    data->state = touch.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->point.x = touch.x; data->point.y = touch.y;
}
static void touch_task(void *arg) {
    (void)arg;
    for (;;) {
        touch_state_t touch = {0};
        touch.pressed = board_touch_read(&touch.x, &touch.y);
        xQueueOverwrite(touch_queue, &touch);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
static void ui_task(void *arg) {
    (void)arg;
    for (;;) {
        ai_view_t view;
        if (xQueueReceive(view_queue, &view, 0) == pdTRUE) ai_lvgl_render(&view);
        network_text_t network;
        if (xQueueReceive(network_queue, &network, 0) == pdTRUE)
            ai_lvgl_network(network.text);
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
int ai_ui_init(void (*command)(ai_ui_command_t)) {
    if (!command) return -1;
    on_command = command;
    view_queue = xQueueCreate(1, sizeof(ai_view_t));
    network_queue = xQueueCreate(1, sizeof(network_text_t));
    touch_queue = xQueueCreate(1, sizeof(touch_state_t));
    const size_t size = BOARD_DISPLAY_WIDTH * 20 * 2;
    void *buffer = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!view_queue || !network_queue || !touch_queue || !buffer) return -1;
    lv_init();
    lv_tick_set_cb(tick_ms);
    lv_display_t *display = lv_display_create(BOARD_DISPLAY_WIDTH, BOARD_DISPLAY_HEIGHT);
    if (!display) return -1;
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buffer, NULL, size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    esp_lcd_panel_io_callbacks_t callbacks = {.on_color_trans_done = flush_done};
    if (esp_lcd_panel_io_register_event_callbacks(board_display_io(), &callbacks, display) != ESP_OK)
        return -1;
    lv_indev_t *input = lv_indev_create();
    if (!input) return -1;
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input, read_touch);
    ai_lvgl_create(board_profile.name, on_command);
    if (xTaskCreate(touch_task, "ai_touch", 3072, NULL, 2, NULL) != pdPASS) return -1;
    return xTaskCreate(ui_task, "ai_ui", 6144, NULL, 4, NULL) == pdPASS ? 0 : -1;
}
void ai_ui_render(void *unused, const ai_view_t *view) {
    (void)unused;
    if (view_queue && view) xQueueOverwrite(view_queue, view);
}
void ai_ui_network(const char *description) {
    network_text_t text = {0};
    snprintf(text.text, sizeof(text.text), "%s", description ? description : "");
    if (network_queue) xQueueOverwrite(network_queue, &text);
}
