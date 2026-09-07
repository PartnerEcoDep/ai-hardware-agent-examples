/**
 * @file ai_chat_ui.c
 * @brief Chat UI orchestrator: model, shared helpers, public API.
 *
 * This file owns the shared UI model, the per-state animation/scaffold
 * helpers, the state-visualization factory registry, and the public API.
 * The actual LVGL geometry for each state lives in widgets/state_viz.c.
 */

#include "ai_chat_ui_internal.h"
#include "audio_init.h"
#include "convai_bridge.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const char *TAG = "ai_chat_ui";

/* ---- Shared model instance ---- */
ui_t ui;

/* ---- Runtime state ---- */
static chat_state_t s_state = CHAT_IDLE;
static uint8_t s_volume = 0;
static volatile bool s_vol_dirty = false;

/* 锁竞争时的状态兜底: SDK 线程 set_state 抢锁失败时记录到此 (最新状态
 * 覆盖旧值), 由 lvgl_task 持锁时 ai_chat_ui_apply_pending() 应用。
 * 跨线程访问, 用 volatile。 */
static volatile chat_state_t s_pending_state = CHAT_IDLE;
static volatile bool s_pending_valid = false;

/* 云连接/网络状态同样走 pending (短抢锁失败时由 lvgl_task 持锁应用),
 * 避免 set_cloud(true) 在 lvgl_task 卡顿窗口内被静默丢弃, 导致
 * 已连接却显示"未连接"。 */
static volatile bool s_pending_cloud_valid = false;
static volatile bool s_pending_cloud_connected = false;
static volatile bool s_pending_net_valid = false;
static volatile bool s_pending_net_online = false;

static void ai_chat_ui_apply_volume(void);
static void ai_chat_ui_apply_state_locked(chat_state_t state);

/* ===================================================================
 *  Animation callbacks (shared by widget create/start functions)
 * =================================================================== */

/** Animate size and keep the object centered on the voice orb anchor. */
void anim_pulse_centered_cb(void *var, int32_t v) {
  lv_obj_t *obj = (lv_obj_t *)var;
  if (v & 1) {
    v--;
  }
  lv_obj_set_size(obj, v, v);
  lv_obj_set_pos(obj, ORB_CX - v / 2, ORB_CY - v / 2);
  lv_obj_set_style_radius(obj, v / 2, 0);
}

void anim_pulse_opa_cb(void *var, int32_t v) {
  lv_obj_set_style_opa((lv_obj_t *)var, v, 0);
}

void anim_bar_h_centered_cb(void *var, int32_t v) {
  lv_obj_t *bar = (lv_obj_t *)var;
  lv_obj_set_height(bar, v);
  lv_obj_set_y(bar, ORB_CY - v / 2);
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
  lv_anim_set_exec_cb(a, anim_bar_h_centered_cb);
  lv_anim_set_values(a, min_h, max_h);
  lv_anim_set_duration(a, dur);
  lv_anim_set_playback_duration(a, dur);
  lv_anim_set_repeat_count(a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(a, lv_anim_path_ease_in_out);
}

/* ===================================================================
 *  State-visualization factory
 * =================================================================== */
/* CHAT_DISCONNECTED is the last chat_state_t entry, so this table covers
 * every state. States without a viz (CHAT_INTERRUPTED) stay NULL and fall
 * back in ai_chat_ui_set_state(). */
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
  /* Stop every running animation before hiding the shared orb objects.
   * Hidden breathing/wave bars would otherwise keep mutating every few ms
   * and flood LVGL's invalid-area queue, making each frame very expensive. */
  lv_anim_delete_all();
  state_viz_hide_all();
}

/* ===================================================================
 *  Bottom labels (state y=180, hint y=204)
 * =================================================================== */
void create_bottom_text(void) {
  ui.state_label = lv_label_create(lv_screen_active());
  lv_obj_set_size(ui.state_label, 320, 20);
  lv_obj_set_pos(ui.state_label, 0, 180);
  lv_label_set_text(ui.state_label, "待机");
  lv_obj_set_style_text_color(ui.state_label, C_TEXT, 0);
  lv_obj_set_style_text_font(ui.state_label, &lv_font_custom_cjk_14, 0);
  lv_obj_set_style_text_align(ui.state_label, LV_TEXT_ALIGN_CENTER, 0);

  ui.hint_label = lv_label_create(lv_screen_active());
  lv_obj_set_size(ui.hint_label, 320, 20);
  lv_obj_set_pos(ui.hint_label, 0, 204);
  lv_label_set_text(ui.hint_label, "点击按钮说话");
  lv_obj_set_style_text_color(ui.hint_label, C_TEXT_GRAY, 0);
  lv_obj_set_style_text_font(ui.hint_label, &lv_font_custom_cjk_14, 0);
  lv_obj_set_style_text_align(ui.hint_label, LV_TEXT_ALIGN_CENTER, 0);
}

/* ===================================================================
 *  对话启停按钮 (laiwfs300 无物理按键, 用触摸屏按钮替代 BOOT 键)
 *  位置: 右下角浮球左侧同排, 避开中央状态/提示文字
 * =================================================================== */
#define TALK_BTN_SIZE  44
#define TALK_BTN_X     212   /* 浮球(266,186)左侧, 间距 10px */
#define TALK_BTN_Y     186

static void on_talk_button_click(lv_event_t *e) {
  (void)e;
  if (convai_bridge_is_started()) {
    ESP_LOGI(TAG, "talk button -> convai_bridge_stop");
    convai_bridge_stop();
  } else {
    ESP_LOGI(TAG, "talk button -> convai_bridge_start");
    if (convai_bridge_start() != 0) {
      ESP_LOGE(TAG, "convai_bridge_start failed");
    }
  }
  if (ui.talk_button_label != NULL) {
    lv_label_set_text(ui.talk_button_label,
                      convai_bridge_is_started() ? "停止" : "说话");
  }
}

void create_talk_button(void) {
  ui.talk_button = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ui.talk_button, TALK_BTN_SIZE, TALK_BTN_SIZE);
  lv_obj_set_pos(ui.talk_button, TALK_BTN_X, TALK_BTN_Y);
  lv_obj_set_style_bg_color(ui.talk_button, C_BLUE, 0);
  lv_obj_set_style_bg_opa(ui.talk_button, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ui.talk_button, 1, 0);
  lv_obj_set_style_border_color(ui.talk_button, C_BLUE_DIM, 0);
  lv_obj_set_style_radius(ui.talk_button, TALK_BTN_SIZE / 2, 0);
  lv_obj_set_style_pad_all(ui.talk_button, 0, 0);
  lv_obj_add_flag(ui.talk_button, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ui.talk_button, on_talk_button_click,
                      LV_EVENT_CLICKED, NULL);

  ui.talk_button_label = lv_label_create(ui.talk_button);
  lv_label_set_text(ui.talk_button_label, "说话");
  lv_obj_set_style_text_color(ui.talk_button_label, C_TEXT, 0);
  lv_obj_set_style_text_font(ui.talk_button_label, &lv_font_custom_cjk_14, 0);
  lv_obj_set_style_text_align(ui.talk_button_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(ui.talk_button_label);
}

/* ===================================================================
 *  Public API
 * =================================================================== */

esp_err_t ai_chat_ui_init(void) {
  ESP_LOGI(TAG, "Creating Voice Assistant UI");

  /* Hold the LVGL lock because lvgl_task (CPU1) may be running
   * lv_timer_handler concurrently. */
  if (!lvgl_port_lock(pdMS_TO_TICKS(2000))) {
    ESP_LOGE(TAG, "init: failed to acquire LVGL lock");
    return ESP_FAIL;
  }

  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, C_BG, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
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
  create_talk_button();

  lv_obj_add_event_cb(scr, on_screen_long_press, LV_EVENT_LONG_PRESSED, NULL);

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

  convai_bridge_on_event(ai_chat_ui_on_cloud_event);

  ESP_LOGI(TAG, "UI ready");
  return ESP_OK;
}

/* 右上角状态统计: 每 ~1s 刷新一次 RAM 空闲堆与上行丢包率。
 * main 主循环以 50ms 间隔调用 ai_chat_ui_tick(), 累计 20 次即 1s。 */
#define STATS_REFRESH_TICKS 20

static void ai_chat_ui_update_stats(void) {
  if (!lvgl_port_lock(pdMS_TO_TICKS(100))) {
    return;
  }

  unsigned int sent = 0;
  unsigned int dropped = 0;
  int have_stats = (convai_bridge_get_uplink_stats(&sent, &dropped) == 0);

  if (ui.ram_label != NULL) {
    uint32_t total = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t used = (total > free_heap) ? (total - free_heap) : 0;
    lv_label_set_text_fmt(ui.ram_label, "内存 %uKB",
                          (unsigned int)(used / 1024));
  }
  if (ui.loss_label != NULL) {
    if (have_stats && (sent + dropped) > 0) {
      unsigned int pct = (unsigned int)((uint64_t)dropped * 100 /
                                        (sent + dropped));
      lv_label_set_text_fmt(ui.loss_label, "丢包 %u%%", pct);
    } else {
      lv_label_set_text(ui.loss_label, "丢包 -");
    }
  }

  lvgl_port_unlock();
}

void ai_chat_ui_tick(void) {
  /* 低频兜底 (每 500ms 最多一次): 正常情况下 pending 由 lvgl_task 持锁时
   * ai_chat_ui_apply_pending() 应用; 此处仅在 lvgl_task 被饿死时补位,
   * 避免 UI 状态永久丢失. 不再每 tick 重试, 防止主循环被锁拖死。 */
  static TickType_t s_last_pending_retry = 0;
  if (s_pending_valid) {
    TickType_t now = xTaskGetTickCount();
    if ((now - s_last_pending_retry) * portTICK_PERIOD_MS >= 500) {
      s_last_pending_retry = now;
      ai_chat_ui_set_state(s_pending_state);
    }
  }
  ai_chat_ui_apply_volume();
  static uint32_t s_stats_cnt = 0;
  if (++s_stats_cnt >= STATS_REFRESH_TICKS) {
    s_stats_cnt = 0;
    ai_chat_ui_update_stats();
  }
}

chat_state_t ai_chat_ui_get_state(void) {
  return s_state;
}

void ai_chat_ui_set_state(chat_state_t state) {
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

  /* 非阻塞: 最新状态写入 pending (单槽, 后写覆盖先写), 再尝试短抢锁。
   * 抢到锁立即应用; 抢不到则留给 lvgl_task 持锁时 ai_chat_ui_apply_pending()
   * 应用 — 不再阻塞 SDK 线程/主循环, 从根上消除锁竞争导致的 UI 卡死。 */
  s_pending_state = state;
  s_pending_valid = true;

  if (lvgl_port_lock(pdMS_TO_TICKS(30))) {
    ai_chat_ui_apply_state_locked(state);
    lvgl_port_unlock();
  }
}

/* 调用方必须已持有 LVGL 锁 (lvgl_task 循环内)。应用最新 pending 状态。 */
void ai_chat_ui_apply_pending(void) {
  if (s_pending_valid) {
    chat_state_t st = s_pending_state;
    ai_chat_ui_apply_state_locked(st);
  }
  if (s_pending_net_valid) {
    bool on = s_pending_net_online;
    s_pending_net_valid = false;
    lv_label_set_text(ui.status_label, on ? "已连接" : "未连接");
    lv_obj_set_style_bg_color(ui.status_dot, on ? C_GREEN : C_RED, 0);
  }
  if (s_pending_cloud_valid) {
    bool c = s_pending_cloud_connected;
    s_pending_cloud_valid = false;
    lv_label_set_text(ui.status_label, c ? "已连接" : "未连接");
    lv_obj_set_style_bg_color(ui.status_dot,
                              c ? C_GREEN : C_TEXT_GRAY, 0);
  }
}

/* ---- 持锁应用状态 (调用方必须已持有 LVGL 锁) ---- */
static void ai_chat_ui_apply_state_locked(chat_state_t state) {
  s_pending_valid = false;
  s_state = state;

  hide_all_viz();

  const state_viz_t *viz = state_viz_factory_get(state);
  if (viz == NULL && state == CHAT_INTERRUPTED) {
    viz = state_viz_factory_get(CHAT_IDLE);
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
      state_color = C_GREEN;
      state_text = "待机";
      hint_text = "点击开始";
      break;
    case CHAT_LISTENING:
      state_color = C_BLUE;
      state_text = "聆听中";
      hint_text = "请说话";
      break;
    case CHAT_SPEAKING:
      state_color = C_PURPLE;
      state_text = "AI 播报";
      hint_text = "播放中";
      break;
    case CHAT_THINKING:
      state_color = C_PURPLE;
      state_text = "思考中";
      hint_text = "生成中...";
      break;
    case CHAT_DISCONNECTED:
      state_color = C_RED;
      state_text = "未连接";
      hint_text = "请检查网络";
      break;
    case CHAT_INTERRUPTED:
      state_color = C_TEXT_GRAY;
      state_text = "已打断";
      hint_text = " ";
      break;
    case CHAT_VOICE_SELECT:
      state_color = C_BLUE;
      state_text = "";
      hint_text = " ";
      break;
    default:
      break;
  }

  lv_label_set_text(ui.state_label, state_text);
  lv_obj_set_style_text_color(ui.state_label, state_color, 0);
  lv_label_set_text(ui.hint_label, hint_text);

  ESP_LOGI(TAG, "page -> %s", state_text);
}

void ai_chat_ui_set_network(bool online) {
  /* 非阻塞: 记录 pending (lvgl_task 持锁时兜底应用), 再短抢锁立即应用. */
  s_pending_net_online = online;
  s_pending_net_valid = true;
  if (!lvgl_port_lock(pdMS_TO_TICKS(30))) {
    return;
  }
  s_pending_net_valid = false;
  lv_label_set_text(ui.status_label, online ? "已连接" : "未连接");
  lv_obj_set_style_bg_color(ui.status_dot, online ? C_GREEN : C_RED, 0);
  lvgl_port_unlock();
}

void ai_chat_ui_set_connection(const char *ssid, const char *ip, bool online) {
  (void)ssid;
  (void)ip;
  ai_chat_ui_set_network(online);
}

void ai_chat_ui_on_cloud_event(convai_event_code_e code, const char *info) {
  (void)info;
  switch (code) {
    case CONVAI_EV_CONNECTED:
      ai_chat_ui_set_cloud(true);
      break;
    case CONVAI_EV_DISCONNECTED:
    case CONVAI_EV_FAILED:
      ai_chat_ui_set_cloud(false);
      ai_chat_ui_set_state(STATE_IDLE);
      break;
    case CONVAI_EV_UPDATED:
    default:
      break;
  }
}

void ai_chat_ui_set_cloud(bool connected) {
  /* 非阻塞: 记录 pending (lvgl_task 持锁时兜底应用), 再短抢锁立即应用.
   * 避免在 lvgl_task 卡顿窗口内抢锁失败导致"已连接"状态永久丢失. */
  s_pending_cloud_connected = connected;
  s_pending_cloud_valid = true;
  if (!lvgl_port_lock(pdMS_TO_TICKS(30))) {
    return;
  }
  s_pending_cloud_valid = false;
  lv_label_set_text(ui.status_label, connected ? "已连接" : "未连接");
  lv_obj_set_style_bg_color(ui.status_dot,
                            connected ? C_GREEN : C_TEXT_GRAY, 0);
  lvgl_port_unlock();
}

void ai_chat_ui_update_volume(uint8_t level) {
  s_volume = level;
  s_vol_dirty = true;
}

static void ai_chat_ui_apply_volume(void) {
  if (!s_vol_dirty) {
    return;
  }
  if (!lvgl_port_lock(pdMS_TO_TICKS(200))) {
    return;
  }
  s_vol_dirty = false;

  if (s_state != CHAT_LISTENING) {
    lvgl_port_unlock();
    return;
  }

  static const int factor[4] = {60, 85, 100, 80};
  int amp = 3 + (s_volume * 3 / 100);
  if (amp > 6) {
    amp = 6;
  }

  for (int i = 0; i < 4; i++) {
    int a_amp = amp * factor[i] / 100;
    if (a_amp < 2) {
      a_amp = 2;
    }
    lv_anim_t *a = lv_anim_get(ui.capsules.capsules[i],
                              anim_capsule_sway_x_cb);
    if (a != NULL) {
      lv_anim_set_values(a, -a_amp, a_amp);
    }
  }
  lvgl_port_unlock();
}

void ai_chat_ui_sync_hw_volume(void) {
  if (ui.volume_ctrl.label != NULL) {
    lv_label_set_text_fmt(ui.volume_ctrl.label, "%u%%",
                           (unsigned)audio_get_volume());
  }
}

void ai_chat_ui_adjust_hw_volume(int8_t delta) {
  if (!lvgl_port_lock(pdMS_TO_TICKS(500))) {
    ESP_LOGE(TAG, "adjust_hw_volume: failed to acquire LVGL lock");
    return;
  }

  int new_volume = (int)audio_get_volume() + (int)delta;
  if (new_volume < (int)AUDIO_VOLUME_MIN) {
    new_volume = (int)AUDIO_VOLUME_MIN;
  }
  if (new_volume > (int)AUDIO_VOLUME_MAX) {
    new_volume = (int)AUDIO_VOLUME_MAX;
  }

  audio_set_volume((uint8_t)new_volume);
  ai_chat_ui_sync_hw_volume();
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
 *  Touch indicator + swipe
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

#define SWIPE_THRESHOLD  40

void ai_chat_ui_touch_swipe(int x, int y, bool pressed) {
  static bool s_tracking = false;
  static int s_start_x = 0;
  static int s_start_y = 0;
  static int s_last_x = 0;
  static int s_last_y = 0;

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
