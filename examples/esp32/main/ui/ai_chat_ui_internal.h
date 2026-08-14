/**
 * @file ai_chat_ui_internal.h
 * @brief Shared model + helpers for the chat UI widgets.
 *
 * The chat UI is organized as a single model (@c ui) operated on by several
 * "view" modules (top bar, state visualizations, voice selector, float ball).
 * This header exposes the model and the helpers every widget needs, so the
 * widget files stay free of copy-pasted animation/show boilerplate.
 *
 * @note This header is internal to the chat UI module. Application code must
 *       use the public API in ai_chat_ui.h only.
 */

#ifndef AI_CHAT_UI_INTERNAL_H
#define AI_CHAT_UI_INTERNAL_H

#include "ai_chat_ui.h"
#include "state_viz.h"
#include "voice_factory.h"
#include "lvgl_port.h"
#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CJK fonts are generated into lv_font_custom_cjk_{14,16}.c. */
LV_FONT_DECLARE(lv_font_custom_cjk_14)
LV_FONT_DECLARE(lv_font_custom_cjk_16)

/* ---- Color palette (dark theme) ---- */
#define C_BG         lv_color_hex(0x000000)
#define C_TEXT       lv_color_hex(0xFFFFFF)
#define C_TEXT_GRAY  lv_color_hex(0x9CA3AF)
#define C_GREEN      lv_color_hex(0x00E676)
#define C_GREEN_DIM  lv_color_hex(0x007A3A)
#define C_BLUE       lv_color_hex(0x3B82F6)
#define C_BLUE_DIM   lv_color_hex(0x1E3A8A)
#define C_PURPLE     lv_color_hex(0xA855F7)
#define C_PURPLE_DIM lv_color_hex(0x581C87)
#define C_RED        lv_color_hex(0xFF5252)

#define ORB_CX  160
#define ORB_CY  110

/* ---- Per-state view sub-structures ---- */
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
  lv_obj_t *btn_back;
  lv_obj_t *title_label;
  lv_obj_t *btn_save;
  lv_obj_t *btn_male;
  lv_obj_t *btn_female;
  lv_obj_t *icon_label;
  lv_obj_t *icon_text;
  lv_obj_t *name_label;
  lv_obj_t *name_prev;
  lv_obj_t *name_next;
  lv_obj_t *desc_label;
  lv_obj_t *tags_label;
  lv_obj_t *dots[6];
  lv_obj_t *code_label;
  int       timbre_count;
  int       gender_idx;
  int       timbre_idx;
} voice_select_viz_t;

/** The single shared UI model. */
typedef struct {
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
  lv_obj_t *float_ball_lines[3];
  lv_obj_t *touch_dot;
} ui_t;

/** Shared model instance (defined in ai_chat_ui.c). */
extern ui_t ui;

/* ---- Shared helpers (defined in ai_chat_ui.c) ---- */
void show_obj(lv_obj_t *o);
void hide_obj(lv_obj_t *o);
void pos_centered(lv_obj_t *obj, lv_coord_t cx, lv_coord_t cy);
void anim_init_bar(lv_anim_t *a, lv_obj_t *bar, int min_h,
                   int max_h, uint32_t dur);

/* Animation callbacks (used by widget create/start functions). */
void anim_pulse_centered_cb(void *var, int32_t v);
void anim_pulse_opa_cb(void *var, int32_t v);
void anim_bar_h_cb(void *var, int32_t v);
void anim_arc_rotate_cb(void *var, int32_t v);

/* ---- Widget constructors / registrars (defined in widget files) ---- */
void create_top_bar(void);
void create_bottom_text(void);
void create_float_ball(void);
void on_screen_long_press(lv_event_t *e);

void state_viz_idle_register(void);
void state_viz_listening_register(void);
void state_viz_thinking_register(void);
void state_viz_speaking_register(void);
void state_viz_disconnected_register(void);
void state_viz_voice_select_register(void);

/* Voice-selector helpers (defined in widgets/state_viz_voice_select.c). */

/** Redraw the voice card for the current gender + timbre index. */
void voice_sel_refresh_timbres(void);

/** Close the voice selector without applying the selection. */
void voice_sel_close(void);

/** Register every state visualization (called once at init). */
void state_viz_register_all(void);

#ifdef __cplusplus
}
#endif

#endif /* AI_CHAT_UI_INTERNAL_H */
