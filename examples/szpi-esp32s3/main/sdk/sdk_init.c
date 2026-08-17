/**
 * @file sdk_init.c
 * @brief Platform HAL bring-up + handoff to convai_bridge.
 *
 * Owns:
 *   - platform factory registration (convai_platform_esp32)
 *   - one-time call to convai_bridge_init() to create the SDK engine
 *
 * Does NOT call convai_bridge_start() — the session is started by the user
 * (long-press of the BOOT button) once the UI is on screen. All session
 * lifecycle (start/stop/restart, stats, callback routing, uplink thread)
 * is delegated to convai_bridge.c — see convai_bridge.h.
 */
#include "sdk_init.h"

#include "convai_bridge.h"
#include "convai_platform_esp32.h"

#include "esp_log.h"

static const char *TAG = "sdk_init";

esp_err_t sdk_init(void) {
  esp_err_t err = convai_platform_esp32_register();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "platform register failed: %s", esp_err_to_name(err));
    return err;
  }
  err = platform_factory_init_by_name(CONVAI_PLATFORM_ESP32_NAME);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "platform init failed: %s", esp_err_to_name(err));
    return err;
  }

  convai_bridge_init();
  return ESP_OK;
}
