/**
 * @file widgets/top_bar.c
 * @brief Status bar at the top of the chat screen (y 0..40).
 *
 * Shows a colored cloud-connection dot, a "Connected"/"Disconnected" label,
 * a compact hardware volume control, and a static "WiFi" caption. The cloud
 * state is updated by ai_chat_ui_set_cloud() (driven by SDK on_convai_event).
 */

#include "ai_chat_ui_internal.h"
#include "audio_init.h"

#define VOL_BTN_W   24
#define VOL_BTN_H   24
#define VOL_STEP    5

static void volume_minus_click(lv_event_t *e) {
  (void)e;
  ai_chat_ui_adjust_hw_volume(-VOL_STEP);
}

static void volume_plus_click(lv_event_t *e) {
  (void)e;
  ai_chat_ui_adjust_hw_volume(VOL_STEP);
}

static lv_obj_t *create_volume_button(int x, const char *text,
                                      lv_event_cb_t cb) {
  lv_obj_t *btn = lv_obj_create(lv_screen_active());
  lv_obj_set_size(btn, VOL_BTN_W, VOL_BTN_H);
  lv_obj_set_pos(btn, x, 8);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x374151), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(btn, 6, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, C_TEXT, 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_center(label);
  return btn;
}

void create_volume_control(void) {
  /* Keep the control between the connection status and the WiFi caption. */
  ui.volume_ctrl.btn_minus = create_volume_button(138, "-", volume_minus_click);
  ui.volume_ctrl.label = lv_label_create(lv_screen_active());
  lv_label_set_text_fmt(ui.volume_ctrl.label, "%u%%",
                        (unsigned)audio_get_volume());
  lv_obj_set_size(ui.volume_ctrl.label, 34, 20);
  lv_obj_set_pos(ui.volume_ctrl.label, 164, 10);
  lv_obj_set_style_text_color(ui.volume_ctrl.label, C_TEXT, 0);
  lv_obj_set_style_text_font(ui.volume_ctrl.label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(ui.volume_ctrl.label, LV_TEXT_ALIGN_CENTER, 0);
  ui.volume_ctrl.btn_plus = create_volume_button(200, "+", volume_plus_click);
}

void create_top_bar(void) {
  ui.status_dot = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.status_dot, 8, 8);
  lv_obj_set_pos(ui.status_dot, 16, 16);
  lv_obj_set_style_bg_color(ui.status_dot, C_TEXT_GRAY, 0);
  lv_obj_set_style_bg_opa(ui.status_dot, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(ui.status_dot, 4, 0);
  lv_obj_set_style_border_width(ui.status_dot, 0, 0);

  ui.status_label = lv_label_create(lv_screen_active());
  lv_label_set_text(ui.status_label, "Disconnected");
  lv_obj_set_pos(ui.status_label, 30, 11);
  lv_obj_set_style_text_color(ui.status_label, C_TEXT, 0);
  lv_obj_set_style_text_font(ui.status_label, &lv_font_montserrat_14, 0);

  create_volume_control();

  /* Top-right: uplink packet loss rate. Bottom-left: RAM usage. */
  ui.loss_label = lv_label_create(lv_screen_active());
  lv_label_set_text(ui.loss_label, "Loss -");
  lv_obj_set_style_text_color(ui.loss_label, C_TEXT_GRAY, 0);
  lv_obj_set_style_text_font(ui.loss_label, &lv_font_montserrat_14, 0);
  lv_obj_align(ui.loss_label, LV_ALIGN_TOP_RIGHT, -8, 6);
  ui.ram_label = lv_label_create(lv_screen_active());
  lv_label_set_text(ui.ram_label, "Use -");
  lv_obj_set_style_text_color(ui.ram_label, C_TEXT_GRAY, 0);
  lv_obj_set_style_text_font(ui.ram_label, &lv_font_montserrat_14, 0);
  lv_obj_align(ui.ram_label, LV_ALIGN_BOTTOM_LEFT, 8, -8);
}
