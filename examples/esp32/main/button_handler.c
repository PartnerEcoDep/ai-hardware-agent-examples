/**
 * @file button_handler.c
 * @brief BOOT-button polling state machine.
 *
 * Pulled out of the former monolithic main.c. Maintains the press/release
 * edge detection and maps a short press to chat-state transitions.
 */

#include "button_handler.h"
#include "board_lckfb_szpi.h"
#include "ai_chat_ui.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "button";

#define LONG_PRESS_MS  1500  /* long-press threshold */
#define SHORT_MIN_MS   50    /* debounce for a valid short press */

static bool s_btn_was_down = false;
static bool s_long_press_fired = false;
static TickType_t s_btn_press_tick = 0;

static bool button_is_down(void) {
  return gpio_get_level(BOARD_BOOT_BUTTON_GPIO) == 0;
}

void button_handler_init(void) {
  gpio_config_t cfg = {
      .pin_bit_mask = (1ULL << BOARD_BOOT_BUTTON_GPIO),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
  };
  gpio_config(&cfg);
  ESP_LOGI(TAG, "Button GPIO%d init OK (polling)", BOARD_BOOT_BUTTON_GPIO);
}

void button_handler_poll(void) {
  bool down = button_is_down();
  TickType_t now = xTaskGetTickCount();

  if (down && !s_btn_was_down) {
    /* Press edge: record timestamp, arm long-press detection. */
    s_btn_press_tick = now;
    s_long_press_fired = false;
  } else if (down && s_btn_was_down) {
    /* Still down: fire long-press once threshold reached. */
    if (!s_long_press_fired &&
        (now - s_btn_press_tick) * portTICK_PERIOD_MS >= LONG_PRESS_MS) {
      s_long_press_fired = true;
    }
  } else if (!down && s_btn_was_down) {
    /* Release edge: treat as short press if not a long press. */
    if (!s_long_press_fired &&
        (now - s_btn_press_tick) * portTICK_PERIOD_MS >= SHORT_MIN_MS) {
      chat_state_t cur = ai_chat_ui_get_state();
      if (cur == STATE_IDLE) {
        ESP_LOGI(TAG, "Short press -> start conversation");
        ai_chat_ui_set_state(STATE_LISTENING);
      } else if (cur == STATE_LISTENING) {
        ESP_LOGI(TAG, "Short press -> cancel");
        ai_chat_ui_set_state(STATE_IDLE);
      } else if (cur == STATE_VOICE_SELECT) {
        /* Physical short press no longer drives the voice panel. */
      }
    }
  }

  s_btn_was_down = down;
}
