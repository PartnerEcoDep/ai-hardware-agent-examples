/**
 * @file widgets/float_ball.c
 * @brief Floating action button (bottom-right) opening the voice selector.
 *
 * The ball is the primary entry point to the voice picker; a long press
 * anywhere on the screen background is kept as a backup gesture for boards
 * where the touch panel misses small targets.
 */

#include "ai_chat_ui_internal.h"

#define BALL_SIZE    44
#define BALL_MARGIN  10
#define BALL_LINES   3

/** Switch to the voice picker (no-op if it is already open). */
static void open_voice_selector(void) {
  if (ai_chat_ui_get_state() == CHAT_VOICE_SELECT) {
    return;
  }
  ai_chat_ui_show_voice_selector(true);
  ai_chat_ui_set_state(CHAT_VOICE_SELECT);
}

void on_screen_long_press(lv_event_t *e) {
  (void)e;
  open_voice_selector();
}

/** Ball tap: open the picker, or cancel it when it is already showing. */
static void on_float_ball_click(lv_event_t *e) {
  (void)e;
  if (ai_chat_ui_get_state() == CHAT_VOICE_SELECT) {
    voice_sel_close();
  } else {
    open_voice_selector();
  }
}

void create_float_ball(void) {
  const int x = 320 - BALL_SIZE - BALL_MARGIN; /* 266 */
  const int y = 240 - BALL_SIZE - BALL_MARGIN; /* 186 */

  ui.float_ball = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.float_ball, BALL_SIZE, BALL_SIZE);
  lv_obj_set_pos(ui.float_ball, x, y);
  lv_obj_set_style_bg_color(ui.float_ball, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(ui.float_ball, LV_OPA_30, 0);
  lv_obj_set_style_border_width(ui.float_ball, 1, 0);
  lv_obj_set_style_border_color(ui.float_ball, lv_color_hex(0x9CA3AF), 0);
  lv_obj_set_style_radius(ui.float_ball, BALL_SIZE / 2, 0);
  lv_obj_set_style_pad_all(ui.float_ball, 0, 0);
  lv_obj_add_flag(ui.float_ball, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ui.float_ball, on_float_ball_click, LV_EVENT_CLICKED,
                      NULL);

  /* Hamburger icon: three short bars centered in the ball. */
  for (int i = 0; i < BALL_LINES; i++) {
    lv_obj_t *line = lv_obj_create(ui.float_ball);
    ui.float_ball_lines[i] = line;
    lv_obj_set_size(line, 22, 3);
    lv_obj_set_pos(line, 11, 14 + i * 9);
    lv_obj_set_style_bg_color(line, C_TEXT, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, 1, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
  }
}
