#include "ws_ui.h"
#include "ws_app_events.h"
#include "ai_lvgl.h"
#include "ai_screen.h"
#include "lvgl.h"
#include "goldie_osal.h"
#include "app_manager.h"
#include "ws_board.h"
#include <stdio.h>
#include <string.h>
extern unsigned int sys_now(void);
static goldie_mutex lock;
static goldie_sem ready;
static int init_result;
static ai_view_t latest;
#ifdef AI_UI_NO_PROVISION
static char network[128] = "Checking Windows network...";
#else
static char network[128] = "Starting Wi-Fi...";
#endif
static bool dirty = true, visible;
static int touch_x, touch_y, touch_pressure;
static uint16_t pixels[AI_SCREEN_WIDTH * 20];
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data) {
    ws_board_display_write(area->x1,area->y1,area->x2-area->x1+1,
                              area->y2-area->y1+1,(uint16_t *)data);
    lv_display_flush_ready(display);
}
static void read_touch(lv_indev_t *input, lv_indev_data_t *data) {
    (void)input;
    goldie_mutex_lock(&lock);
    data->point.x = touch_x; data->point.y = touch_y;
    data->state = touch_pressure && visible ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    goldie_mutex_unlock(&lock);
}
static void command(ai_ui_command_t action) { ws_command(action == AI_UI_PROVISION ? AI_COMMAND_PROVISION : AI_COMMAND_TOGGLE); }
static int ui_task(void *ctx) {
    (void)ctx;
    lv_init(); lv_tick_set_cb(sys_now);
    lv_display_t *display = lv_display_create(AI_SCREEN_WIDTH, AI_SCREEN_HEIGHT);
    if (!display) { init_result = -1; goldie_sem_post(&ready); return -1; }
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, pixels, NULL, sizeof(pixels), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    lv_indev_t *input = lv_indev_create();
    if (!input) { lv_display_delete(display); init_result = -1; goldie_sem_post(&ready); return -1; }
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER); lv_indev_set_read_cb(input, read_touch);
    ai_lvgl_create(ws_board_name(), command);
    goldie_sem_post(&ready);
    for (;;) {
        ai_view_t view; char text[128]; bool update, shown;
        goldie_mutex_lock(&lock);
        view = latest; memcpy(text, network, sizeof(text));
        update = dirty; dirty = false; shown = visible;
        goldie_mutex_unlock(&lock);
        if (update) { ai_lvgl_render(&view); ai_lvgl_network(text); }
        if (shown) lv_timer_handler();
        goldie_msleep(20);
    }
    return 0;
}
static void show(void) { goldie_mutex_lock(&lock); visible = true; dirty = true; goldie_mutex_unlock(&lock); }
static void hide(void) { ws_command(AI_COMMAND_STOP); goldie_mutex_lock(&lock); visible = false; goldie_mutex_unlock(&lock); }
static void touch(int pressure, int x, int y) {
    goldie_mutex_lock(&lock);
    touch_pressure = pressure; touch_x = x; touch_y = y;
    goldie_mutex_unlock(&lock);
}
static void key(int pressure, int value) {
    if (pressure && value == 300) ws_command(AI_COMMAND_STOP);
    if (pressure && value == 301) ws_command(AI_COMMAND_TOGGLE);
}
static App_t app = {"Unified AI", show, hide, hide, show, touch, key, NULL, 0};
int ws_ui_init(void) {
    goldie_mutex_init(&lock);
    if (ws_board_display_init()) return -1;
    goldie_sem_init(&ready);
    if (!goldie_thread_create(ui_task, NULL, "ai_ui", 8192)) { goldie_sem_destroy(&ready); return -1; }
    goldie_sem_wait(&ready); goldie_sem_destroy(&ready);
    if (init_result) return init_result;
    if (install_app(&app) != 0) return -1;
    return goldie_run_app(&app);
}
void ws_ui_render(void *ctx, const ai_view_t *view) {
    (void)ctx; goldie_mutex_lock(&lock); latest = *view; dirty = true; goldie_mutex_unlock(&lock);
}
void ws_ui_network(const char *text) {
    goldie_mutex_lock(&lock);
    if (strcmp(network, text)) { snprintf(network, sizeof(network), "%s", text); dirty = true; }
    goldie_mutex_unlock(&lock);
}
