/**
 * @file ai_chat_ui.c
 * @brief AI Chat UI - Voice Assistant style (LVGL 9.x, Google C style)
 *
 * Layout (320x240):
 *   [0..40]    status bar: dot + text / WiFi icon
 *   [50..170]  center voice orb (5 states, 80~120 px)
 *   [180..195] state label (16px, centered)
 *   [200..220] hint label  (16px, centered)
 */

#include "ai_chat_ui.h"
#include "voice_config.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "ai_chat_ui";

#define FONT_CJK_16  (&lv_font_source_han_sans_sc_16_cjk)
#define FONT_CJK_14  (&lv_font_source_han_sans_sc_14_cjk)

/* color palette (dark theme) */
#define C_BG          lv_color_hex(0x000000)
#define C_TEXT        lv_color_hex(0xFFFFFF)
#define C_TEXT_GRAY   lv_color_hex(0x9CA3AF)
#define C_GREEN       lv_color_hex(0x00E676)
#define C_GREEN_DIM   lv_color_hex(0x007A3A)
#define C_BLUE        lv_color_hex(0x3B82F6)
#define C_BLUE_DIM    lv_color_hex(0x1E3A8A)
#define C_PURPLE      lv_color_hex(0xA855F7)
#define C_PURPLE_DIM  lv_color_hex(0x581C87)
#define C_RED         lv_color_hex(0xFF5252)

#define ORB_CX  160
#define ORB_CY  110

typedef struct {
  lv_obj_t *ring_outer;
  lv_obj_t *ring_mid;
  lv_obj_t *core;
} idle_viz_t;

typedef struct {
  lv_obj_t *circle;
  lv_obj_t *icon_mic;
  lv_obj_t *icon_stand;
  lv_obj_t *icon_base;
  lv_obj_t *bars_l[5];
  lv_obj_t *bars_r[5];
  lv_anim_t anims_l[5];
  lv_anim_t anims_r[5];
} listening_viz_t;

typedef struct {
  lv_obj_t *circle;
  lv_obj_t *icon_speaker;
  lv_obj_t *wave_arcs[3];
} speaking_viz_t;

typedef struct {
  lv_obj_t *circle;
  lv_obj_t *spin_arc;
  lv_obj_t *static_arc;
} thinking_viz_t;

typedef struct {
  lv_obj_t *circle;
  lv_obj_t *bar1;
  lv_obj_t *bar2;
} disconnected_viz_t;

typedef struct {
  lv_obj_t *panel;
  lv_obj_t *title_label;
  lv_obj_t *left_bg;
  lv_obj_t *right_bg;
  lv_obj_t *gender_labels[VOICE_GENDER_COUNT];
  lv_obj_t *timbre_labels[6];  /* max 6 per gender */
  lv_obj_t *btn_cancel;
  lv_obj_t *btn_confirm;
  lv_obj_t *cancel_label;
  lv_obj_t *confirm_label;
  int       timbre_count;
  int       gender_idx;
  int       timbre_idx;
} voice_select_viz_t;

static struct {
  lv_obj_t *status_dot;
  lv_obj_t *status_label;

  idle_viz_t         idle;
  listening_viz_t    listening;
  speaking_viz_t     speaking;
  thinking_viz_t     thinking;
  disconnected_viz_t disconnected;
  voice_select_viz_t voice_sel;

  lv_obj_t *state_label;
  lv_obj_t *hint_label;
  lv_obj_t *float_ball;
  lv_obj_t *float_ball_label;
} ui;

static chat_state_t s_state = CHAT_IDLE;
static uint8_t s_volume = 0;

/* ===================================================================
 *  animation callbacks
 * =================================================================== */

static void anim_pulse_size_cb(void *var, int32_t v) {
  lv_obj_set_size((lv_obj_t *)var, v, v);
}

/* animate size AND re-center (for orb-anchored objects) */
static void anim_pulse_centered_cb(void *var, int32_t v) {
  lv_obj_t *obj = (lv_obj_t *)var;
  lv_obj_set_size(obj, v, v);
  lv_obj_set_pos(obj, ORB_CX - v / 2, ORB_CY - v / 2);
  lv_obj_set_style_radius(obj, v / 2, 0);
}

static void anim_pulse_opa_cb(void *var, int32_t v) {
  lv_obj_set_style_opa((lv_obj_t *)var, v, 0);
}

static void anim_bar_h_cb(void *var, int32_t v) {
  lv_obj_set_height((lv_obj_t *)var, v);
}

static void anim_arc_rotate_cb(void *var, int32_t v) {
  lv_arc_set_rotation((lv_obj_t *)var, v);
}

static inline void pos_centered(lv_obj_t *obj, lv_coord_t cx,
                                lv_coord_t cy) {
  lv_obj_set_pos(obj, cx - lv_obj_get_width(obj) / 2,
                 cy - lv_obj_get_height(obj) / 2);
}

static void show_obj(lv_obj_t *o) {
  if (o) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static void hide_obj(lv_obj_t *o) {
  if (o) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static void anim_init_bar(lv_anim_t *a, lv_obj_t *bar, int min_h,
                          int max_h, uint32_t dur) {
  lv_anim_init(a);
  lv_anim_set_var(a, bar);
  lv_anim_set_exec_cb(a, anim_bar_h_cb);
  lv_anim_set_values(a, min_h, max_h);
  lv_anim_set_duration(a, dur);
  lv_anim_set_playback_duration(a, dur);
  lv_anim_set_repeat_count(a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(a, lv_anim_path_ease_in_out);
}

/* ===================================================================
 *  top bar  (y 0..40)
 * =================================================================== */

static void create_top_bar(void) {
  ui.status_dot = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.status_dot, 8, 8);
  lv_obj_set_pos(ui.status_dot, 16, 16);
  lv_obj_set_style_bg_color(ui.status_dot, C_GREEN, 0);
  lv_obj_set_style_bg_opa(ui.status_dot, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(ui.status_dot, 4, 0);
  lv_obj_set_style_border_width(ui.status_dot, 0, 0);

  ui.status_label = lv_label_create(lv_screen_active());
  lv_label_set_text(ui.status_label, "Online");
  lv_obj_set_pos(ui.status_label, 30, 11);
  lv_obj_set_style_text_color(ui.status_label, C_TEXT, 0);
  lv_obj_set_style_text_font(ui.status_label, &lv_font_montserrat_14, 0);

  /* WiFi — plain text, CJK font has no FontAwesome symbols */
  lv_obj_t *wifi_icon = lv_label_create(lv_screen_active());
  lv_label_set_text(wifi_icon, "WiFi");
  lv_obj_set_pos(wifi_icon, 290, 10);
  lv_obj_set_style_text_color(wifi_icon, C_TEXT, 0);
  lv_obj_set_style_text_font(wifi_icon, FONT_CJK_14, 0);
}

/* ===================================================================
 *  IDLE  -  green center dot + breathing outer ring
 * =================================================================== */

static void create_idle_viz(void) {
  /* outer ring — border circle (lv_obj, not lv_arc, for reliable ESP32 rendering) */
  ui.idle.ring_outer = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(ui.idle.ring_outer);
  lv_obj_set_size(ui.idle.ring_outer, 110, 110);
  lv_obj_set_pos(ui.idle.ring_outer, ORB_CX - 55, ORB_CY - 55);
  lv_obj_set_style_radius(ui.idle.ring_outer, 55, 0);
  lv_obj_set_style_bg_opa(ui.idle.ring_outer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ui.idle.ring_outer, 2, 0);
  lv_obj_set_style_border_color(ui.idle.ring_outer, C_GREEN_DIM, 0);
  lv_obj_set_style_border_opa(ui.idle.ring_outer, LV_OPA_40, 0);

  /* mid ring — border circle */
  ui.idle.ring_mid = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(ui.idle.ring_mid);
  lv_obj_set_size(ui.idle.ring_mid, 86, 86);
  lv_obj_set_pos(ui.idle.ring_mid, ORB_CX - 43, ORB_CY - 43);
  lv_obj_set_style_radius(ui.idle.ring_mid, 43, 0);
  lv_obj_set_style_bg_opa(ui.idle.ring_mid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ui.idle.ring_mid, 2, 0);
  lv_obj_set_style_border_color(ui.idle.ring_mid, C_GREEN, 0);
  lv_obj_set_style_border_opa(ui.idle.ring_mid, LV_OPA_70, 0);

  /* core — filled circle + shadow */
  ui.idle.core = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.idle.core, 22, 22);
  pos_centered(ui.idle.core, ORB_CX, ORB_CY);
  lv_obj_set_style_bg_color(ui.idle.core, C_GREEN, 0);
  lv_obj_set_style_bg_opa(ui.idle.core, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(ui.idle.core, 11, 0);
  lv_obj_set_style_border_width(ui.idle.core, 0, 0);
  lv_obj_set_style_shadow_color(ui.idle.core, C_GREEN, 0);
  lv_obj_set_style_shadow_width(ui.idle.core, 16, 0);
  lv_obj_set_style_shadow_opa(ui.idle.core, LV_OPA_60, 0);

  /* outer ring breathing (size + opacity, centered) */
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, ui.idle.ring_outer);
  lv_anim_set_exec_cb(&a, anim_pulse_centered_cb);
  lv_anim_set_values(&a, 110, 128);
  lv_anim_set_duration(&a, 2000);
  lv_anim_set_playback_duration(&a, 2000);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_start(&a);

  lv_anim_init(&a);
  lv_anim_set_var(&a, ui.idle.ring_outer);
  lv_anim_set_exec_cb(&a, anim_pulse_opa_cb);
  lv_anim_set_values(&a, LV_OPA_70, LV_OPA_10);
  lv_anim_set_duration(&a, 2000);
  lv_anim_set_playback_duration(&a, 2000);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_start(&a);

  /* core breathing */
  lv_anim_init(&a);
  lv_anim_set_var(&a, ui.idle.core);
  lv_anim_set_exec_cb(&a, anim_pulse_centered_cb);
  lv_anim_set_values(&a, 22, 26);
  lv_anim_set_duration(&a, 1500);
  lv_anim_set_playback_duration(&a, 1500);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_start(&a);
}

/* ===================================================================
 *  LISTENING  -  blue + mic + 5+5 waveform bars
 * =================================================================== */

static void create_listening_viz(void) {
  ui.listening.circle = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.listening.circle, 80, 80);
  pos_centered(ui.listening.circle, ORB_CX, ORB_CY);
  lv_obj_set_style_bg_color(ui.listening.circle, C_BLUE, 0);
  lv_obj_set_style_bg_opa(ui.listening.circle, LV_OPA_20, 0);
  lv_obj_set_style_border_width(ui.listening.circle, 3, 0);
  lv_obj_set_style_border_color(ui.listening.circle, C_BLUE, 0);
  lv_obj_set_style_border_opa(ui.listening.circle, LV_OPA_60, 0);
  lv_obj_set_style_radius(ui.listening.circle, 40, 0);
  lv_obj_set_style_shadow_color(ui.listening.circle, C_BLUE, 0);
  lv_obj_set_style_shadow_width(ui.listening.circle, 20, 0);
  lv_obj_set_style_shadow_opa(ui.listening.circle, LV_OPA_40, 0);

  /* mic body */
  ui.listening.icon_mic = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.listening.icon_mic, 14, 22);
  lv_obj_set_pos(ui.listening.icon_mic, ORB_CX - 7, ORB_CY - 24);
  lv_obj_set_style_bg_color(ui.listening.icon_mic, C_BLUE, 0);
  lv_obj_set_style_bg_opa(ui.listening.icon_mic, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(ui.listening.icon_mic, 7, 0);
  lv_obj_set_style_border_width(ui.listening.icon_mic, 0, 0);

  /* mic stand (half arc via lv_obj border, avoid lv_arc ESP32 bug) */
  ui.listening.icon_stand = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.listening.icon_stand, 24, 12);
  lv_obj_set_pos(ui.listening.icon_stand, ORB_CX - 12, ORB_CY + 2);
  lv_obj_set_style_bg_opa(ui.listening.icon_stand, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(ui.listening.icon_stand, C_BLUE, 0);
  lv_obj_set_style_border_width(ui.listening.icon_stand, 2, 0);
  lv_obj_set_style_border_opa(ui.listening.icon_stand, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(ui.listening.icon_stand, 24, 0);
  lv_obj_set_style_bg_opa(ui.listening.icon_stand, LV_OPA_TRANSP, LV_PART_MAIN);
  /* draw only top border: zero out left/right/bottom */
  lv_obj_set_style_border_side(ui.listening.icon_stand,
                                LV_BORDER_SIDE_TOP, 0);

  /* mic base */
  ui.listening.icon_base = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.listening.icon_base, 18, 2);
  lv_obj_set_pos(ui.listening.icon_base, ORB_CX - 9, ORB_CY + 12);
  lv_obj_set_style_bg_color(ui.listening.icon_base, C_BLUE, 0);
  lv_obj_set_style_bg_opa(ui.listening.icon_base, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(ui.listening.icon_base, 1, 0);
  lv_obj_set_style_border_width(ui.listening.icon_base, 0, 0);

  /* 5+5 waveform bars (default heights) */
  static const int xs_l[5] = {
    ORB_CX - 75, ORB_CX - 67, ORB_CX - 59, ORB_CX - 51, ORB_CX - 43
  };
  static const int xs_r[5] = {
    ORB_CX + 40, ORB_CX + 48, ORB_CX + 56, ORB_CX + 64, ORB_CX + 72
  };
  static const int default_h[5] = { 8, 16, 26, 18, 10 };
  for (int i = 0; i < 5; i++) {
    ui.listening.bars_l[i] = lv_obj_create(lv_screen_active());
    lv_obj_set_size(ui.listening.bars_l[i], 3, default_h[i]);
    lv_obj_set_pos(ui.listening.bars_l[i], xs_l[i],
                   ORB_CY - default_h[i] / 2);
    lv_obj_set_style_bg_color(ui.listening.bars_l[i], C_BLUE, 0);
    lv_obj_set_style_bg_opa(ui.listening.bars_l[i], LV_OPA_80, 0);
    lv_obj_set_style_radius(ui.listening.bars_l[i], 1, 0);
    lv_obj_set_style_border_width(ui.listening.bars_l[i], 0, 0);
    anim_init_bar(&ui.listening.anims_l[i], ui.listening.bars_l[i],
                  4, default_h[i], 500 + i * 80);
    lv_anim_start(&ui.listening.anims_l[i]);

    ui.listening.bars_r[i] = lv_obj_create(lv_screen_active());
    lv_obj_set_size(ui.listening.bars_r[i], 3, default_h[i]);
    lv_obj_set_pos(ui.listening.bars_r[i], xs_r[i],
                   ORB_CY - default_h[i] / 2);
    lv_obj_set_style_bg_color(ui.listening.bars_r[i], C_BLUE, 0);
    lv_obj_set_style_bg_opa(ui.listening.bars_r[i], LV_OPA_80, 0);
    lv_obj_set_style_radius(ui.listening.bars_r[i], 1, 0);
    lv_obj_set_style_border_width(ui.listening.bars_r[i], 0, 0);
    anim_init_bar(&ui.listening.anims_r[i], ui.listening.bars_r[i],
                  4, default_h[i], 500 + i * 80);
    lv_anim_start(&ui.listening.anims_r[i]);
  }

  /* circle breathing */
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, ui.listening.circle);
  lv_anim_set_exec_cb(&a, anim_pulse_centered_cb);
  lv_anim_set_values(&a, 80, 88);
  lv_anim_set_duration(&a, 1500);
  lv_anim_set_playback_duration(&a, 1500);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_start(&a);
}

/* ===================================================================
 *  SPEAKING  -  purple + speaker + 3 expanding rings
 * =================================================================== */

static void create_speaking_viz(void) {
  ui.speaking.circle = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.speaking.circle, 80, 80);
  pos_centered(ui.speaking.circle, ORB_CX, ORB_CY);
  lv_obj_set_style_bg_color(ui.speaking.circle, C_PURPLE, 0);
  lv_obj_set_style_bg_opa(ui.speaking.circle, LV_OPA_20, 0);
  lv_obj_set_style_border_width(ui.speaking.circle, 3, 0);
  lv_obj_set_style_border_color(ui.speaking.circle, C_PURPLE, 0);
  lv_obj_set_style_border_opa(ui.speaking.circle, LV_OPA_60, 0);
  lv_obj_set_style_radius(ui.speaking.circle, 40, 0);
  lv_obj_set_style_shadow_color(ui.speaking.circle, C_PURPLE, 0);
  lv_obj_set_style_shadow_width(ui.speaking.circle, 20, 0);
  lv_obj_set_style_shadow_opa(ui.speaking.circle, LV_OPA_40, 0);

  /* speaker body (24x16 rounded rect) */
  ui.speaking.icon_speaker = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.speaking.icon_speaker, 24, 16);
  lv_obj_set_pos(ui.speaking.icon_speaker, ORB_CX - 12, ORB_CY - 8);
  lv_obj_set_style_bg_color(ui.speaking.icon_speaker, C_PURPLE, 0);
  lv_obj_set_style_bg_opa(ui.speaking.icon_speaker, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(ui.speaking.icon_speaker, 3, 0);
  lv_obj_set_style_border_width(ui.speaking.icon_speaker, 0, 0);

  static const int wave_sizes[3] = { 96, 110, 124 };
  for (int i = 0; i < 3; i++) {
    int s = wave_sizes[i];
    ui.speaking.wave_arcs[i] = lv_arc_create(lv_screen_active());
    lv_obj_set_size(ui.speaking.wave_arcs[i], s, s);
    pos_centered(ui.speaking.wave_arcs[i], ORB_CX, ORB_CY);
    lv_arc_set_bg_angles(ui.speaking.wave_arcs[i], 0, 360);
    lv_obj_set_style_arc_width(ui.speaking.wave_arcs[i], 2, 0);
    lv_obj_set_style_arc_color(ui.speaking.wave_arcs[i], C_PURPLE, 0);
    lv_obj_remove_style(ui.speaking.wave_arcs[i], NULL, LV_PART_KNOB);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ui.speaking.wave_arcs[i]);
    lv_anim_set_exec_cb(&a, anim_pulse_centered_cb);
    lv_anim_set_values(&a, s, s + 16);
    lv_anim_set_duration(&a, 1200);
    lv_anim_set_playback_duration(&a, 1200);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_delay(&a, i * 400);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, ui.speaking.wave_arcs[i]);
    lv_anim_set_exec_cb(&a, anim_pulse_opa_cb);
    lv_anim_set_values(&a, LV_OPA_70, LV_OPA_10);
    lv_anim_set_duration(&a, 1200);
    lv_anim_set_playback_duration(&a, 1200);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_delay(&a, i * 400);
    lv_anim_start(&a);
  }
}

/* ===================================================================
 *  THINKING  -  purple + rotating arc
 * =================================================================== */

static void create_thinking_viz(void) {
  ui.thinking.circle = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.thinking.circle, 80, 80);
  pos_centered(ui.thinking.circle, ORB_CX, ORB_CY);
  lv_obj_set_style_bg_color(ui.thinking.circle, C_PURPLE, 0);
  lv_obj_set_style_bg_opa(ui.thinking.circle, LV_OPA_20, 0);
  lv_obj_set_style_border_width(ui.thinking.circle, 0, 0);
  lv_obj_set_style_radius(ui.thinking.circle, 40, 0);

  ui.thinking.static_arc = lv_arc_create(lv_screen_active());
  lv_obj_set_size(ui.thinking.static_arc, 96, 96);
  pos_centered(ui.thinking.static_arc, ORB_CX, ORB_CY);
  lv_arc_set_bg_angles(ui.thinking.static_arc, 0, 360);
  lv_arc_set_value(ui.thinking.static_arc, 0);
  lv_obj_set_style_arc_width(ui.thinking.static_arc, 4, 0);
  lv_obj_set_style_arc_color(ui.thinking.static_arc, C_PURPLE_DIM, 0);
  lv_obj_set_style_arc_opa(ui.thinking.static_arc, LV_OPA_50, 0);
  lv_obj_remove_style(ui.thinking.static_arc, NULL, LV_PART_KNOB);

  ui.thinking.spin_arc = lv_arc_create(lv_screen_active());
  lv_obj_set_size(ui.thinking.spin_arc, 96, 96);
  pos_centered(ui.thinking.spin_arc, ORB_CX, ORB_CY);
  lv_arc_set_bg_angles(ui.thinking.spin_arc, 0, 90);
  lv_arc_set_value(ui.thinking.spin_arc, 0);
  lv_obj_set_style_arc_width(ui.thinking.spin_arc, 4, 0);
  lv_obj_set_style_arc_color(ui.thinking.spin_arc, C_PURPLE, 0);
  lv_obj_remove_style(ui.thinking.spin_arc, NULL, LV_PART_KNOB);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, ui.thinking.spin_arc);
  lv_anim_set_exec_cb(&a, anim_arc_rotate_cb);
  lv_anim_set_values(&a, 0, 3600);
  lv_anim_set_duration(&a, 1500);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&a, lv_anim_path_linear);
  lv_anim_start(&a);
}

/* ===================================================================
 *  DISCONNECTED  -  red + crossed bars
 * =================================================================== */

static void create_disconnected_viz(void) {
  ui.disconnected.circle = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.disconnected.circle, 80, 80);
  pos_centered(ui.disconnected.circle, ORB_CX, ORB_CY);
  lv_obj_set_style_bg_color(ui.disconnected.circle, C_RED, 0);
  lv_obj_set_style_bg_opa(ui.disconnected.circle, LV_OPA_20, 0);
  lv_obj_set_style_border_width(ui.disconnected.circle, 3, 0);
  lv_obj_set_style_border_color(ui.disconnected.circle, C_RED, 0);
  lv_obj_set_style_border_opa(ui.disconnected.circle, LV_OPA_60, 0);
  lv_obj_set_style_radius(ui.disconnected.circle, 40, 0);
  lv_obj_set_style_shadow_color(ui.disconnected.circle, C_RED, 0);
  lv_obj_set_style_shadow_width(ui.disconnected.circle, 20, 0);
  lv_obj_set_style_shadow_opa(ui.disconnected.circle, LV_OPA_30, 0);

  ui.disconnected.bar1 = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.disconnected.bar1, 36, 4);
  pos_centered(ui.disconnected.bar1, ORB_CX, ORB_CY);
  lv_obj_set_style_bg_color(ui.disconnected.bar1, C_RED, 0);
  lv_obj_set_style_bg_opa(ui.disconnected.bar1, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(ui.disconnected.bar1, 2, 0);
  lv_obj_set_style_border_width(ui.disconnected.bar1, 0, 0);
  lv_obj_set_style_transform_pivot_x(ui.disconnected.bar1, 18, 0);
  lv_obj_set_style_transform_pivot_y(ui.disconnected.bar1, 2, 0);
  lv_obj_set_style_transform_rotation(ui.disconnected.bar1, 450, 0);

  ui.disconnected.bar2 = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.disconnected.bar2, 36, 4);
  pos_centered(ui.disconnected.bar2, ORB_CX, ORB_CY);
  lv_obj_set_style_bg_color(ui.disconnected.bar2, C_RED, 0);
  lv_obj_set_style_bg_opa(ui.disconnected.bar2, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(ui.disconnected.bar2, 2, 0);
  lv_obj_set_style_border_width(ui.disconnected.bar2, 0, 0);
  lv_obj_set_style_transform_pivot_x(ui.disconnected.bar2, 18, 0);
  lv_obj_set_style_transform_pivot_y(ui.disconnected.bar2, 2, 0);
  lv_obj_set_style_transform_rotation(ui.disconnected.bar2, 1350, 0);
}

/* ===================================================================
 *  bottom labels  (state y=180, hint y=204)
 * =================================================================== */

static void create_bottom_text(void) {
  ui.state_label = lv_label_create(lv_screen_active());
  lv_obj_set_size(ui.state_label, 320, 20);
  lv_obj_set_pos(ui.state_label, 0, 180);
  lv_label_set_text(ui.state_label, "Idle");
  lv_obj_set_style_text_color(ui.state_label, C_TEXT, 0);
  lv_obj_set_style_text_font(ui.state_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(ui.state_label, LV_TEXT_ALIGN_CENTER, 0);

  ui.hint_label = lv_label_create(lv_screen_active());
  lv_obj_set_size(ui.hint_label, 320, 20);
  lv_obj_set_pos(ui.hint_label, 0, 204);
  lv_label_set_text(ui.hint_label, "Tap to talk");
  lv_obj_set_style_text_color(ui.hint_label, C_TEXT_GRAY, 0);
  lv_obj_set_style_text_font(ui.hint_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(ui.hint_label, LV_TEXT_ALIGN_CENTER, 0);
}

/* ===================================================================
 *  visibility helpers
 * =================================================================== */

static void hide_all_viz(void) {
  hide_obj(ui.idle.ring_outer);
  hide_obj(ui.idle.ring_mid);
  hide_obj(ui.idle.core);
  hide_obj(ui.listening.circle);
  hide_obj(ui.listening.icon_mic);
  hide_obj(ui.listening.icon_stand);
  hide_obj(ui.listening.icon_base);
  for (int i = 0; i < 5; i++) {
    hide_obj(ui.listening.bars_l[i]);
    hide_obj(ui.listening.bars_r[i]);
  }
  hide_obj(ui.speaking.circle);
  hide_obj(ui.speaking.icon_speaker);
  for (int i = 0; i < 3; i++) hide_obj(ui.speaking.wave_arcs[i]);
  hide_obj(ui.thinking.circle);
  hide_obj(ui.thinking.spin_arc);
  hide_obj(ui.thinking.static_arc);
  hide_obj(ui.disconnected.circle);
  hide_obj(ui.disconnected.bar1);
  hide_obj(ui.disconnected.bar2);
  hide_obj(ui.voice_sel.panel);
  hide_obj(ui.voice_sel.title_label);
  hide_obj(ui.voice_sel.left_bg);
  hide_obj(ui.voice_sel.right_bg);
  for (int i = 0; i < VOICE_GENDER_COUNT; i++) {
    hide_obj(ui.voice_sel.gender_labels[i]);
  }
  for (int i = 0; i < 6; i++) {
    hide_obj(ui.voice_sel.timbre_labels[i]);
  }
}

/* ===================================================================
 *  voice selector panel (dual-column)
 * =================================================================== */

/* refresh right-column timbre labels for current gender */
static void voice_sel_refresh_timbres(void) {
  voice_select_viz_t *vs = &ui.voice_sel;
  voice_gender_t gender = (voice_gender_t)vs->gender_idx;
  vs->timbre_count = voice_config_get_gender_voice_count(gender);

  for (int i = 0; i < 6; i++) {
    if (i < vs->timbre_count) {
      const char *name = voice_config_get_gender_voice_name(gender, i);
      lv_label_set_text(vs->timbre_labels[i], name);
      lv_obj_set_style_text_color(vs->timbre_labels[i],
          (i == vs->timbre_idx) ? C_BLUE : C_TEXT_GRAY, 0);
      show_obj(vs->timbre_labels[i]);
    } else {
      hide_obj(vs->timbre_labels[i]);
    }
  }

  /* update gender highlight */
  for (int g = 0; g < VOICE_GENDER_COUNT; g++) {
    lv_obj_set_style_text_color(vs->gender_labels[g],
        (g == vs->gender_idx) ? C_BLUE : C_TEXT_GRAY, 0);
  }

  /* clamp timbre_idx */
  if (vs->timbre_idx >= vs->timbre_count) vs->timbre_idx = 0;
}

static void on_gender_click(lv_event_t *e) {
  lv_obj_t *target = lv_event_get_target_obj(e);
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  ui.voice_sel.gender_idx = idx;
  ui.voice_sel.timbre_idx = 0;
  voice_sel_refresh_timbres();
  (void)target;
}

static void on_timbre_click(lv_event_t *e) {
  lv_obj_t *target = lv_event_get_target_obj(e);
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  ui.voice_sel.timbre_idx = idx;
  voice_sel_refresh_timbres();
  (void)target;
}

static void on_voice_sel_cancel(lv_event_t *e) {
  (void)e;
  ai_chat_ui_show_voice_selector(false);
  ai_chat_ui_set_state(CHAT_IDLE);
}

static void on_voice_sel_confirm(lv_event_t *e) {
  (void)e;
  voice_select_viz_t *vs = &ui.voice_sel;
  voice_gender_t g = (voice_gender_t)vs->gender_idx;
  int sel = voice_config_get_gender_voice_id(g, vs->timbre_idx);
  voice_config_set(NULL, sel);
  ai_chat_ui_show_voice_selector(false);
  ai_chat_ui_set_state(CHAT_IDLE);
}

static void create_voice_select_viz(void) {
  voice_select_viz_t *vs = &ui.voice_sel;
  const int pw = 230, ph = 140;
  const int px = ORB_CX - pw / 2, py = ORB_CY - ph / 2;

  /* dark card */
  vs->panel = lv_obj_create(lv_screen_active());
  lv_obj_set_size(vs->panel, pw, ph);
  lv_obj_set_pos(vs->panel, px, py);
  lv_obj_set_style_bg_color(vs->panel, lv_color_hex(0x18181B), 0);
  lv_obj_set_style_bg_opa(vs->panel, LV_OPA_90, 0);
  lv_obj_set_style_border_width(vs->panel, 1, 0);
  lv_obj_set_style_border_color(vs->panel, lv_color_hex(0x3730A3), 0);
  lv_obj_set_style_radius(vs->panel, 12, 0);
  lv_obj_set_style_pad_all(vs->panel, 0, 0);

  /* title */
  vs->title_label = lv_label_create(vs->panel);
  lv_label_set_text(vs->title_label, "Voice Select");
  lv_obj_set_style_text_color(vs->title_label, C_TEXT_GRAY, 0);
  lv_obj_set_style_text_font(vs->title_label, &lv_font_montserrat_14, 0);
  lv_obj_align(vs->title_label, LV_ALIGN_TOP_MID, 0, 6);

  /* left column bg */
  vs->left_bg = lv_obj_create(vs->panel);
  lv_obj_set_size(vs->left_bg, 80, 76);
  lv_obj_set_pos(vs->left_bg, 6, 28);
  lv_obj_set_style_bg_color(vs->left_bg, lv_color_hex(0x0F0F14), 0);
  lv_obj_set_style_bg_opa(vs->left_bg, LV_OPA_70, 0);
  lv_obj_set_style_border_width(vs->left_bg, 0, 0);
  lv_obj_set_style_radius(vs->left_bg, 6, 0);
  lv_obj_set_style_pad_all(vs->left_bg, 0, 0);

  /* right column bg */
  vs->right_bg = lv_obj_create(vs->panel);
  lv_obj_set_size(vs->right_bg, 132, 76);
  lv_obj_set_pos(vs->right_bg, 92, 28);
  lv_obj_set_style_bg_color(vs->right_bg, lv_color_hex(0x0F0F14), 0);
  lv_obj_set_style_bg_opa(vs->right_bg, LV_OPA_70, 0);
  lv_obj_set_style_border_width(vs->right_bg, 0, 0);
  lv_obj_set_style_radius(vs->right_bg, 6, 0);
  lv_obj_set_style_pad_all(vs->right_bg, 0, 0);

  /* gender labels (left column) */
  const int gl_y[VOICE_GENDER_COUNT] = {4, 28, 52};
  for (int g = 0; g < VOICE_GENDER_COUNT; g++) {
    vs->gender_labels[g] = lv_label_create(vs->left_bg);
    lv_label_set_text(vs->gender_labels[g],
                      voice_config_get_gender_name((voice_gender_t)g));
    lv_obj_set_style_text_font(vs->gender_labels[g],
                               &lv_font_montserrat_14, 0);
    lv_obj_set_pos(vs->gender_labels[g], 10, gl_y[g]);
    lv_obj_add_flag(vs->gender_labels[g], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(vs->gender_labels[g], on_gender_click,
                        LV_EVENT_CLICKED, (void *)(intptr_t)g);
  }

  /* timbre labels (right column, max 6) */
  for (int i = 0; i < 6; i++) {
    vs->timbre_labels[i] = lv_label_create(vs->right_bg);
    lv_label_set_text(vs->timbre_labels[i], "");
    lv_obj_set_style_text_font(vs->timbre_labels[i],
                               &lv_font_montserrat_14, 0);
    lv_obj_set_pos(vs->timbre_labels[i], 8, 4 + i * 20);
    lv_obj_add_flag(vs->timbre_labels[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(vs->timbre_labels[i], on_timbre_click,
                        LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  /* Cancel button */
  vs->btn_cancel = lv_obj_create(vs->panel);
  lv_obj_set_size(vs->btn_cancel, 70, 24);
  lv_obj_set_pos(vs->btn_cancel, 30, 110);
  lv_obj_set_style_bg_color(vs->btn_cancel, lv_color_hex(0x374151), 0);
  lv_obj_set_style_bg_opa(vs->btn_cancel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(vs->btn_cancel, 0, 0);
  lv_obj_set_style_radius(vs->btn_cancel, 4, 0);
  lv_obj_set_style_pad_all(vs->btn_cancel, 0, 0);
  lv_obj_add_flag(vs->btn_cancel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(vs->btn_cancel, on_voice_sel_cancel,
                      LV_EVENT_CLICKED, NULL);
  vs->cancel_label = lv_label_create(vs->btn_cancel);
  lv_label_set_text(vs->cancel_label, "Cancel");
  lv_obj_set_style_text_color(vs->cancel_label, C_TEXT_GRAY, 0);
  lv_obj_set_style_text_font(vs->cancel_label, &lv_font_montserrat_14, 0);
  lv_obj_center(vs->cancel_label);

  /* Confirm button */
  vs->btn_confirm = lv_obj_create(vs->panel);
  lv_obj_set_size(vs->btn_confirm, 70, 24);
  lv_obj_set_pos(vs->btn_confirm, 130, 110);
  lv_obj_set_style_bg_color(vs->btn_confirm, C_BLUE, 0);
  lv_obj_set_style_bg_opa(vs->btn_confirm, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(vs->btn_confirm, 0, 0);
  lv_obj_set_style_radius(vs->btn_confirm, 4, 0);
  lv_obj_set_style_pad_all(vs->btn_confirm, 0, 0);
  lv_obj_add_flag(vs->btn_confirm, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(vs->btn_confirm, on_voice_sel_confirm,
                      LV_EVENT_CLICKED, NULL);
  vs->confirm_label = lv_label_create(vs->btn_confirm);
  lv_label_set_text(vs->confirm_label, "Confirm");
  lv_obj_set_style_text_color(vs->confirm_label, C_TEXT, 0);
  lv_obj_set_style_text_font(vs->confirm_label, &lv_font_montserrat_14, 0);
  lv_obj_center(vs->confirm_label);

  /* initial state */
  vs->gender_idx = 0;
  vs->timbre_idx = 0;
  voice_sel_refresh_timbres();
}

/* ===================================================================
 *  float ball (voice settings entry, always visible)
 * =================================================================== */

static void on_float_ball_click(lv_event_t *e) {
  (void)e;
  if (s_state == CHAT_VOICE_SELECT) {
    /* exit without saving */
    ai_chat_ui_show_voice_selector(false);
    ai_chat_ui_set_state(CHAT_IDLE);
  } else {
    /* enter voice selector */
    ai_chat_ui_show_voice_selector(true);
    ai_chat_ui_set_state(CHAT_VOICE_SELECT);
  }
}

static void create_float_ball(void) {
  const int size = 28;
  const int x = 282, y = 200;

  ui.float_ball = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.float_ball, size, size);
  lv_obj_set_pos(ui.float_ball, x, y);
  lv_obj_set_style_bg_color(ui.float_ball,
                            lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(ui.float_ball, LV_OPA_30, 0);
  lv_obj_set_style_border_width(ui.float_ball, 1, 0);
  lv_obj_set_style_border_color(ui.float_ball,
                                lv_color_hex(0x9CA3AF), 0);
  lv_obj_set_style_radius(ui.float_ball, size / 2, 0);
  lv_obj_set_style_pad_all(ui.float_ball, 0, 0);
  lv_obj_add_flag(ui.float_ball, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ui.float_ball, on_float_ball_click,
                      LV_EVENT_CLICKED, NULL);

  /* gear icon: "?" */
  ui.float_ball_label = lv_label_create(ui.float_ball);
  lv_label_set_text(ui.float_ball_label, LV_SYMBOL_SETTINGS);
  lv_obj_set_style_text_color(ui.float_ball_label, C_TEXT, 0);
  lv_obj_set_style_text_font(ui.float_ball_label,
                             &lv_font_montserrat_14, 0);
  lv_obj_center(ui.float_ball_label);
}

/* ===================================================================
 *  public API
 * =================================================================== */

void ai_chat_ui_init(void) {
  ESP_LOGI(TAG, "Creating Voice Assistant UI");

  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, C_BG, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  create_top_bar();
  create_idle_viz();
  create_listening_viz();
  create_speaking_viz();
  create_thinking_viz();
  create_disconnected_viz();
  create_bottom_text();
  create_voice_select_viz();
  create_float_ball();

  hide_all_viz();
  show_obj(ui.idle.ring_outer);
  show_obj(ui.idle.ring_mid);
  show_obj(ui.idle.core);
  s_state = CHAT_IDLE;

  ESP_LOGI(TAG, "UI ready");
}

void ai_chat_ui_tick(void) {
  lv_timer_handler();
}

chat_state_t ai_chat_ui_get_state(void) {
  return s_state;
}

void ai_chat_ui_set_state(chat_state_t state) {
  if (state == s_state && state != CHAT_INTERRUPTED) return;
  s_state = state;

  hide_all_viz();

  lv_color_t state_color = C_TEXT;
  const char *state_text = "";
  const char *hint_text = "";

  switch (state) {
    case CHAT_IDLE:
      show_obj(ui.idle.ring_outer);
      show_obj(ui.idle.ring_mid);
      show_obj(ui.idle.core);
      state_color = C_GREEN;
      state_text = "Idle";
      hint_text = "Tap to start";
      break;

    case CHAT_LISTENING:
      show_obj(ui.listening.circle);
      show_obj(ui.listening.icon_mic);
      show_obj(ui.listening.icon_stand);
      show_obj(ui.listening.icon_base);
      for (int i = 0; i < 5; i++) {
        show_obj(ui.listening.bars_l[i]);
        show_obj(ui.listening.bars_r[i]);
      }
      state_color = C_BLUE;
      state_text = "Listening";
      hint_text = "Please wait";
      break;

    case CHAT_SPEAKING:
      show_obj(ui.speaking.circle);
      show_obj(ui.speaking.icon_speaker);
      for (int i = 0; i < 3; i++) show_obj(ui.speaking.wave_arcs[i]);
      state_color = C_PURPLE;
      state_text = "AI speaking";
      hint_text = "Playing";
      break;

    case CHAT_THINKING:
      show_obj(ui.thinking.circle);
      show_obj(ui.thinking.static_arc);
      show_obj(ui.thinking.spin_arc);
      state_color = C_PURPLE;
      state_text = "Thinking";
      hint_text = "Generating...";
      break;

    case CHAT_DISCONNECTED:
      show_obj(ui.disconnected.circle);
      show_obj(ui.disconnected.bar1);
      show_obj(ui.disconnected.bar2);
      state_color = C_RED;
      state_text = "Disconnected";
      hint_text = "Check network";
      break;

    case CHAT_INTERRUPTED:
      show_obj(ui.idle.ring_outer);
      show_obj(ui.idle.ring_mid);
      show_obj(ui.idle.core);
      state_color = C_TEXT_GRAY;
      state_text = "Interrupted";
      hint_text = " ";
      break;

    case CHAT_VOICE_SELECT:
      show_obj(ui.idle.ring_outer);
      show_obj(ui.idle.ring_mid);
      show_obj(ui.idle.core);
      state_color = C_BLUE;
      state_text = "Voice";
      hint_text = "Short: next  Long: confirm";
      break;

    default:
      break;
  }

  lv_label_set_text(ui.state_label, state_text);
  lv_obj_set_style_text_color(ui.state_label, state_color, 0);
  lv_label_set_text(ui.hint_label, hint_text);
}

void ai_chat_ui_set_network(bool online) {
  lv_label_set_text(ui.status_label, online ? "Online" : "Offline");
  lv_obj_set_style_bg_color(ui.status_dot, online ? C_GREEN : C_RED, 0);
}

void ai_chat_ui_set_connection(const char *ssid, const char *ip,
                               bool online) {
  (void)ssid;
  (void)ip;
  ai_chat_ui_set_network(online);
}

void ai_chat_ui_update_volume(uint8_t level) {
  s_volume = level;
  if (s_state != CHAT_LISTENING) return;

  /* shape: small/medium/large/medium/small to mimic bar envelope */
  static const int factor[5] = { 30, 70, 100, 80, 40 };
  int base = 4 + (level * 22 / 100);  /* 4..26 */
  if (base > 26) base = 26;

  for (int i = 0; i < 5; i++) {
    int h = base * factor[i] / 100;
    if (h < 4) h = 4;
    if (h > 28) h = 28;
    lv_anim_set_values(&ui.listening.anims_l[i], 4, h);
    lv_anim_set_values(&ui.listening.anims_r[i], 4, h);
  }
}

void ai_chat_ui_add_message(const char *text, bool is_user) {
  (void)text;
  (void)is_user;
}

void ai_chat_ui_show_voice_selector(bool show) {
  voice_select_viz_t *vs = &ui.voice_sel;

  if (show) {
    /* load current voice and resolve gender/timbre */
    int vid = voice_config_get();
    voice_gender_t g = voice_config_get_gender(vid);
    int tcnt = voice_config_get_gender_voice_count(g);
    vs->gender_idx = (int)g;
    vs->timbre_idx = 0;
    for (int i = 0; i < tcnt; i++) {
      if (voice_config_get_gender_voice_id(g, i) == vid) {
        vs->timbre_idx = i;
        break;
      }
    }

    voice_sel_refresh_timbres();

    show_obj(vs->panel);
    show_obj(vs->title_label);
    show_obj(vs->left_bg);
    show_obj(vs->right_bg);
    show_obj(vs->btn_cancel);
    show_obj(vs->btn_confirm);
    for (int i = 0; i < VOICE_GENDER_COUNT; i++) {
      show_obj(vs->gender_labels[i]);
    }
    /* timbre_labels visibility handled by voice_sel_refresh_timbres */
  } else {
    hide_obj(vs->panel);
    hide_obj(vs->title_label);
    hide_obj(vs->left_bg);
    hide_obj(vs->right_bg);
    hide_obj(vs->btn_cancel);
    hide_obj(vs->btn_confirm);
    for (int i = 0; i < VOICE_GENDER_COUNT; i++) {
      hide_obj(vs->gender_labels[i]);
    }
    for (int i = 0; i < 6; i++) {
      hide_obj(vs->timbre_labels[i]);
    }
  }
}

void ai_chat_ui_voice_select_next(void) {
  voice_select_viz_t *vs = &ui.voice_sel;

  /* cycle timbre within current gender */
  vs->timbre_idx = (vs->timbre_idx + 1) % vs->timbre_count;
  voice_sel_refresh_timbres();
}

int ai_chat_ui_voice_select_get(void) {
  voice_select_viz_t *vs = &ui.voice_sel;
  voice_gender_t g = (voice_gender_t)vs->gender_idx;
  return voice_config_get_gender_voice_id(g, vs->timbre_idx);
}