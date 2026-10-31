#include "ai_lvgl.h"
#include "ai_screen.h"
#include "lvgl.h"
static void (*on_command)(ai_ui_command_t);
static lv_obj_t *state_label, *network_label, *toggle_label, *toggle_button;
static void toggle(lv_event_t *event) { (void)event; on_command(AI_UI_TOGGLE); }
#ifndef AI_UI_NO_PROVISION
static void provision(lv_event_t *event) { (void)event; on_command(AI_UI_PROVISION); }
#endif

static lv_obj_t *label(lv_obj_t *parent, int x, int y, int width, const char *text) {
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_pos(obj, x, y); lv_obj_set_width(obj, width);
    lv_label_set_text(obj, text);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);
    return obj;
}
void ai_lvgl_create(const char *board_name, void (*command)(ai_ui_command_t)) {
    on_command = command;
    lv_obj_t *screen = lv_screen_active();
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(AI_SCREEN_BACKGROUND), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(AI_SCREEN_FOREGROUND), 0);
    label(screen, AI_SCREEN_LEFT, AI_SCREEN_TITLE_Y, AI_SCREEN_TEXT_WIDTH, board_name);
    state_label = label(screen, AI_SCREEN_LEFT, AI_SCREEN_STATE_Y, AI_SCREEN_TEXT_WIDTH, "Starting...");
#ifdef AI_UI_NO_PROVISION
    network_label = label(screen, AI_SCREEN_LEFT, AI_SCREEN_NETWORK_Y, AI_SCREEN_TEXT_WIDTH, "Checking Windows network...");
#else
    network_label = label(screen, AI_SCREEN_LEFT, AI_SCREEN_NETWORK_Y, AI_SCREEN_TEXT_WIDTH, "Starting Wi-Fi...");
#endif
    toggle_button = lv_button_create(screen);
    lv_obj_set_pos(toggle_button, AI_SCREEN_LEFT, AI_SCREEN_BUTTON_Y);
    lv_obj_set_size(toggle_button, AI_SCREEN_TOGGLE_WIDTH, AI_SCREEN_BUTTON_HEIGHT);
    lv_obj_add_event_cb(toggle_button, toggle, LV_EVENT_CLICKED, NULL);
    toggle_label = lv_label_create(toggle_button);
    lv_label_set_text(toggle_label, AI_SCREEN_START); lv_obj_center(toggle_label);
    lv_obj_add_state(toggle_button, LV_STATE_DISABLED);
#ifndef AI_UI_NO_PROVISION
    lv_obj_t *wifi = lv_button_create(screen);
    lv_obj_set_pos(wifi, AI_SCREEN_WIFI_X, AI_SCREEN_BUTTON_Y);
    lv_obj_set_size(wifi, AI_SCREEN_WIFI_WIDTH, AI_SCREEN_BUTTON_HEIGHT);
    lv_obj_add_event_cb(wifi, provision, LV_EVENT_CLICKED, NULL);
    lv_obj_t *text = lv_label_create(wifi);
    lv_label_set_text(text, AI_SCREEN_WIFI); lv_obj_center(text);
#endif
}
void ai_lvgl_render(const ai_view_t *view) {
    char status[80];
    ai_screen_status(view, status, sizeof(status));
    lv_label_set_text(state_label, status);
    lv_label_set_text(toggle_label, view->active ? AI_SCREEN_STOP : AI_SCREEN_START);
    if (view->network_up) lv_obj_remove_state(toggle_button, LV_STATE_DISABLED);
    else lv_obj_add_state(toggle_button, LV_STATE_DISABLED);
}
void ai_lvgl_network(const char *text) { lv_label_set_text(network_label, text); }
