#include "ai_lvgl.h"
#include "lvgl.h"
/* Design preview only: connection is simulated locally. */
LV_FONT_DECLARE(design_chinese_font);
static lv_obj_t *status_label, *status_dot, *button, *button_label;
static bool connected;
static lv_timer_t *pending;
static int volume = 50, brightness = 70;
/* Keep corner controls inside a 24–28 px inset for rounded board displays. */
enum { SAFE_EDGE = 28 };

static void style_button(lv_obj_t *obj) {
    lv_obj_set_style_radius(obj, 12, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x91ABBA), 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_20, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(0xF4F7F8), 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_transform_width(obj, 0, LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(obj, 0, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(obj, LV_OPA_80, LV_STATE_PRESSED);
}

static void close_panel(lv_event_t *event) {
    lv_obj_delete((lv_obj_t *)lv_event_get_user_data(event));
}

static lv_obj_t *text_button(lv_obj_t *parent, int x, int y, int w, int h, const char *text) {
    lv_obj_t *obj = lv_button_create(parent);
    style_button(obj);
    lv_obj_set_pos(obj, x, y); lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, 10, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x344858), 0);
    lv_obj_t *label = lv_label_create(obj);
    lv_label_set_text(label, text); lv_obj_center(label);
    return obj;
}

static lv_obj_t *overlay(lv_obj_t *parent) {
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, 320, 240);
    lv_obj_set_pos(panel, 0, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x18232D), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    return panel;
}

static void wifi_prompt(lv_event_t *event) {
    lv_obj_t *panel = overlay((lv_obj_t *)lv_event_get_user_data(event));
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_set_width(label, 240);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(label, "请用手机连接本设备配置网络");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 72);
    lv_obj_t *back = text_button(panel, 100, 152, 120, 40, "返回");
    lv_obj_add_event_cb(back, close_panel, LV_EVENT_CLICKED, panel);
}

static void slider_changed(lv_event_t *event) {
    lv_obj_t *slider = lv_event_get_target_obj(event);
    bool is_volume = lv_obj_get_y(slider) < 160;
    int value = lv_slider_get_value(slider);
    if (is_volume) volume = value; else brightness = value;
    lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(event);
    lv_label_set_text_fmt(label, "%s %d%%", is_volume ? "音量" : "亮度", value);
}

static void add_slider(lv_obj_t *panel, int y, const char *title, int value) {
    lv_obj_t *label = lv_label_create(panel);
    lv_label_set_text_fmt(label, "%s %d%%", title, value);
    lv_obj_set_pos(label, 48, y);
    lv_obj_set_style_text_color(label, lv_color_hex(0xBECDD7), 0);
    lv_obj_t *slider = lv_slider_create(panel);
    lv_obj_set_pos(slider, 56, y + 32);
    lv_obj_set_size(slider, 208, 8);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x304552), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(slider, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 5, LV_PART_KNOB);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, value, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x82BDAA), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xB9DAD0), LV_PART_KNOB);
    lv_obj_add_event_cb(slider, slider_changed, LV_EVENT_VALUE_CHANGED, label);
}

static void open_settings(lv_event_t *event) {
    (void)event;
    lv_obj_t *panel = overlay(lv_screen_active());
    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "设置");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, SAFE_EDGE, 24);
    lv_obj_t *back = text_button(panel, 234, 20, 58, 30, "返回");
    lv_obj_add_event_cb(back, close_panel, LV_EVENT_CLICKED, panel);
    lv_obj_t *wifi = text_button(panel, 48, 64, 224, 36, "WIFI配置");
    lv_obj_add_event_cb(wifi, wifi_prompt, LV_EVENT_CLICKED, panel);
    add_slider(panel, 112, "音量", volume);
    add_slider(panel, 168, "亮度", brightness);
}
static void refresh(void) {
    lv_label_set_text(status_label, connected ? "已连接" : "未连接");
    lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, -SAFE_EDGE, 24);
    lv_label_set_text(button_label, connected ? "断开连接" : "连接");
    lv_obj_set_style_bg_color(button, lv_color_hex(connected ? 0xB96666 : 0x438D7B), 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(connected ? 0xA5CDBE : 0x9CAEBB), 0);
    lv_obj_center(button_label);
    lv_obj_set_style_bg_color(status_dot, lv_color_hex(connected ? 0x82BDAA : 0x8496A5), 0);
    lv_obj_remove_state(button, LV_STATE_DISABLED);
}
static void complete(lv_timer_t *timer) {
    (void)timer; pending = NULL; connected = true; refresh();
}
static void toggle(lv_event_t *event) {
    (void)event;
    if (connected) { connected = false; refresh(); }
    else {
        lv_label_set_text(status_label, "连接中");
        lv_label_set_text(button_label, "连接中");
        lv_obj_center(button_label);
        lv_obj_add_state(button, LV_STATE_DISABLED);
        pending = lv_timer_create(complete, 600, NULL);
        lv_timer_set_repeat_count(pending, 1);
    }
}
static lv_obj_t *shape(lv_obj_t *parent, int x, int y, int w, int h, int radius, unsigned color) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y); lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}
void ai_lvgl_create(const char *board_name, void (*command)(ai_ui_command_t)) {
    (void)board_name; (void)command;
    if (pending) { lv_timer_delete(pending); pending = NULL; }
    connected = false;
    lv_obj_t *screen = lv_screen_active();
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x18232D), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xE6EDF2), 0);
    lv_obj_set_style_text_font(screen, &design_chinese_font, 0);
    /* Static diagnostic examples, independent of live device data. */
    lv_obj_t *error_label = lv_label_create(screen);
    lv_label_set_text(error_label, "error:1001");
    lv_obj_set_style_text_font(error_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(error_label, lv_color_hex(0xD49B91), 0);
    lv_obj_set_pos(error_label, SAFE_EDGE, 24);
    lv_obj_t *memory_label = lv_label_create(screen);
    lv_label_set_text(memory_label, "MEM:20/320KB");
    lv_obj_set_style_text_font(memory_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(memory_label, lv_color_hex(0x9CAEBB), 0);
    lv_obj_set_pos(memory_label, SAFE_EDGE, 42);
    status_label = lv_label_create(screen);
    status_dot = shape(screen, 229, 30, 7, 7, LV_RADIUS_CIRCLE, 0x8496A5);
    lv_obj_t *face = shape(screen, 85, 68, 150, 102, 22, 0x223440);
    lv_obj_set_style_border_width(face, 1, 0);
    lv_obj_set_style_shadow_color(face, lv_color_hex(0x080F16), 0);
    lv_obj_set_style_shadow_width(face, 20, 0);
    lv_obj_set_style_shadow_opa(face, LV_OPA_20, 0);
    lv_obj_set_style_shadow_ofs_y(face, 5, 0);
    lv_obj_set_style_border_color(face, lv_color_hex(0x354D5C), 0);
    shape(face, 43, 28, 12, 22, LV_RADIUS_CIRCLE, 0xB9DAD0);
    shape(face, 89, 28, 12, 22, LV_RADIUS_CIRCLE, 0xB9DAD0);
    lv_obj_t *mouth = shape(face, 54, 65, 38, 17, 0, 0x223440);
    lv_obj_set_style_radius(mouth, 12, 0);
    lv_obj_set_style_border_color(mouth, lv_color_hex(0xB9DAD0), 0);
    lv_obj_set_style_border_width(mouth, 3, 0);
    lv_obj_set_style_border_side(mouth, LV_BORDER_SIDE_BOTTOM, 0);
    button = lv_button_create(screen);
    style_button(button);
    lv_obj_set_pos(button, 80, 180); lv_obj_set_size(button, 160, 40);
    lv_obj_set_style_radius(button, 12, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x438D7B), 0);
    lv_obj_add_event_cb(button, toggle, LV_EVENT_CLICKED, NULL);
    button_label = lv_label_create(button);
    lv_obj_t *settings = text_button(screen, 254, 182, 36, 36, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_bg_color(settings, lv_color_hex(0x253744), 0);
    lv_obj_t *gear = lv_obj_get_child(settings, 0);
    lv_obj_set_style_text_font(gear, &lv_font_montserrat_14, 0);
    lv_obj_add_event_cb(settings, open_settings, LV_EVENT_CLICKED, NULL);
    refresh();
}
/* Ignore the formal preview's sample business states. */
void ai_lvgl_render(const ai_view_t *view) { (void)view; }
void ai_lvgl_network(const char *text) { (void)text; }


