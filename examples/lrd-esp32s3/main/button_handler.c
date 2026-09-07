/**
 * @file button_handler.c
 * @brief BOOT-button polling state machine.
 *
 * Pulled out of the former monolithic main.c. Maintains press/release
 * edge detection. A short press toggles the SDK cloud session
 * (convai_bridge_start / stop) — this is the single user-facing control
 * for AI conversation on/off, matching the goldieos CheckboxView_cloudsw.
 *
 * The chat UI state (IDLE / LISTENING / THINKING / SPEAKING) is driven
 * entirely by the SDK's on_conversation_status callback (see
 * convai_bridge.c:on_status) — this file does NOT poke UI state, the
 * way goldieos' cloud_status_callback does the same.
 */

#include "button_handler.h"
#include "board_lckfb_szpi.h"
#include "convai_bridge.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "button";

#define SHORT_MIN_MS   50    /* debounce for a valid short press */

static bool s_btn_was_down = false;
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
    /* Press edge: record timestamp for debounce. */
    s_btn_press_tick = now;
  } else if (!down && s_btn_was_down) {
    /* Release edge: short press -> toggle the SDK session. */
    if ((now - s_btn_press_tick) * portTICK_PERIOD_MS >= SHORT_MIN_MS) {
      if (convai_bridge_is_started()) {
        ESP_LOGI(TAG, "Press -> convai_bridge_stop");
        convai_bridge_stop();
      } else {
        ESP_LOGI(TAG, "Press -> convai_bridge_start");
        if (convai_bridge_start() != CONVAI_OK) {
          ESP_LOGE(TAG, "convai_bridge_start failed");
        }
      }
    }
  }

  s_btn_was_down = down;
}
