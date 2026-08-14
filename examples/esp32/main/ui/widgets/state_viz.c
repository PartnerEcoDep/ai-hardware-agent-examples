/**
 * @file ui/widgets/state_viz.c
 * @brief State visualizations for the chat UI, merged into one file.
 *
 * Each chat state (idle, listening, thinking, speaking, disconnected,
 * voice select) provides create/show/start_anims through a state_viz_t;
 * ai_chat_ui.c owns the registry + dispatch table.
 */

#include "ai_chat_ui_internal.h"
#include "esp_log.h"


static void create_idle_viz(void) {
  /* Outer ring — border circle. */
  ui.idle.ring_outer = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(ui.idle.ring_outer);
  lv_obj_set_size(ui.idle.ring_outer, 110, 110);
  lv_obj_set_pos(ui.idle.ring_outer, ORB_CX - 55, ORB_CY - 55);
  lv_obj_set_style_radius(ui.idle.ring_outer, 55, 0);
  lv_obj_set_style_bg_opa(ui.idle.ring_outer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ui.idle.ring_outer, 2, 0);
  lv_obj_set_style_border_color(ui.idle.ring_outer, C_GREEN_DIM, 0);
  lv_obj_set_style_border_opa(ui.idle.ring_outer, LV_OPA_40, 0);

  /* Mid ring — border circle. */
  ui.idle.ring_mid = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(ui.idle.ring_mid);
  lv_obj_set_size(ui.idle.ring_mid, 86, 86);
  lv_obj_set_pos(ui.idle.ring_mid, ORB_CX - 43, ORB_CY - 43);
  lv_obj_set_style_radius(ui.idle.ring_mid, 43, 0);
  lv_obj_set_style_bg_opa(ui.idle.ring_mid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ui.idle.ring_mid, 2, 0);
  lv_obj_set_style_border_color(ui.idle.ring_mid, C_GREEN, 0);
  lv_obj_set_style_border_opa(ui.idle.ring_mid, LV_OPA_70, 0);

  /* Core — filled circle + shadow. */
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
}

static void show_idle(void) {
  show_obj(ui.idle.ring_outer);
  show_obj(ui.idle.ring_mid);
  show_obj(ui.idle.core);
}

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

static const state_viz_t s_idle_viz = {
    .state = CHAT_IDLE,
    .name = "idle",
    .create = create_idle_viz,
    .show = show_idle,
    .start_anims = start_idle_anims,
};

void state_viz_idle_register(void) {
  state_viz_factory_register(&s_idle_viz);
}

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

  ui.listening.icon_mic = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.listening.icon_mic, 14, 22);
  lv_obj_set_pos(ui.listening.icon_mic, ORB_CX - 7, ORB_CY - 24);
  lv_obj_set_style_bg_color(ui.listening.icon_mic, C_BLUE, 0);
  lv_obj_set_style_bg_opa(ui.listening.icon_mic, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(ui.listening.icon_mic, 7, 0);
  lv_obj_set_style_border_width(ui.listening.icon_mic, 0, 0);

  /* Mic stand (half-arc via lv_obj border; avoids lv_arc ESP32 bug). */
  ui.listening.icon_stand = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.listening.icon_stand, 24, 12);
  lv_obj_set_pos(ui.listening.icon_stand, ORB_CX - 12, ORB_CY + 2);
  lv_obj_set_style_bg_opa(ui.listening.icon_stand, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(ui.listening.icon_stand, C_BLUE, 0);
  lv_obj_set_style_border_width(ui.listening.icon_stand, 2, 0);
  lv_obj_set_style_border_opa(ui.listening.icon_stand, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(ui.listening.icon_stand, 24, 0);
  lv_obj_set_style_border_side(ui.listening.icon_stand,
                               LV_BORDER_SIDE_TOP, 0);

  ui.listening.icon_base = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.listening.icon_base, 18, 2);
  lv_obj_set_pos(ui.listening.icon_base, ORB_CX - 9, ORB_CY + 12);
  lv_obj_set_style_bg_color(ui.listening.icon_base, C_BLUE, 0);
  lv_obj_set_style_bg_opa(ui.listening.icon_base, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(ui.listening.icon_base, 1, 0);
  lv_obj_set_style_border_width(ui.listening.icon_base, 0, 0);

  static const int xs_l[5] = {
      ORB_CX - 75, ORB_CX - 67, ORB_CX - 59, ORB_CX - 51, ORB_CX - 43};
  static const int xs_r[5] = {
      ORB_CX + 40, ORB_CX + 48, ORB_CX + 56, ORB_CX + 64, ORB_CX + 72};
  static const int default_h[5] = {8, 16, 26, 18, 10};
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

static void show_listening(void) {
  show_obj(ui.listening.circle);
  show_obj(ui.listening.icon_mic);
  show_obj(ui.listening.icon_stand);
  show_obj(ui.listening.icon_base);
  for (int i = 0; i < 5; i++) {
    show_obj(ui.listening.bars_l[i]);
    show_obj(ui.listening.bars_r[i]);
  }
}

static void start_listening_anims(void) {
  static const int default_h[5] = {8, 16, 26, 18, 10};
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

static const state_viz_t s_listening_viz = {
    .state = CHAT_LISTENING,
    .name = "listening",
    .create = create_listening_viz,
    .show = show_listening,
    .start_anims = start_listening_anims,
};

void state_viz_listening_register(void) {
  state_viz_factory_register(&s_listening_viz);
}

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
}

static void show_thinking(void) {
  show_obj(ui.thinking.circle);
  show_obj(ui.thinking.static_arc);
  show_obj(ui.thinking.spin_arc);
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

static const state_viz_t s_thinking_viz = {
    .state = CHAT_THINKING,
    .name = "thinking",
    .create = create_thinking_viz,
    .show = show_thinking,
    .start_anims = start_thinking_anims,
};

void state_viz_thinking_register(void) {
  state_viz_factory_register(&s_thinking_viz);
}

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

  ui.speaking.icon_speaker = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.speaking.icon_speaker, 24, 16);
  lv_obj_set_pos(ui.speaking.icon_speaker, ORB_CX - 12, ORB_CY - 8);
  lv_obj_set_style_bg_color(ui.speaking.icon_speaker, C_PURPLE, 0);
  lv_obj_set_style_bg_opa(ui.speaking.icon_speaker, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(ui.speaking.icon_speaker, 3, 0);
  lv_obj_set_style_border_width(ui.speaking.icon_speaker, 0, 0);

  static const int wave_sizes[3] = {96, 110, 124};
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

static void show_speaking(void) {
  show_obj(ui.speaking.circle);
  show_obj(ui.speaking.icon_speaker);
  for (int i = 0; i < 3; i++) {
    show_obj(ui.speaking.wave_arcs[i]);
  }
}

static void start_speaking_anims(void) {
  static const int wave_sizes[3] = {96, 110, 124};
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

static const state_viz_t s_speaking_viz = {
    .state = CHAT_SPEAKING,
    .name = "speaking",
    .create = create_speaking_viz,
    .show = show_speaking,
    .start_anims = start_speaking_anims,
};

void state_viz_speaking_register(void) {
  state_viz_factory_register(&s_speaking_viz);
}

/** Bar rotation is expressed in 0.1 degree units (LVGL convention). */
#define BAR_ROT_45   450
#define BAR_ROT_135  1350

/** Configure one arm of the cross (shared by both bars). */
static void init_cross_bar(lv_obj_t *bar, int32_t rotation) {
  lv_obj_set_size(bar, 36, 4);
  pos_centered(bar, ORB_CX, ORB_CY);
  lv_obj_set_style_bg_color(bar, C_RED, 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(bar, 2, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_transform_pivot_x(bar, 18, 0);
  lv_obj_set_style_transform_pivot_y(bar, 2, 0);
  lv_obj_set_style_transform_rotation(bar, rotation, 0);
}

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
  init_cross_bar(ui.disconnected.bar1, BAR_ROT_45);

  ui.disconnected.bar2 = lv_obj_create(lv_screen_active());
  init_cross_bar(ui.disconnected.bar2, BAR_ROT_135);
}

static void show_disconnected(void) {
  show_obj(ui.disconnected.circle);
  show_obj(ui.disconnected.bar1);
  show_obj(ui.disconnected.bar2);
}

static const state_viz_t s_disconnected_viz = {
    .state = CHAT_DISCONNECTED,
    .name = "disconnected",
    .create = create_disconnected_viz,
    .show = show_disconnected,
    .start_anims = NULL, /* deliberately static */
};

void state_viz_disconnected_register(void) {
  state_viz_factory_register(&s_disconnected_viz);
}

static const char *TAG = "voice_sel";

/** Max page dots drawn at the bottom; extra timbres reuse the same dots. */
#define VOICE_SEL_MAX_DOTS  6

/** Gender button user-data, matching voice_gender_t. */
#define GENDER_IDX_FEMALE   0
#define GENDER_IDX_MALE     1

/** One-letter badge per gender, indexed by voice_gender_t. */
static const char *const kGenderIcons[VOICE_GENDER_COUNT] = {"F", "M", "R"};

/**
 * @brief Create a flat rounded button with a centered montserrat label.
 *
 * All four buttons on this panel share the same visual recipe; only size,
 * position, color, opacity and callback differ.
 *
 * @param parent    Parent object (the panel).
 * @param w,h       Button size in pixels.
 * @param x,y       Button position relative to @p parent.
 * @param bg        Background color.
 * @param bg_opa    Background opacity.
 * @param text      Label text (ASCII, montserrat 16).
 * @param cb        Click handler.
 * @param user_data Value passed to @p cb via lv_event_get_user_data().
 * @return The created button object.
 */
static lv_obj_t *create_flat_button(lv_obj_t *parent, int w, int h, int x,
                                    int y, lv_color_t bg, lv_opa_t bg_opa,
                                    const char *text, lv_event_cb_t cb,
                                    void *user_data) {
  lv_obj_t *btn = lv_obj_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_style_bg_color(btn, bg, 0);
  lv_obj_set_style_bg_opa(btn, bg_opa, 0);
  lv_obj_set_style_radius(btn, 6, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, C_TEXT, 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
  lv_obj_center(label);
  return btn;
}

/**
 * @brief Create a centered text label on the panel.
 *
 * Uses a fixed full-width row + centered-text alignment (the same recipe as
 * the status/hint labels) instead of a one-shot lv_obj_align, so the label
 * stays exactly at @p y even after lv_label_set_text() re-sizes it.
 *
 * @param parent Parent object (the panel).
 * @param y      Offset from the top of @p parent.
 * @param color  Text color.
 * @param font   Text font.
 * @return The created label (initially empty).
 */
static lv_obj_t *create_centered_label(lv_obj_t *parent, int y,
                                       lv_color_t color,
                                       const lv_font_t *font) {
  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_size(label, 320, 24);
  lv_obj_set_pos(label, 0, y);
  lv_label_set_text(label, "");
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  return label;
}

void voice_sel_refresh_timbres(void) {
  voice_select_viz_t *vs = &ui.voice_sel;
  voice_gender_t gender = (voice_gender_t)vs->gender_idx;

  vs->timbre_count = voice_factory_gender_voice_count(gender);
  if (vs->timbre_count < 1) {
    vs->timbre_count = 1;
  }
  if (vs->timbre_idx >= vs->timbre_count) {
    vs->timbre_idx = 0;
  }

  int vid = voice_factory_gender_voice_id(gender, vs->timbre_idx);
  const voice_entry_t *entry = voice_factory_get(vid);

  /* Gender badge. */
  lv_label_set_text(vs->icon_text, kGenderIcons[vs->gender_idx]);
  lv_obj_set_style_bg_color(vs->icon_label,
                            (vs->gender_idx == GENDER_IDX_FEMALE) ? C_PURPLE
                            : (vs->gender_idx == GENDER_IDX_MALE) ? C_BLUE
                                                                  : C_GREEN,
                            0);

  /* Current voice card. */
  lv_label_set_text(vs->name_label, entry->name);
  lv_label_set_text(vs->desc_label, entry->desc);
  lv_label_set_text(vs->tags_label, entry->tags);
  lv_label_set_text(vs->code_label, entry->code);

  /* Dimmed neighbours, so a swipe hints at what comes next. */
  int prev_idx = (vs->timbre_idx - 1 + vs->timbre_count) % vs->timbre_count;
  int next_idx = (vs->timbre_idx + 1) % vs->timbre_count;
  int prev_vid = voice_factory_gender_voice_id(gender, prev_idx);
  int next_vid = voice_factory_gender_voice_id(gender, next_idx);
  lv_label_set_text(vs->name_prev, voice_factory_get(prev_vid)->name);
  lv_label_set_text(vs->name_next, voice_factory_get(next_vid)->name);

  /* Gender toggle highlight. */
  lv_obj_set_style_bg_opa(
      vs->btn_male,
      (vs->gender_idx == GENDER_IDX_MALE) ? LV_OPA_COVER : LV_OPA_20, 0);
  lv_obj_set_style_bg_opa(
      vs->btn_female,
      (vs->gender_idx == GENDER_IDX_FEMALE) ? LV_OPA_COVER : LV_OPA_20, 0);

  /* Page dots. */
  for (int i = 0; i < VOICE_SEL_MAX_DOTS; i++) {
    if (i < vs->timbre_count) {
      show_obj(vs->dots[i]);
      lv_obj_set_style_bg_color(
          vs->dots[i],
          (i == vs->timbre_idx) ? C_BLUE : lv_color_hex(0x4B5563), 0);
    } else {
      hide_obj(vs->dots[i]);
    }
  }
}

void voice_sel_close(void) {
  ai_chat_ui_show_voice_selector(false);
  ai_chat_ui_set_state(CHAT_IDLE);
}

/** Gender toggle: switch list and reset to its first timbre. */
static void on_gender_btn_click(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  if (ui.voice_sel.gender_idx == idx) {
    return;
  }
  ui.voice_sel.gender_idx = idx;
  ui.voice_sel.timbre_idx = 0;
  ESP_LOGI(TAG, "gender -> %d (%s)", idx,
           voice_factory_gender_name((voice_gender_t)idx));
  voice_sel_refresh_timbres();
}

/** "OK": persist the highlighted timbre and return to the chat screen. */
static void on_voice_sel_confirm(lv_event_t *e) {
  (void)e;
  const voice_select_viz_t *vs = &ui.voice_sel;
  voice_gender_t gender = (voice_gender_t)vs->gender_idx;
  int voice_id = voice_factory_gender_voice_id(gender, vs->timbre_idx);
  const char *name = voice_factory_gender_voice_name(gender, vs->timbre_idx);

  ESP_LOGI(TAG, "confirm: id=%d name=%s", voice_id, name);
  voice_factory_select(NULL, voice_id);
  voice_sel_close();
}

/** "<": discard the change and return to the chat screen. */
static void on_voice_sel_back(lv_event_t *e) {
  (void)e;
  ESP_LOGI(TAG, "cancelled");
  voice_sel_close();
}

static void create_voice_select_viz(void) {
  voice_select_viz_t *vs = &ui.voice_sel;

  /* Full-screen opaque panel; every widget below is one of its children. */
  vs->panel = lv_obj_create(lv_screen_active());
  lv_obj_set_size(vs->panel, 320, 240);
  lv_obj_set_pos(vs->panel, 0, 0);
  lv_obj_set_style_bg_color(vs->panel, lv_color_hex(0x0F0F14), 0);
  lv_obj_set_style_bg_opa(vs->panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(vs->panel, 0, 0);
  lv_obj_set_style_radius(vs->panel, 0, 0);
  lv_obj_set_style_pad_all(vs->panel, 0, 0);
  lv_obj_clear_flag(vs->panel, LV_OBJ_FLAG_SCROLLABLE);

  /* Top row: back | title | save. */
  vs->btn_back = create_flat_button(vs->panel, 50, 28, 6, 6,
                                    lv_color_hex(0x374151), LV_OPA_COVER, "<",
                                    on_voice_sel_back, NULL);
  vs->btn_save = create_flat_button(vs->panel, 50, 28, 264, 6, C_BLUE,
                                    LV_OPA_COVER, "OK", on_voice_sel_confirm,
                                    NULL);

  vs->title_label = lv_label_create(vs->panel);
  lv_label_set_text(vs->title_label, "Voice");
  lv_obj_set_style_text_color(vs->title_label, C_TEXT, 0);
  lv_obj_set_style_text_font(vs->title_label, &lv_font_montserrat_16, 0);
  lv_obj_align(vs->title_label, LV_ALIGN_TOP_MID, 0, 12);

  /* Gender toggle. */
  vs->btn_male = create_flat_button(vs->panel, 70, 28, 80, 44, C_BLUE,
                                    LV_OPA_20, "Male", on_gender_btn_click,
                                    (void *)(intptr_t)GENDER_IDX_MALE);
  vs->btn_female = create_flat_button(vs->panel, 70, 28, 170, 44, C_PURPLE,
                                      LV_OPA_20, "Female", on_gender_btn_click,
                                      (void *)(intptr_t)GENDER_IDX_FEMALE);

  /* Center badge: colored circle carrying the gender letter. */
  vs->icon_label = lv_obj_create(vs->panel);
  lv_obj_set_size(vs->icon_label, 56, 56);
  lv_obj_align(vs->icon_label, LV_ALIGN_TOP_MID, 0, 82);
  lv_obj_set_style_bg_color(vs->icon_label, C_BLUE, 0);
  lv_obj_set_style_bg_opa(vs->icon_label, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(vs->icon_label, 28, 0);
  lv_obj_set_style_border_width(vs->icon_label, 0, 0);
  lv_obj_set_style_pad_all(vs->icon_label, 0, 0);
  lv_obj_clear_flag(vs->icon_label,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  vs->icon_text = lv_label_create(vs->icon_label);
  lv_label_set_text(vs->icon_text, "M");
  lv_obj_set_style_text_color(vs->icon_text, C_TEXT, 0);
  lv_obj_set_style_text_font(vs->icon_text, &lv_font_montserrat_16, 0);
  lv_obj_center(vs->icon_text);

  /* Voice name / description / tags use the CJK font (Chinese content). */
  vs->name_label =
      create_centered_label(vs->panel, 146, C_TEXT, &lv_font_custom_cjk_16);
  vs->desc_label = create_centered_label(vs->panel, 168, C_TEXT_GRAY,
                                         &lv_font_custom_cjk_14);
  vs->tags_label =
      create_centered_label(vs->panel, 188, C_BLUE, &lv_font_custom_cjk_14);

  /* Dimmed neighbours on both sides of the name. */
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

  /* Bottom: page dots + voice code. */
  for (int i = 0; i < VOICE_SEL_MAX_DOTS; i++) {
    vs->dots[i] = lv_obj_create(vs->panel);
    lv_obj_set_size(vs->dots[i], 8, 8);
    lv_obj_set_pos(vs->dots[i], 120 + i * 16, 210);
    lv_obj_set_style_bg_color(vs->dots[i], lv_color_hex(0x4B5563), 0);
    lv_obj_set_style_bg_opa(vs->dots[i], LV_OPA_COVER, 0);
    lv_obj_set_style_radius(vs->dots[i], 4, 0);
    lv_obj_set_style_border_width(vs->dots[i], 0, 0);
    lv_obj_clear_flag(vs->dots[i],
                      LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  }

  vs->code_label = lv_label_create(vs->panel);
  lv_label_set_text(vs->code_label, "");
  lv_obj_set_style_text_color(vs->code_label, C_TEXT_GRAY, 0);
  lv_obj_set_style_text_font(vs->code_label, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(vs->code_label, 8, 212);

  vs->gender_idx = GENDER_IDX_FEMALE;
  vs->timbre_idx = 0;
  voice_sel_refresh_timbres();
}

/**
 * @brief Reveal the picker.
 *
 * Only the panel is shown: its children were never hidden individually, so
 * they reappear with it (see hide_all_viz() in ai_chat_ui.c).
 */
static void show_voice_select(void) {
  show_obj(ui.voice_sel.panel);
  voice_sel_refresh_timbres();
}

static const state_viz_t s_voice_select_viz = {
    .state = CHAT_VOICE_SELECT,
    .name = "voice_select",
    .create = create_voice_select_viz,
    .show = show_voice_select,
    .start_anims = NULL, /* static card, no animation */
};

void state_viz_voice_select_register(void) {
  state_viz_factory_register(&s_voice_select_viz);
}
