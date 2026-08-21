/**
 * @file ui/widgets/state_viz.c
 * @brief State visualizations for the chat UI, merged into one file.
 *
 * The main voice UI is four vertical capsules arranged horizontally. Every
 * chat state (idle, listening, thinking, speaking, disconnected) reuses the
 * same four LVGL objects and only changes their color plus which animation
 * runs, so the screen has one bounded visual group instead of five complete
 * widget groups.
 *
 * The voice selector remains a separate full-screen panel. It is shown over
 * the capsules and is mutually exclusive with them.
 */

#include "ai_chat_ui_internal.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "convai_bridge.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include <stdint.h>

#define CAPSULE_COUNT 4

static bool s_capsules_created = false;

static const int s_capsule_x[CAPSULE_COUNT] = {106, 142, 178, 214};

static lv_obj_t *new_plain_obj(void) {
  lv_obj_t *obj = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(obj);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  return obj;
}

static int capsule_index(lv_obj_t *capsule) {
  for (int i = 0; i < CAPSULE_COUNT; i++) {
    if (ui.capsules.capsules[i] == capsule) {
      return i;
    }
  }
  return 0;
}

static void capsule_style(lv_obj_t *capsule, int center_x, lv_color_t color,
                          lv_opa_t opa) {
  const int w = 24;
  const int h = 68;

  lv_obj_set_size(capsule, w, h);
  lv_obj_set_pos(capsule, center_x - w / 2, ORB_CY - h / 2);
  lv_obj_set_style_radius(capsule, w / 2, 0);
  lv_obj_set_style_bg_color(capsule, color, 0);
  lv_obj_set_style_bg_opa(capsule, opa, 0);
  lv_obj_set_style_border_width(capsule, 0, 0);
  lv_obj_set_style_border_opa(capsule, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_width(capsule, 0, 0);
  lv_obj_set_style_shadow_color(capsule, color, 0);
  lv_obj_set_style_shadow_opa(capsule, LV_OPA_TRANSP, 0);
}

static void show_capsules(lv_color_t color, lv_opa_t opa) {
  for (int i = 0; i < CAPSULE_COUNT; i++) {
    capsule_style(ui.capsules.capsules[i], s_capsule_x[i], color, opa);
    show_obj(ui.capsules.capsules[i]);
  }
}

static void hide_capsules(void) {
  for (int i = 0; i < CAPSULE_COUNT; i++) {
    hide_obj(ui.capsules.capsules[i]);
  }
}

void state_viz_hide_all(void) {
  hide_capsules();
  hide_obj(ui.voice_sel.panel);
}

static void capsules_ensure_created(void) {
  if (s_capsules_created) {
    return;
  }
  s_capsules_created = true;

  for (int i = 0; i < CAPSULE_COUNT; i++) {
    ui.capsules.capsules[i] = new_plain_obj();
  }

  state_viz_hide_all();
}

/* --------------------------- IDLE --------------------------- */
static void create_idle_viz(void) {
  capsules_ensure_created();
}

static void show_idle(void) {
  show_capsules(lv_color_hex(0xFFFFFF), LV_OPA_90);
}

static void start_idle_anims(void) {
  /* Idle is intentionally static: four evenly spaced white capsules. */
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

/* ------------------------- LISTENING ------------------------- */
static void create_listening_viz(void) {
  capsules_ensure_created();
}

static void show_listening(void) {
  show_capsules(C_BLUE, LV_OPA_90);
}

static void start_listening_anims(void) {
  for (int i = 0; i < CAPSULE_COUNT; i++) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ui.capsules.capsules[i]);
    lv_anim_set_exec_cb(&a, anim_capsule_sway_x_cb);
    lv_anim_set_values(&a, -6, 6);
    lv_anim_set_duration(&a, 420);
    lv_anim_set_playback_duration(&a, 420);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_delay(&a, i * 110);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
  }
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

/* ------------------------- THINKING ------------------------- */

void anim_capsule_sway_x_cb(void *var, int32_t v) {
  lv_obj_t *capsule = (lv_obj_t *)var;
  int idx = capsule_index(capsule);
  lv_obj_set_pos(capsule, s_capsule_x[idx] - 12 + v, ORB_CY - 34);
}

static void create_thinking_viz(void) {
  capsules_ensure_created();
}

static void show_thinking(void) {
  show_capsules(C_PURPLE, LV_OPA_90);
}

static void start_thinking_anims(void) {
  const int amplitudes[CAPSULE_COUNT] = {4, 5, 5, 4};

  for (int i = 0; i < CAPSULE_COUNT; i++) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ui.capsules.capsules[i]);
    lv_anim_set_exec_cb(&a, anim_capsule_sway_x_cb);
    lv_anim_set_values(&a, -amplitudes[i], amplitudes[i]);
    lv_anim_set_duration(&a, 600);
    lv_anim_set_playback_duration(&a, 600);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
  }
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

/* ------------------------- SPEAKING ------------------------- */
static void anim_capsule_bounce_y_cb(void *var, int32_t v) {
  lv_obj_t *capsule = (lv_obj_t *)var;
  int idx = capsule_index(capsule);
  lv_obj_set_pos(capsule, s_capsule_x[idx] - 12, ORB_CY - 34 + v);
}

static void create_speaking_viz(void) {
  capsules_ensure_created();
}

static void show_speaking(void) {
  show_capsules(C_PURPLE, LV_OPA_90);
}

static void start_speaking_anims(void) {
  const int amplitudes[CAPSULE_COUNT] = {16, 12, 12, 16};

  for (int i = 0; i < CAPSULE_COUNT; i++) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ui.capsules.capsules[i]);
    lv_anim_set_exec_cb(&a, anim_capsule_bounce_y_cb);
    lv_anim_set_values(&a, -amplitudes[i], amplitudes[i]);
    lv_anim_set_duration(&a, 460);
    lv_anim_set_playback_duration(&a, 460);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
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

/* ----------------------- DISCONNECTED ----------------------- */
static void create_disconnected_viz(void) {
  capsules_ensure_created();
}

static void show_disconnected(void) {
  show_capsules(C_RED, LV_OPA_80);
}

static const state_viz_t s_disconnected_viz = {
    .state = CHAT_DISCONNECTED,
    .name = "disconnected",
    .create = create_disconnected_viz,
    .show = show_disconnected,
    .start_anims = NULL,
};

void state_viz_disconnected_register(void) {
  state_viz_factory_register(&s_disconnected_viz);
}

/* ----------------------- VOICE SELECT ----------------------- */
static const char *TAG = "voice_sel";

#define VOICE_SEL_MAX_DOTS  6
#define GENDER_IDX_FEMALE   0
#define GENDER_IDX_MALE     1

static const char *const kGenderIcons[VOICE_GENDER_COUNT] = {"F", "M", "R"};

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
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_center(label);
  return btn;
}

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

  lv_label_set_text(vs->icon_text, kGenderIcons[vs->gender_idx]);
  lv_obj_set_style_bg_color(vs->icon_label,
                            (vs->gender_idx == GENDER_IDX_FEMALE) ? C_PURPLE
                            : (vs->gender_idx == GENDER_IDX_MALE) ? C_BLUE
                                                                  : C_GREEN,
                            0);

  lv_label_set_text(vs->name_label, entry->name);
  lv_label_set_text(vs->desc_label, entry->desc);
  lv_label_set_text(vs->tags_label, entry->tags);
  lv_label_set_text(vs->code_label, entry->code);

  int prev_idx = (vs->timbre_idx - 1 + vs->timbre_count) % vs->timbre_count;
  int next_idx = (vs->timbre_idx + 1) % vs->timbre_count;
  int prev_vid = voice_factory_gender_voice_id(gender, prev_idx);
  int next_vid = voice_factory_gender_voice_id(gender, next_idx);
  lv_label_set_text(vs->name_prev, voice_factory_get(prev_vid)->name);
  lv_label_set_text(vs->name_next, voice_factory_get(next_vid)->name);

  lv_obj_set_style_bg_opa(
      vs->btn_male,
      (vs->gender_idx == GENDER_IDX_MALE) ? LV_OPA_COVER : LV_OPA_20, 0);
  lv_obj_set_style_bg_opa(
      vs->btn_female,
      (vs->gender_idx == GENDER_IDX_FEMALE) ? LV_OPA_COVER : LV_OPA_20, 0);

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

static chat_state_t s_prev_state = CHAT_IDLE;

void voice_sel_save_prev_state(chat_state_t state) {
  s_prev_state = state;
}

void voice_sel_close(void) {
  ai_chat_ui_show_voice_selector(false);
  ai_chat_ui_set_state(s_prev_state);
}

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

/* Voice switching calls convai_update() (a network round-trip) plus an
 * NVS commit. Running that from the LVGL event callback would hold the
 * LVGL lock for seconds, blocking ai_chat_ui_set_state() from the SDK
 * thread and freezing the UI. So we close the selector immediately and
 * defer the blocking engine update to a short-lived worker task. */
static volatile bool s_voice_applying = false;

static void voice_apply_task(void *arg) {
  int voice_id = (int)(intptr_t)arg;
  if (voice_factory_select(convai_bridge_get_engine(), voice_id) != 0) {
    ESP_LOGE(TAG, "apply voice failed (id=%d)", voice_id);
  }
  s_voice_applying = false;
  vTaskDelete(NULL);
}

static void on_voice_sel_confirm(lv_event_t *e) {
  (void)e;
  if (s_voice_applying) {
    ESP_LOGW(TAG, "voice apply already in progress");
    return;
  }
  const voice_select_viz_t *vs = &ui.voice_sel;
  voice_gender_t gender = (voice_gender_t)vs->gender_idx;
  int voice_id = voice_factory_gender_voice_id(gender, vs->timbre_idx);

  s_voice_applying = true;
  voice_sel_close();
  /* 栈放 PSRAM: 内部 RAM 紧张时 xTaskCreate 偶发失败 */
  if (xTaskCreateWithCaps(voice_apply_task, "voice_apply", 6144,
                          (void *)(intptr_t)voice_id, 5, NULL,
                          MALLOC_CAP_SPIRAM) != pdPASS) {
    s_voice_applying = false;
    ESP_LOGE(TAG, "voice_apply task create failed (id=%d)", voice_id);
  }
}

static void on_voice_sel_back(lv_event_t *e) {
  (void)e;
  ESP_LOGI(TAG, "cancelled");
  voice_sel_close();
}

static void create_voice_select_viz(void) {
  voice_select_viz_t *vs = &ui.voice_sel;

  vs->panel = lv_obj_create(lv_screen_active());
  /* 椤堕儴鐣欏嚭 40px 缁欑姸鎬佹爮(鍚煶閲忔帶鍒?, 淇濊瘉鎵€鏈夌姸鎬佷笅闊抽噺閮藉彲璋?*/
  lv_obj_set_size(vs->panel, 320, 200);
  lv_obj_set_pos(vs->panel, 0, 40);
  lv_obj_set_style_bg_color(vs->panel, lv_color_hex(0x0F0F14), 0);
  lv_obj_set_style_bg_opa(vs->panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(vs->panel, 0, 0);
  lv_obj_set_style_radius(vs->panel, 0, 0);
  lv_obj_set_style_pad_all(vs->panel, 0, 0);
  lv_obj_clear_flag(vs->panel, LV_OBJ_FLAG_SCROLLABLE);

  vs->btn_back = create_flat_button(vs->panel, 50, 28, 6, 6,
                                    lv_color_hex(0x374151), LV_OPA_COVER, "<",
                                    on_voice_sel_back, NULL);
  vs->btn_save = create_flat_button(vs->panel, 50, 28, 264, 6, C_BLUE,
                                    LV_OPA_COVER, "OK", on_voice_sel_confirm,
                                    NULL);

  vs->title_label = lv_label_create(vs->panel);
  lv_label_set_text(vs->title_label, "Voice");
  lv_obj_set_style_text_color(vs->title_label, C_TEXT, 0);
  lv_obj_set_style_text_font(vs->title_label, &lv_font_montserrat_14, 0);
  lv_obj_align(vs->title_label, LV_ALIGN_TOP_MID, 0, 12);

  vs->btn_male = create_flat_button(vs->panel, 70, 28, 80, 42, C_BLUE,
                                    LV_OPA_20, "Male", on_gender_btn_click,
                                    (void *)(intptr_t)GENDER_IDX_MALE);
  vs->btn_female = create_flat_button(vs->panel, 70, 28, 170, 42, C_PURPLE,
                                      LV_OPA_20, "Female", on_gender_btn_click,
                                      (void *)(intptr_t)GENDER_IDX_FEMALE);

  vs->icon_label = lv_obj_create(vs->panel);
  lv_obj_set_size(vs->icon_label, 56, 56);
  lv_obj_align(vs->icon_label, LV_ALIGN_TOP_MID, 0, 70);
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
  lv_obj_set_style_text_font(vs->icon_text, &lv_font_montserrat_14, 0);
  lv_obj_center(vs->icon_text);

  vs->name_label =
      create_centered_label(vs->panel, 132, C_TEXT, &lv_font_custom_cjk_16);
  vs->desc_label = create_centered_label(vs->panel, 152, C_TEXT_GRAY,
                                         &lv_font_custom_cjk_14);
  vs->tags_label =
      create_centered_label(vs->panel, 170, C_BLUE, &lv_font_custom_cjk_14);

  vs->name_prev = lv_label_create(vs->panel);
  lv_label_set_text(vs->name_prev, "");
  lv_obj_set_style_text_color(vs->name_prev, C_TEXT_GRAY, 0);
  lv_obj_set_style_text_opa(vs->name_prev, LV_OPA_40, 0);
  lv_obj_set_style_text_font(vs->name_prev, &lv_font_custom_cjk_14, 0);
  lv_obj_set_style_text_align(vs->name_prev, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(vs->name_prev, 8, 136);

  vs->name_next = lv_label_create(vs->panel);
  lv_label_set_text(vs->name_next, "");
  lv_obj_set_style_text_color(vs->name_next, C_TEXT_GRAY, 0);
  lv_obj_set_style_text_opa(vs->name_next, LV_OPA_40, 0);
  lv_obj_set_style_text_font(vs->name_next, &lv_font_custom_cjk_14, 0);
  lv_obj_set_style_text_align(vs->name_next, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_pos(vs->name_next, 220, 136);

  for (int i = 0; i < VOICE_SEL_MAX_DOTS; i++) {
    vs->dots[i] = lv_obj_create(vs->panel);
    lv_obj_set_size(vs->dots[i], 8, 8);
    lv_obj_set_pos(vs->dots[i], 120 + i * 16, 184);
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
  lv_obj_set_pos(vs->code_label, 8, 186);

  vs->gender_idx = GENDER_IDX_FEMALE;
  vs->timbre_idx = 0;
  voice_sel_refresh_timbres();
}

static void show_voice_select(void) {
  show_obj(ui.voice_sel.panel);
  voice_sel_refresh_timbres();
}

static const state_viz_t s_voice_select_viz = {
    .state = CHAT_VOICE_SELECT,
    .name = "voice_select",
    .create = create_voice_select_viz,
    .show = show_voice_select,
    .start_anims = NULL,
};

void state_viz_voice_select_register(void) {
  state_viz_factory_register(&s_voice_select_viz);
}
