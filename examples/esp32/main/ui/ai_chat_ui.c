/**
 * @file ai_chat_ui.c
 * @brief Chat UI orchestrator: model, shared helpers, public API.
 *
 * This file owns the shared @c ui model, the per-state animation/scaffold
 * helpers used by every widget, the state-visualization factory registry,
 * and the public API (init / set_state / network / volume / touch). The
 * actual LVGL geometry for each state lives in its widget file under
 * widgets/ and is reached through the factory.
 */

#include "ai_chat_ui_internal.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ai_chat_ui";

/* ---- Shared model instance ---- */
ui_t ui;

/* ---- Runtime state ---- */
static chat_state_t s_state = CHAT_IDLE;
static uint8_t s_volume = 0;
static volatile bool s_vol_dirty = false;

static void ai_chat_ui_apply_volume(void);

/* ===================================================================
 *  Animation callbacks (shared by widget create/start functions)
 * =================================================================== */

/** Animate size AND re-center (for orb-anchored objects). */
void anim_pulse_centered_cb(void *var, int32_t v) {
  lv_obj_t *obj = (lv_obj_t *)var;
  lv_obj_set_size(obj, v, v);
  lv_obj_set_pos(obj, ORB_CX - v / 2, ORB_CY - v / 2);
  lv_obj_set_style_radius(obj, v / 2, 0);
}

void anim_pulse_opa_cb(void *var, int32_t v) {
  lv_obj_set_style_opa((lv_obj_t *)var, v, 0);
}

void anim_bar_h_cb(void *var, int32_t v) {
  lv_obj_set_height((lv_obj_t *)var, v);
}

void anim_arc_rotate_cb(void *var, int32_t v) {
  lv_arc_set_rotation((lv_obj_t *)var, v);
}

void pos_centered(lv_obj_t *obj, lv_coord_t cx, lv_coord_t cy) {
  lv_obj_set_pos(obj, cx - lv_obj_get_width(obj) / 2,
                 cy - lv_obj_get_height(obj) / 2);
}

void show_obj(lv_obj_t *o) {
  if (o) {
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
}

void hide_obj(lv_obj_t *o) {
  if (o) {
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
}

void anim_init_bar(lv_anim_t *a, lv_obj_t *bar, int min_h,
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
 *  State-visualization factory
 * =================================================================== */
/* CHAT_DISCONNECTED is the last chat_state_t entry, so the table indexed by
 * state covers every state. States without a viz (e.g. CHAT_INTERRUPTED)
 * simply stay NULL and fall back in ai_chat_ui_set_state(). */
#define VIZ_TABLE_SIZE  (CHAT_DISCONNECTED + 1)

static const state_viz_t *s_viz_table[VIZ_TABLE_SIZE];

esp_err_t state_viz_factory_register(const state_viz_t *viz) {
  if (viz == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if ((int)viz->state < 0 || (int)viz->state >= VIZ_TABLE_SIZE) {
    ESP_LOGE(TAG, "register: invalid state %d", (int)viz->state);
    return ESP_ERR_INVALID_ARG;
  }
  s_viz_table[viz->state] = viz;
  ESP_LOGD(TAG, "viz registered: %s -> state %d", viz->name, (int)viz->state);
  return ESP_OK;
}

const state_viz_t *state_viz_factory_get(chat_state_t state) {
  if ((int)state < 0 || (int)state >= VIZ_TABLE_SIZE) {
    return NULL;
  }
  return s_viz_table[state];
}

void state_viz_register_all(void) {
  state_viz_idle_register();
  state_viz_listening_register();
  state_viz_thinking_register();
  state_viz_speaking_register();
  state_viz_disconnected_register();
  state_viz_voice_select_register();
}

/* ===================================================================
 *  Visibility helper
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
  for (int i = 0; i < 3; i++) {
    hide_obj(ui.speaking.wave_arcs[i]);
  }
  hide_obj(ui.thinking.circle);
  hide_obj(ui.thinking.spin_arc);
  hide_obj(ui.thinking.static_arc);
  hide_obj(ui.disconnected.circle);
  hide_obj(ui.disconnected.bar1);
  hide_obj(ui.disconnected.bar2);
  /* Voice-select panel: only hide the panel itself. All children are
   * created as panel children, so they are invisible while the panel is
   * hidden. We must NOT individually hide them, because set_state() only
   * re-shows the panel -- individually-hidden children would stay invisible. */
  hide_obj(ui.voice_sel.panel);
}

/* ===================================================================
 *  Bottom labels (state y=180, hint y=204)
 * =================================================================== */
void create_bottom_text(void) {
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
 *  Public API
 * =================================================================== */

esp_err_t ai_chat_ui_init(void) {
  ESP_LOGI(TAG, "Creating Voice Assistant UI");

  /* Must hold the lock: lvgl_task(CPU1) may be running lv_timer_handler;
   * without the lock we race LVGL's internal state across cores. */
  if (!lvgl_port_lock(pdMS_TO_TICKS(2000))) {
    ESP_LOGE(TAG, "init: failed to acquire LVGL lock");
    return ESP_FAIL;
  }

  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, C_BG, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  /* No scrollable content; prevents stray touch coords from being treated
   * as drag gestures that push the whole screen off (the previous bug). */
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  state_viz_register_all();

  create_top_bar();
  for (int s = 0; s < VIZ_TABLE_SIZE; s++) {
    const state_viz_t *viz = s_viz_table[s];
    if (viz != NULL && viz->create != NULL) {
      viz->create();
    }
  }
  create_bottom_text();
  create_float_ball();

  /* Long-press anywhere opens the voice selector (backup for the ball). */
  lv_obj_add_event_cb(scr, on_screen_long_press, LV_EVENT_LONG_PRESSED, NULL);

  /* Boot into IDLE. */
  hide_all_viz();
  const state_viz_t *idle = state_viz_factory_get(CHAT_IDLE);
  if (idle != NULL) {
    if (idle->show != NULL) {
      idle->show();
    }
    if (idle->start_anims != NULL) {
      idle->start_anims();
    }
  }
  s_state = CHAT_IDLE;

  lvgl_port_unlock();
  ESP_LOGI(TAG, "UI ready");
  return ESP_OK;
}

void ai_chat_ui_tick(void) {
  /* LVGL timers are driven by lvgl_task (CPU1). Here we only apply any
   * pending volume change queued by the audio-capture task. */
  ai_chat_ui_apply_volume();
}

chat_state_t ai_chat_ui_get_state(void) {
  return s_state;
}

void ai_chat_ui_set_state(chat_state_t state) {
  /* Debounce: ignore rapid duplicate state changes within 300ms. SDK
   * callbacks can fire IDLE->LISTENING->IDLE in quick succession, each
   * triggering hide_all_viz + show + anim restart, burning LVGL CPU. */
  static TickType_t s_last_change = 0;
  TickType_t now = xTaskGetTickCount();
  if (state == s_state && state != CHAT_INTERRUPTED) {
    return;
  }
  if (state == s_state &&
      (now - s_last_change) * portTICK_PERIOD_MS < 300) {
    return;
  }
  s_last_change = now;

  if (!lvgl_port_lock(pdMS_TO_TICKS(100))) {
    ESP_LOGE(TAG, "set_state: failed to acquire LVGL lock");
    return;
  }
  s_state = state;

  hide_all_viz();

  const state_viz_t *viz = state_viz_factory_get(state);
  if (viz == NULL && state == CHAT_INTERRUPTED) {
    viz = state_viz_factory_get(CHAT_IDLE); /* interrupted reuses idle */
  }
  if (viz) {
    if (viz->show) {
      viz->show();
    }
    if (viz->start_anims) {
      viz->start_anims();
    }
  }

  lv_color_t state_color = C_TEXT;
  const char *state_text = "";
  const char *hint_text = "";
  switch (state) {
    case CHAT_IDLE:
      state_color = C_GREEN; state_text = "Idle"; hint_text = "Tap to start";
      break;
    case CHAT_LISTENING:
      state_color = C_BLUE; state_text = "Listening"; hint_text = "Please wait";
      break;
    case CHAT_SPEAKING:
      state_color = C_PURPLE; state_text = "AI speaking"; hint_text = "Playing";
      break;
    case CHAT_THINKING:
      state_color = C_PURPLE; state_text = "Thinking";
      hint_text = "Generating...";
      break;
    case CHAT_DISCONNECTED:
      state_color = C_RED; state_text = "Disconnected";
      hint_text = "Check network";
      break;
    case CHAT_INTERRUPTED:
      state_color = C_TEXT_GRAY; state_text = "Interrupted"; hint_text = " ";
      break;
    case CHAT_VOICE_SELECT:
      /* The full-screen voice panel already carries its own "Voice" title;
       * leave the bottom state label empty so we don't draw a second
       * overlapping "Voice" on top of the panel. */
      state_color = C_BLUE; state_text = ""; hint_text = " ";
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

void ai_chat_ui_set_connection(const char *ssid, const char *ip, bool online) {
  (void)ssid;
  (void)ip;
  ai_chat_ui_set_network(online);
}

void ai_chat_ui_update_volume(uint8_t level) {
  /* Decoupled from LVGL: the audio-capture task (priority 5) calls this
   * every ~10ms. Storing the level + flag avoids taking the LVGL lock from
   * the audio context (which raced lvgl_task). The actual UI update is
   * applied later from ai_chat_ui_tick() on the low-priority main loop. */
  s_volume = level;
  s_vol_dirty = true;
}

/* Apply a pending volume change under the LVGL lock. Called from the main
 * loop (ai_chat_ui_tick), never from the high-priority audio task. */
static void ai_chat_ui_apply_volume(void) {
  if (!s_vol_dirty) {
    return;
  }
  if (!lvgl_port_lock(pdMS_TO_TICKS(50))) {
    return; /* LVGL busy; retry on next tick */
  }
  s_vol_dirty = false;

  if (s_state != CHAT_LISTENING) {
    lvgl_port_unlock();
    return;
  }

  /* shape: small/medium/large/medium/small to mimic a bar envelope */
  static const int factor[5] = {30, 70, 100, 80, 40};
  int base = 4 + (s_volume * 22 / 100); /* 4..26 */
  if (base > 26) {
    base = 26;
  }

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
    int vid = voice_factory_current_id();
    voice_gender_t g = voice_factory_get_gender(vid);
    int tcnt = voice_factory_gender_voice_count(g);
    vs->gender_idx = (int)g;
    vs->timbre_idx = 0;
    for (int i = 0; i < tcnt; i++) {
      if (voice_factory_gender_voice_id(g, i) == vid) {
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
  vs->timbre_idx = (vs->timbre_idx + 1) % vs->timbre_count;
  voice_sel_refresh_timbres();
  lvgl_port_unlock();
}

int ai_chat_ui_voice_select_get(void) {
  voice_select_viz_t *vs = &ui.voice_sel;
  voice_gender_t g = (voice_gender_t)vs->gender_idx;
  return voice_factory_gender_voice_id(g, vs->timbre_idx);
}

/* ===================================================================
 *  Touch indicator + swipe (called from lvgl_port touch_read_cb, which
 *  runs inside lvgl_task's LVGL lock; no extra lock needed here).
 * =================================================================== */
void ai_chat_ui_touch_indicator(int x, int y) {
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

void ai_chat_ui_touch_indicator_hide(void) {
  if (ui.touch_dot) {
    lv_obj_set_style_opa(ui.touch_dot, LV_OPA_TRANSP, 0);
  }
}

#define SWIPE_THRESHOLD  40   /* min horizontal pixels for a swipe */

void ai_chat_ui_touch_swipe(int x, int y, bool pressed) {
  static bool s_tracking = false;
  static int  s_start_x = 0;
  static int  s_start_y = 0;
  static int  s_last_x = 0;
  static int  s_last_y = 0;

  /* Only track when in voice-select mode. */
  if (s_state != CHAT_VOICE_SELECT) {
    s_tracking = false;
    return;
  }

  if (pressed && !s_tracking) {
    s_tracking = true;
    s_start_x = x;
    s_start_y = y;
    s_last_x = x;
    s_last_y = y;
  } else if (pressed && s_tracking) {
    s_last_x = x;
    s_last_y = y;
  } else if (!pressed && s_tracking) {
    s_tracking = false;
    int dx = s_last_x - s_start_x;
    int dy = s_last_y - s_start_y;

    /* Horizontal swipe: |dx| > threshold and |dx| > 2*|dy|. */
    if (dx > SWIPE_THRESHOLD && (dx > 2 * (dy > 0 ? dy : -dy))) {
      voice_select_viz_t *vs = &ui.voice_sel;
      vs->timbre_idx = (vs->timbre_idx - 1 + vs->timbre_count) %
                       vs->timbre_count;
      ESP_LOGI(TAG, "swipe right -> prev %d (dx=%d)", vs->timbre_idx, dx);
      voice_sel_refresh_timbres();
    } else if (dx < -SWIPE_THRESHOLD &&
               (-dx > 2 * (dy > 0 ? dy : -dy))) {
      voice_select_viz_t *vs = &ui.voice_sel;
      vs->timbre_idx = (vs->timbre_idx + 1) % vs->timbre_count;
      ESP_LOGI(TAG, "swipe left -> next %d (dx=%d)", vs->timbre_idx, dx);
      voice_sel_refresh_timbres();
    }
  }
}

/* ===================================================================
 *  Panel interface (used by the UI panel factory)
 * =================================================================== */
void ai_chat_ui_show(void) {
  /* The chat panel is the persistent base layer; nothing to show. */
}

void ai_chat_ui_hide(void) {
  /* The chat panel is the persistent base layer; nothing to hide. */
}
