/**
 * @file widgets/top_bar.c
 * @brief Status bar at the top of the chat screen (y 0..40).
 *
 * Shows a colored cloud-connection dot, a "Connected"/"Disconnected" label
 * and a static "WiFi" caption. The cloud state is updated by
 * ai_chat_ui_set_cloud() (driven by SDK on_convai_event).
 */

#include "ai_chat_ui_internal.h"

void create_top_bar(void) {
  ui.status_dot = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.status_dot, 8, 8);
  lv_obj_set_pos(ui.status_dot, 16, 16);
  lv_obj_set_style_bg_color(ui.status_dot, C_TEXT_GRAY, 0);
  lv_obj_set_style_bg_opa(ui.status_dot, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(ui.status_dot, 4, 0);
  lv_obj_set_style_border_width(ui.status_dot, 0, 0);

  ui.status_label = lv_label_create(lv_screen_active());
  lv_label_set_text(ui.status_label, "\xE2\x97\x8F Offline");
  lv_obj_set_pos(ui.status_label, 30, 11);
  lv_obj_set_style_text_color(ui.status_label, C_TEXT, 0);
  lv_obj_set_style_text_font(ui.status_label, &lv_font_montserrat_14, 0);

  /* Plain text on purpose: the CJK font carries no FontAwesome glyphs. */
  lv_obj_t *wifi_icon = lv_label_create(lv_screen_active());
  lv_label_set_text(wifi_icon, "WiFi");
  lv_obj_set_style_text_color(wifi_icon, C_TEXT, 0);
  lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_14, 0);
  lv_obj_align(wifi_icon, LV_ALIGN_TOP_RIGHT, -8, 10);
}
