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
#include "lvgl_port.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
LV_FONT_DECLARE(lv_font_custom_cjk_14);
LV_FONT_DECLARE(lv_font_custom_cjk_16);


static const char *TAG = "ai_chat_ui";

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
  lv_obj_t *panel;             /* full-screen card */
  lv_obj_t *btn_back;          /* top-left back button */
  lv_obj_t *title_label;       /* "音色选择" */
  lv_obj_t *btn_save;          /* top-right save button */
  lv_obj_t *btn_male;          /* gender toggle: 男声 */
  lv_obj_t *btn_female;        /* gender toggle: 女声 */
  lv_obj_t *icon_label;        /* circle container */
  lv_obj_t *icon_text;         /* text inside icon (F/M/R) */
  lv_obj_t *name_label;        /* voice name, e.g. "温润男声" */
  lv_obj_t *name_prev;         /* previous voice name (dimmed, left) */
  lv_obj_t *name_next;         /* next voice name (dimmed, right) */
  lv_obj_t *desc_label;        /* description line 1 */
  lv_obj_t *tags_label;        /* tags, e.g. "清晰 年轻 活力" */
  lv_obj_t *dots[6];           /* page indicator dots */
  lv_obj_t *code_label;        /* code, e.g. "M02" */
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
  lv_obj_t *float_ball_lines[3];  /* hamburger menu icon */
  lv_obj_t *touch_dot;            /* touch indicator red dot */
} ui;

static chat_state_t s_state = CHAT_IDLE;
static uint8_t s_volume = 0;
static volatile bool s_vol_dirty = false;

/* ===================================================================
 *  animation callbacks
 * =================================================================== */

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
  lv_obj_set_style_text_color(wifi_icon, C_TEXT, 0);
  lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_14, 0);
  lv_obj_align(wifi_icon, LV_ALIGN_TOP_RIGHT, -8, 10);
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
  /* IMPORTANT: stop every running animation before hiding objects.
   * Without this, the breathing/pulse/wave animations continue to
   * mutate hidden widgets every few ms, flooding LVGL's invalid-area
   * queue and driving lv_timer_handler from 5ms to 50-90ms per frame,
   * which manifests as "界面卡死" within minutes. */
  lv_anim_delete_all();

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
  /* Voice-select panel: only hide the panel itself.
   * All children (buttons, labels, dots) are created as panel children,
   * so they are invisible while the panel is hidden.  We must NOT
   * individually hide them, because set_state() only re-shows the
   * panel -- individually-hidden children would stay invisible. */
  hide_obj(ui.voice_sel.panel);
}

/* ===================================================================
 *  animation restarters (called from set_state() AFTER lv_anim_delete_all
 *  has been issued in hide_all_viz, so re-showing a viz re-launches
 *  its breathing/pulse/wave animations cleanly).
 * =================================================================== */
static void start_idle_anims(void) {
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

static void start_listening_anims(void) {
  static const int default_h[5] = { 8, 16, 26, 18, 10 };
  for (int i = 0; i < 5; i++) {
    anim_init_bar(&ui.listening.anims_l[i], ui.listening.bars_l[i],
                  4, default_h[i], 500 + i * 80);
    lv_anim_start(&ui.listening.anims_l[i]);
    anim_init_bar(&ui.listening.anims_r[i], ui.listening.bars_r[i],
                  4, default_h[i], 500 + i * 80);
    lv_anim_start(&ui.listening.anims_r[i]);
  }
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

static void start_speaking_anims(void) {
  static const int wave_sizes[3] = { 96, 110, 124 };
  for (int i = 0; i < 3; i++) {
    int s = wave_sizes[i];
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

static void start_thinking_anims(void) {
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
 *  voice selector - card style with page dots
 * =================================================================== */

/* Refresh the voice card display for current gender + timbre_idx */
static void voice_sel_refresh_timbres(void) {
  voice_select_viz_t *vs = &ui.voice_sel;
  voice_gender_t gender = (voice_gender_t)vs->gender_idx;
  vs->timbre_count = voice_config_get_gender_voice_count(gender);
  if (vs->timbre_count < 1) vs->timbre_count = 1;
  if (vs->timbre_idx >= vs->timbre_count) vs->timbre_idx = 0;

  int vid = voice_config_get_gender_voice_id(gender, vs->timbre_idx);
  const voice_entry_t *ve = &voice_config_get_list()[vid];

  /* Update icon text */
  const char *icons[] = { "F", "M", "R" };
  lv_label_set_text(vs->icon_text, icons[vs->gender_idx]);
  lv_obj_set_style_bg_color(vs->icon_label,
      (vs->gender_idx == 0) ? C_PURPLE :
      (vs->gender_idx == 1) ? C_BLUE : C_GREEN, 0);

  /* Name + desc + tags + code */
  lv_label_set_text(vs->name_label, ve->name);
  lv_label_set_text(vs->desc_label, ve->desc);
  lv_label_set_text(vs->tags_label, ve->tags);
  lv_label_set_text(vs->code_label, ve->code);

  /* Side labels: show prev/next voice names (dimmed) */
  int prev_idx = (vs->timbre_idx - 1 + vs->timbre_count) % vs->timbre_count;
  int next_idx = (vs->timbre_idx + 1) % vs->timbre_count;
  int prev_vid = voice_config_get_gender_voice_id(gender, prev_idx);
  int next_vid = voice_config_get_gender_voice_id(gender, next_idx);
  lv_label_set_text(vs->name_prev, voice_config_get_list()[prev_vid].name);
  lv_label_set_text(vs->name_next, voice_config_get_list()[next_vid].name);

  /* Gender toggle highlight */
  lv_obj_set_style_bg_opa(vs->btn_male,
      (vs->gender_idx == 1) ? LV_OPA_COVER : LV_OPA_20, 0);
  lv_obj_set_style_bg_opa(vs->btn_female,
      (vs->gender_idx == 0) ? LV_OPA_COVER : LV_OPA_20, 0);

  /* Page dots */
  for (int i = 0; i < 6; i++) {
    if (i < vs->timbre_count) {
      show_obj(vs->dots[i]);
      lv_obj_set_style_bg_color(vs->dots[i],
          (i == vs->timbre_idx) ? C_BLUE :
          lv_color_hex(0x4B5563), 0);
    } else {
      hide_obj(vs->dots[i]);
    }
  }
}

/* Gender toggle callback */
static void on_gender_btn_click(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  if (ui.voice_sel.gender_idx == idx) return;
  ui.voice_sel.gender_idx = idx;
  ui.voice_sel.timbre_idx = 0;
  ESP_LOGI(TAG, "voice gender -> %d (%s)", idx,
           voice_config_get_gender_name((voice_gender_t)idx));
  voice_sel_refresh_timbres();
}

/* Save = confirm */
static void on_voice_sel_confirm(lv_event_t *e) {
  (void)e;
  voice_select_viz_t *vs = &ui.voice_sel;
  voice_gender_t g = (voice_gender_t)vs->gender_idx;
  int sel = voice_config_get_gender_voice_id(g, vs->timbre_idx);
  const char *name = voice_config_get_gender_voice_name(g, vs->timbre_idx);
  ESP_LOGI(TAG, "voice confirm: id=%d name=%s", sel, name);
  voice_config_set(NULL, sel);
  ESP_LOGI(TAG, "voice set done: id=%d (%s)", sel, name);
  ai_chat_ui_show_voice_selector(false);
  ai_chat_ui_set_state(CHAT_IDLE);
}

/* Back = cancel */
static void on_voice_sel_back(lv_event_t *e) {
  (void)e;
  ESP_LOGI(TAG, "voice select cancelled");
  ai_chat_ui_show_voice_selector(false);
  ai_chat_ui_set_state(CHAT_IDLE);
}

static void create_voice_select_viz(void) {
  voice_select_viz_t *vs = &ui.voice_sel;
  lv_obj_t *scr = lv_screen_active();

  /* ---- Full-screen panel ---- */
  vs->panel = lv_obj_create(scr);
  lv_obj_set_size(vs->panel, 320, 240);
  lv_obj_set_pos(vs->panel, 0, 0);
  lv_obj_set_style_bg_color(vs->panel, lv_color_hex(0x0F0F14), 0);
  lv_obj_set_style_bg_opa(vs->panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(vs->panel, 0, 0);
  lv_obj_set_style_radius(vs->panel, 0, 0);
  lv_obj_set_style_pad_all(vs->panel, 0, 0);
  lv_obj_clear_flag(vs->panel, LV_OBJ_FLAG_SCROLLABLE);

  /* ---- Top bar: back | title | save ---- */
  vs->btn_back = lv_obj_create(vs->panel);
  lv_obj_set_size(vs->btn_back, 50, 28);
  lv_obj_set_pos(vs->btn_back, 6, 6);
  lv_obj_set_style_bg_color(vs->btn_back, lv_color_hex(0x374151), 0);
  lv_obj_set_style_bg_opa(vs->btn_back, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(vs->btn_back, 6, 0);
  lv_obj_set_style_border_width(vs->btn_back, 0, 0);
  lv_obj_set_style_pad_all(vs->btn_back, 0, 0);
  lv_obj_add_flag(vs->btn_back, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(vs->btn_back, on_voice_sel_back,
                      LV_EVENT_CLICKED, NULL);
  {
    lv_obj_t *l = lv_label_create(vs->btn_back);
    lv_label_set_text(l, "<");
    lv_obj_set_style_text_color(l, C_TEXT, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_center(l);
  }

  vs->title_label = lv_label_create(vs->panel);
  lv_label_set_text(vs->title_label, "Voice");
  lv_obj_set_style_text_color(vs->title_label, C_TEXT, 0);
  lv_obj_set_style_text_font(vs->title_label, &lv_font_montserrat_16, 0);
  lv_obj_align(vs->title_label, LV_ALIGN_TOP_MID, 0, 12);

  vs->btn_save = lv_obj_create(vs->panel);
  lv_obj_set_size(vs->btn_save, 50, 28);
  lv_obj_set_pos(vs->btn_save, 264, 6);
  lv_obj_set_style_bg_color(vs->btn_save, C_BLUE, 0);
  lv_obj_set_style_bg_opa(vs->btn_save, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(vs->btn_save, 6, 0);
  lv_obj_set_style_border_width(vs->btn_save, 0, 0);
  lv_obj_set_style_pad_all(vs->btn_save, 0, 0);
  lv_obj_add_flag(vs->btn_save, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(vs->btn_save, on_voice_sel_confirm,
                      LV_EVENT_CLICKED, NULL);
  {
    lv_obj_t *l = lv_label_create(vs->btn_save);
    lv_label_set_text(l, "OK");
    lv_obj_set_style_text_color(l, C_TEXT, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_center(l);
  }

  /* ---- Gender toggle: Male | Female ---- */
  vs->btn_male = lv_obj_create(vs->panel);
  lv_obj_set_size(vs->btn_male, 70, 28);
  lv_obj_set_pos(vs->btn_male, 80, 44);
  lv_obj_set_style_bg_color(vs->btn_male, C_BLUE, 0);
  lv_obj_set_style_bg_opa(vs->btn_male, LV_OPA_20, 0);
  lv_obj_set_style_radius(vs->btn_male, 6, 0);
  lv_obj_set_style_border_width(vs->btn_male, 0, 0);
  lv_obj_set_style_pad_all(vs->btn_male, 0, 0);
  lv_obj_add_flag(vs->btn_male, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(vs->btn_male, on_gender_btn_click,
                      LV_EVENT_CLICKED, (void *)(intptr_t)1);
  {
    lv_obj_t *l = lv_label_create(vs->btn_male);
    lv_label_set_text(l, "Male");
    lv_obj_set_style_text_color(l, C_TEXT, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_center(l);
  }

  vs->btn_female = lv_obj_create(vs->panel);
  lv_obj_set_size(vs->btn_female, 70, 28);
  lv_obj_set_pos(vs->btn_female, 170, 44);
  lv_obj_set_style_bg_color(vs->btn_female, C_PURPLE, 0);
  lv_obj_set_style_bg_opa(vs->btn_female, LV_OPA_20, 0);
  lv_obj_set_style_radius(vs->btn_female, 6, 0);
  lv_obj_set_style_border_width(vs->btn_female, 0, 0);
  lv_obj_set_style_pad_all(vs->btn_female, 0, 0);
  lv_obj_add_flag(vs->btn_female, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(vs->btn_female, on_gender_btn_click,
                      LV_EVENT_CLICKED, (void *)(intptr_t)0);
  {
    lv_obj_t *l = lv_label_create(vs->btn_female);
    lv_label_set_text(l, "Female");
    lv_obj_set_style_text_color(l, C_TEXT, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_center(l);
  }

  /* ---- Center icon (circle with letter) ---- */
  vs->icon_label = lv_obj_create(vs->panel);
  lv_obj_set_size(vs->icon_label, 56, 56);
  lv_obj_align(vs->icon_label, LV_ALIGN_TOP_MID, 0, 82);
  lv_obj_set_style_bg_color(vs->icon_label, C_BLUE, 0);
  lv_obj_set_style_bg_opa(vs->icon_label, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(vs->icon_label, 28, 0);
  lv_obj_set_style_border_width(vs->icon_label, 0, 0);
  lv_obj_set_style_pad_all(vs->icon_label, 0, 0);
  lv_obj_clear_flag(vs->icon_label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  vs->icon_text = lv_label_create(vs->icon_label);
  lv_label_set_text(vs->icon_text, "M");
  lv_obj_set_style_text_color(vs->icon_text, C_TEXT, 0);
  lv_obj_set_style_text_font(vs->icon_text, &lv_font_montserrat_16, 0);
  lv_obj_center(vs->icon_text);

  /* ---- Voice name ---- */
  vs->name_label = lv_label_create(vs->panel);
  lv_label_set_text(vs->name_label, "");
  lv_obj_set_style_text_color(vs->name_label, C_TEXT, 0);
  lv_obj_set_style_text_font(vs->name_label, &lv_font_custom_cjk_16, 0);
  lv_obj_align(vs->name_label, LV_ALIGN_TOP_MID, 0, 146);

  /* ---- Side voice names (prev/next, dimmed) ---- */
  vs->name_prev = lv_label_create(vs->panel);
  lv_label_set_text(vs->name_prev, "");
  lv_obj_set_style_text_color(vs->name_prev, C_TEXT_GRAY, 0);
  lv_obj_set_style_text_opa(vs->name_prev, LV_OPA_40, 0);
  lv_obj_set_style_text_font(vs->name_prev, &lv_font_custom_cjk_14, 0);
  lv_obj_set_style_text_align(vs->name_prev, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(vs->name_prev, 8, 150);

  vs->name_next = lv_label_create(vs->panel);
  lv_label_set_text(vs->name_next, "");
  lv_obj_set_style_text_color(vs->name_next, C_TEXT_GRAY, 0);
  lv_obj_set_style_text_opa(vs->name_next, LV_OPA_40, 0);
  lv_obj_set_style_text_font(vs->name_next, &lv_font_custom_cjk_14, 0);
  lv_obj_set_style_text_align(vs->name_next, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_pos(vs->name_next, 220, 150);

  /* ---- Description ---- */
  vs->desc_label = lv_label_create(vs->panel);
  lv_label_set_text(vs->desc_label, "");
  lv_obj_set_style_text_color(vs->desc_label, C_TEXT_GRAY, 0);
  lv_obj_set_style_text_font(vs->desc_label, &lv_font_custom_cjk_14, 0);
  lv_obj_align(vs->desc_label, LV_ALIGN_TOP_MID, 0, 168);

  /* ---- Tags ---- */
  vs->tags_label = lv_label_create(vs->panel);
  lv_label_set_text(vs->tags_label, "");
  lv_obj_set_style_text_color(vs->tags_label, C_BLUE, 0);
  lv_obj_set_style_text_font(vs->tags_label, &lv_font_custom_cjk_14, 0);
  lv_obj_align(vs->tags_label, LV_ALIGN_TOP_MID, 0, 188);

  /* ---- Bottom: page dots (centered) ---- */
  for (int i = 0; i < 6; i++) {
    vs->dots[i] = lv_obj_create(vs->panel);
    lv_obj_set_size(vs->dots[i], 8, 8);
    lv_obj_set_pos(vs->dots[i], 120 + i * 16, 210);
    lv_obj_set_style_bg_color(vs->dots[i], lv_color_hex(0x4B5563), 0);
    lv_obj_set_style_bg_opa(vs->dots[i], LV_OPA_COVER, 0);
    lv_obj_set_style_radius(vs->dots[i], 4, 0);
    lv_obj_set_style_border_width(vs->dots[i], 0, 0);
    lv_obj_clear_flag(vs->dots[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  }

  /* ---- Code label (bottom-left corner) ---- */
  vs->code_label = lv_label_create(vs->panel);
  lv_label_set_text(vs->code_label, "");
  lv_obj_set_style_text_color(vs->code_label, C_TEXT_GRAY, 0);
  lv_obj_set_style_text_font(vs->code_label, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(vs->code_label, 8, 212);

  /* Initial state */
  vs->gender_idx = 0;
  vs->timbre_idx = 0;
  voice_sel_refresh_timbres();
}

/* ===================================================================
 *  float ball (voice settings entry, always visible)
 * =================================================================== */

/* long-press on the screen background -> open voice selector */
static void on_screen_long_press(lv_event_t *e) {
  (void)e;
  if (s_state == CHAT_VOICE_SELECT) return;
  ai_chat_ui_show_voice_selector(true);
  ai_chat_ui_set_state(CHAT_VOICE_SELECT);
}

static void on_float_ball_click(lv_event_t *e) {
  (void)e;
  if (s_state == CHAT_VOICE_SELECT) {
    /* In voice-select mode, float ball = cancel/back */
    on_voice_sel_back(NULL);
  } else {
    /* enter voice selector */
    ai_chat_ui_show_voice_selector(true);
    ai_chat_ui_set_state(CHAT_VOICE_SELECT);
  }
}

static void create_float_ball(void) {
  /* A normal-sized 56x56 floating ball in the bottom-right corner,
   * acting as the voice-settings entry. */
  const int size = 44;
  const int x = 320 - size - 10;   /* 266 */
  const int y = 240 - size - 10;   /* 186 */

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

  /* hamburger menu icon: 3 short bars centered in the ball */
  for (int i = 0; i < 3; i++) {
    ui.float_ball_lines[i] = lv_obj_create(ui.float_ball);
    lv_obj_set_size(ui.float_ball_lines[i], 22, 3);
    lv_obj_set_pos(ui.float_ball_lines[i], 11, 14 + i * 9);
    lv_obj_set_style_bg_color(ui.float_ball_lines[i], C_TEXT, 0);
    lv_obj_set_style_bg_opa(ui.float_ball_lines[i], LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui.float_ball_lines[i], 0, 0);
    lv_obj_set_style_radius(ui.float_ball_lines[i], 1, 0);
    lv_obj_set_style_pad_all(ui.float_ball_lines[i], 0, 0);
    lv_obj_remove_flag(ui.float_ball_lines[i], LV_OBJ_FLAG_CLICKABLE);
  }
}

/* ===================================================================
 *  public API
 * =================================================================== */

void ai_chat_ui_init(void) {
  ESP_LOGI(TAG, "Creating Voice Assistant UI");

  /* 必须持锁：lvgl_task(CPU1) 同时可能在跑 lv_timer_handler，
   * 不加锁会跨核竞争 LVGL 内部状态，导致死循环/卡死。 */
  if (!lvgl_port_lock(pdMS_TO_TICKS(2000))) {
    ESP_LOGE(TAG, "init: failed to acquire LVGL lock");
    return;
  }

  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, C_BG, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  /* 本 UI 无可滚动内容；禁止滚动，避免任何异常触摸坐标被 LVGL
   * 当成拖拽手势把整屏推偏（此前乱码坐标即导致画面被拖到右）。 */
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  create_top_bar();
  create_idle_viz();
  create_listening_viz();
  create_speaking_viz();
  create_thinking_viz();
  create_disconnected_viz();
  create_bottom_text();
  create_voice_select_viz();
  create_float_ball();

  /* Long-press anywhere on the screen (1.5s) opens the voice
   * selector. Backup for when the user can't hit the small float ball. */
  lv_obj_add_event_cb(scr, on_screen_long_press, LV_EVENT_LONG_PRESSED, NULL);

  hide_all_viz();
  show_obj(ui.idle.ring_outer);
  show_obj(ui.idle.ring_mid);
  show_obj(ui.idle.core);
  start_idle_anims();
  s_state = CHAT_IDLE;

  lvgl_port_unlock();
  ESP_LOGI(TAG, "UI ready");
}

/* Forward declaration: defined later in this file. */
static void ai_chat_ui_apply_volume(void);

void ai_chat_ui_tick(void) {
  /* LVGL timers are already driven by lvgl_task (CPU1). Here we only apply
   * any pending volume change that was queued by the audio capture task. */
  ai_chat_ui_apply_volume();
}

chat_state_t ai_chat_ui_get_state(void) {
  return s_state;
}

void ai_chat_ui_set_state(chat_state_t state) {
  /* Debounce: ignore rapid duplicate state changes within 300ms.
   * SDK callbacks can fire IDLE->LISTENING->IDLE in quick succession,
   * each triggering hide_all_viz + show + anim restart, which burns
   * LVGL CPU and eventually stalls. */
  static TickType_t s_last_change = 0;
  TickType_t now = xTaskGetTickCount();
  if (state == s_state && state != CHAT_INTERRUPTED) return;
  if (state == s_state && (now - s_last_change) * portTICK_PERIOD_MS < 300) return;
  s_last_change = now;

  if (!lvgl_port_lock(pdMS_TO_TICKS(100))) {
    ESP_LOGE(TAG, "set_state: failed to acquire LVGL lock");
    return;
  }
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
      start_idle_anims();
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
      start_listening_anims();
      state_color = C_BLUE;
      state_text = "Listening";
      hint_text = "Please wait";
      break;

    case CHAT_SPEAKING:
      show_obj(ui.speaking.circle);
      show_obj(ui.speaking.icon_speaker);
      for (int i = 0; i < 3; i++) show_obj(ui.speaking.wave_arcs[i]);
      start_speaking_anims();
      state_color = C_PURPLE;
      state_text = "AI speaking";
      hint_text = "Playing";
      break;

    case CHAT_THINKING:
      show_obj(ui.thinking.circle);
      show_obj(ui.thinking.static_arc);
      show_obj(ui.thinking.spin_arc);
      start_thinking_anims();
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
      start_idle_anims();
      state_color = C_TEXT_GRAY;
      state_text = "Interrupted";
      hint_text = " ";
      break;

    case CHAT_VOICE_SELECT:
      /* Full-screen voice selection card.
       * set_state() calls hide_all_viz() first, so re-show panel here.
       * Panel children (buttons, labels, dots) are shown automatically. */
      show_obj(ui.voice_sel.panel);
      voice_sel_refresh_timbres();
      state_color = C_BLUE;
      state_text = "Voice";
      hint_text = " ";
      break;

    default:
      break;
  }

  lv_label_set_text(ui.state_label, state_text);
  lv_obj_set_style_text_color(ui.state_label, state_color, 0);
  lv_label_set_text(ui.hint_label, hint_text);

  ESP_LOGI(TAG, "page -> %s", state_text);
  lvgl_port_unlock();
}

void ai_chat_ui_set_network(bool online) {
  if (!lvgl_port_lock(pdMS_TO_TICKS(100))) {
    ESP_LOGE(TAG, "set_network: failed to acquire LVGL lock");
    return;
  }
  lv_label_set_text(ui.status_label, online ? "Online" : "Offline");
  lv_obj_set_style_bg_color(ui.status_dot, online ? C_GREEN : C_RED, 0);
  lvgl_port_unlock();
}

void ai_chat_ui_set_connection(const char *ssid, const char *ip,
                               bool online) {
  (void)ssid;
  (void)ip;
  ai_chat_ui_set_network(online);
}

void ai_chat_ui_update_volume(uint8_t level) {
  /* Decoupled from LVGL: the audio capture task (priority 5) calls this
   * every ~10ms. Storing the level + flag here avoids taking the LVGL lock
   * from the audio context, which raced with lvgl_task and triggered the
   * "failed to acquire LVGL lock" errors. The actual UI update is applied
   * later from ai_chat_ui_tick() on the low-priority main loop. */
  s_volume = level;
  s_vol_dirty = true;
}

/* Apply a pending volume change under the LVGL lock. Called from the main
 * loop (ai_chat_ui_tick), never from the high-priority audio task. */
static void ai_chat_ui_apply_volume(void) {
  if (!s_vol_dirty) return;
  if (!lvgl_port_lock(pdMS_TO_TICKS(50))) {
    return;  /* LVGL busy; retry on next tick */
  }
  s_vol_dirty = false;

  if (s_state != CHAT_LISTENING) {
    lvgl_port_unlock();
    return;
  }

  /* shape: small/medium/large/medium/small to mimic bar envelope */
  static const int factor[5] = { 30, 70, 100, 80, 40 };
  int base = 4 + (s_volume * 22 / 100);  /* 4..26 */
  if (base > 26) base = 26;

  for (int i = 0; i < 5; i++) {
    int h = base * factor[i] / 100;
    if (h < 4) h = 4;
    if (h > 28) h = 28;
    lv_anim_set_values(&ui.listening.anims_l[i], 4, h);
    lv_anim_set_values(&ui.listening.anims_r[i], 4, h);
  }
  lvgl_port_unlock();
}

void ai_chat_ui_add_message(const char *text, bool is_user) {
  (void)text;
  (void)is_user;
}

void ai_chat_ui_show_voice_selector(bool show) {
  if (!lvgl_port_lock(pdMS_TO_TICKS(100))) {
    ESP_LOGE(TAG, "show_voice_selector: failed to acquire LVGL lock");
    return;
  }
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
  } else {
    hide_obj(vs->panel);
  }
  lvgl_port_unlock();
}

void ai_chat_ui_voice_select_next(void) {
  if (!lvgl_port_lock(pdMS_TO_TICKS(100))) {
    ESP_LOGE(TAG, "voice_select_next: failed to acquire LVGL lock");
    return;
  }
  voice_select_viz_t *vs = &ui.voice_sel;

  /* cycle timbre within current gender */
  vs->timbre_idx = (vs->timbre_idx + 1) % vs->timbre_count;
  voice_sel_refresh_timbres();
  lvgl_port_unlock();
}

int ai_chat_ui_voice_select_get(void) {
  voice_select_viz_t *vs = &ui.voice_sel;
  voice_gender_t g = (voice_gender_t)vs->gender_idx;
  return voice_config_get_gender_voice_id(g, vs->timbre_idx);
}

/* ===================================================================
 *  Touch indicator — a small red dot at the last touch point.
 *  (Coordinate text debug was removed on user request.)
 *
 *  Called from lvgl_port touch_read_cb, which runs inside
 *  lvgl_task's LVGL lock; no extra lock needed here.
 * =================================================================== */
void ai_chat_ui_touch_indicator(int x, int y)
{
  if (!ui.touch_dot) {
    ui.touch_dot = lv_obj_create(lv_screen_active());
    lv_obj_set_size(ui.touch_dot, 16, 16);
    lv_obj_set_style_bg_color(ui.touch_dot, lv_color_hex(0xFF3030), 0);
    lv_obj_set_style_bg_opa(ui.touch_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ui.touch_dot, 8, 0);
    lv_obj_set_style_border_width(ui.touch_dot, 0, 0);
    lv_obj_set_style_pad_all(ui.touch_dot, 0, 0);
    lv_obj_clear_flag(ui.touch_dot,
                      LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(ui.touch_dot);
  }
  lv_obj_set_pos(ui.touch_dot, x - 8, y - 8);
  lv_obj_set_style_opa(ui.touch_dot, LV_OPA_COVER, 0);
}

void ai_chat_ui_touch_indicator_hide(void)
{
  if (ui.touch_dot) {
    lv_obj_set_style_opa(ui.touch_dot, LV_OPA_TRANSP, 0);
  }
}

/* ===================================================================
 *  Swipe detection for voice-select page.
 *
 *  Called from lvgl_port touch_read_cb with raw screen coordinates.
 *  Tracks press -> release; if horizontal travel exceeds threshold
 *  and vertical travel is small, fires prev/next.
 *  Only active when state == CHAT_VOICE_SELECT.
 * =================================================================== */
#define SWIPE_THRESHOLD  40   /* min horizontal pixels for a swipe */

void ai_chat_ui_touch_swipe(int x, int y, bool pressed)
{
  static bool  s_tracking = false;
  static int   s_start_x  = 0;
  static int   s_start_y  = 0;
  static int   s_last_x   = 0;   /* last position while pressing */
  static int   s_last_y   = 0;

  /* Only track when in voice-select mode */
  if (s_state != CHAT_VOICE_SELECT) {
    s_tracking = false;
    return;
  }

  if (pressed && !s_tracking) {
    /* Finger just touched down */
    s_tracking = true;
    s_start_x = x;
    s_start_y = y;
    s_last_x = x;
    s_last_y = y;
  } else if (pressed && s_tracking) {
    /* Finger still down - keep updating last position */
    s_last_x = x;
    s_last_y = y;
  } else if (!pressed && s_tracking) {
    /* Finger just lifted - check if it was a swipe.
     * Use s_last_x/y (the last valid touch position before release),
     * NOT the x/y args which are (0,0) when no touch is active. */
    s_tracking = false;
    int dx = s_last_x - s_start_x;
    int dy = s_last_y - s_start_y;

    /* Horizontal swipe: |dx| > threshold and |dx| > 2*|dy| */
    if (dx > SWIPE_THRESHOLD && (dx > 2 * (dy > 0 ? dy : -dy))) {
      /* right swipe -> previous */
      voice_select_viz_t *vs = &ui.voice_sel;
      vs->timbre_idx = (vs->timbre_idx - 1 + vs->timbre_count) % vs->timbre_count;
      ESP_LOGI(TAG, "swipe right -> prev %d (dx=%d)", vs->timbre_idx, dx);
      voice_sel_refresh_timbres();
    } else if (dx < -SWIPE_THRESHOLD &&
               (-dx > 2 * (dy > 0 ? dy : -dy))) {
      /* left swipe -> next */
      voice_select_viz_t *vs = &ui.voice_sel;
      vs->timbre_idx = (vs->timbre_idx + 1) % vs->timbre_count;
      ESP_LOGI(TAG, "swipe left -> next %d (dx=%d)", vs->timbre_idx, dx);
      voice_sel_refresh_timbres();
    }
  }
}