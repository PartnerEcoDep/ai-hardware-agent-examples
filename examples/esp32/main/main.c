/**
 * @file main.c
 * @brief LCKFB ESP32-S3 鈥?AI Hardware Agent entry point.
 *
 * Boot sequence (delegated to focused modules):
 *   1. GPIO / status LED
 *   2. NVS
 *   3. I2C bus + PCA9557 IO expander
 *   4. LCD ST7789 init + backlight
 *   5. Audio (ES8311 + ES7210 + I2S)
 *   6. WiFi provisioning
 *   7. LVGL + chat UI
 *   8. Platform HAL + SDK session
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_lcd_panel_ops.h"

#include "app_state.h"
#include "board_init.h"
#include "lcd_init.h"
#include "lcd_boot_diag.h"
#include "audio_init.h"
#include "sntp_init.h"
#include "sdk_init.h"
#include "button_handler.h"
#include "lvgl_port.h"
#include "ai_chat_ui.h"
#include "wifi_prov_ui.h"
#include "wifi_provisioning.h"
#include "voice_factory.h"
#include "ui_panel.h"

static const char *TAG = "main";

void app_main(void) {
  printf("\n=== ESP32-S3 Step-by-step Init ===\n\n");
  app_state_set(APP_STATE_BOOTING);

  /* 1. GPIO / status LED */
  board_led_init();
  board_led_set(1);
  printf("[1/8] GPIO OK\n");
  fflush(stdout);

  /* 2. NVS */
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }
  printf("[2/8] NVS OK\n");
  fflush(stdout);

  /* 3-4. I2C bus + PCA9557 */
  if (board_init_all() != ESP_OK) {
    printf("[3-4/8] I2C/PCA9557 FAILED, skipping hardware\n");
    goto skip_hw;
  }
  printf("[3-4/8] I2C/PCA9557 OK\n");
  fflush(stdout);

  /* 5. LCD */
  printf("[5/8] LCD init...\n");
  fflush(stdout);
  if (lcd_init() != ESP_OK) {
    printf("[5/8] LCD FAILED\n");
    goto skip_hw;
  }
  lcd_fill(0x0000);
  esp_lcd_panel_disp_on_off(g_lcd_panel, true);
  lcd_backlight_init();
  printf("[5/8] LCD OK\n");
  fflush(stdout);

  /* 6. Audio */
  printf("[6/8] Audio init...\n");
  fflush(stdout);
  if (audio_init() != ESP_OK) {
    printf("[6/8] Audio FAILED\n");
  } else {
    printf("[6/8] Audio OK\n");
  }
  fflush(stdout);

  /* 7. WiFi provisioning (panel runs until connected or failed) */
  printf("[7/8] WiFi init...\n");
  fflush(stdout);
  if (ui_panel_factory_show(UI_PANEL_WIFI_PROV) != ESP_OK) {
    printf("[7/8] WiFi FAILED\n");
  } else {
    printf("[7/8] WiFi OK\n");
  }
  fflush(stdout);

  /* SNTP time sync (network is up once provisioning succeeds). */
  sntp_init_sync();

  /* LVGL + main chat UI */
  lvgl_port_init();
  lvgl_port_touch_init(g_i2c_bus);
  voice_factory_init();
  ui_panel_factory_show(UI_PANEL_CHAT_MAIN);
  ai_chat_ui_set_network(wifi_prov_is_connected());
  printf("AI Chat UI ready (IDLE)\n");
  fflush(stdout);

  /* 8. Platform HAL + SDK engine (no session start until BOOT long-press) */
  printf("[8/8] HAL register + SDK engine...\n");
  fflush(stdout);
  if (sdk_init() != ESP_OK) {
    printf("[8/8] SDK init FAILED\n");
  } else {
    printf("[8/8] SDK engine ready (idle, awaiting BOOT long-press)\n");
  }
  fflush(stdout);

  app_state_set(APP_STATE_RUNNING);

skip_hw:
  button_handler_init();
  printf("\n=== Init complete, press BOOT to start AI conversation ===\n");
  board_led_set(0);

  static int s_hb_cnt = 0;
  while (1) {
    button_handler_poll();
    ai_chat_ui_tick();

    if (++s_hb_cnt >= 200) { /* ~10s */
      s_hb_cnt = 0;
      ESP_LOGI(TAG, "heartbeat: free_heap=%u, min_free=%u",
               (unsigned)esp_get_free_heap_size(),
               (unsigned)esp_get_minimum_free_heap_size());
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
