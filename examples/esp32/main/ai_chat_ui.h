/**
 * @file ai_chat_ui.h
 * @brief AI Chat UI - Voice Assistant (LVGL 9.x)
 *
 * Layout (320x240):
 *   [0..40]    status bar: dot + text / WiFi icon
 *   [50..170]  center voice orb (5 states)
 *   [180..195] state label (16px, centered)
 *   [200..220] hint label  (16px, centered)
 */
#ifndef AI_CHAT_UI_H
#define AI_CHAT_UI_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  CHAT_IDLE = 0,
  CHAT_LISTENING,
  CHAT_THINKING,
  CHAT_SPEAKING,
  CHAT_INTERRUPTED,
  CHAT_VOICE_SELECT,
  CHAT_DISCONNECTED,
} chat_state_t;

typedef chat_state_t voice_state_t;

#define STATE_IDLE         CHAT_IDLE
#define STATE_LISTENING    CHAT_LISTENING
#define STATE_THINKING     CHAT_THINKING
#define STATE_SPEAKING     CHAT_SPEAKING
#define STATE_INTERRUPTED  CHAT_INTERRUPTED
#define STATE_VOICE_SELECT CHAT_VOICE_SELECT
#define STATE_DISCONNECTED CHAT_DISCONNECTED

void ai_chat_ui_init(void);
void ai_chat_ui_tick(void);
voice_state_t ai_chat_ui_get_state(void);
void ai_chat_ui_set_state(chat_state_t state);
void ai_chat_ui_set_network(bool online);
void ai_chat_ui_set_connection(const char *ssid, const char *ip,
                               bool online);
void ai_chat_ui_update_volume(uint8_t level);

/* legacy no-op stubs */
void ai_chat_ui_add_message(const char *text, bool is_user);
void ai_chat_ui_show_voice_selector(bool show);
void ai_chat_ui_voice_select_next(void);
int  ai_chat_ui_voice_select_get(void);

#ifdef __cplusplus
}
#endif

#endif  /* AI_CHAT_UI_H */