/**
 * @file board_init.c
 * @brief Board-level hardware bring-up.
 *
 * Owns the shared I2C bus and PCA9557 IO-expander handles and the status
 * LED. Pulled out of the former monolithic main.c.
 */

#include "board_init.h"
#include "board_lckfb_szpi.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "board_init";

/* Shared hardware singletons. */
i2c_master_bus_handle_t g_i2c_bus = NULL;
pca9557_t g_pca9557 = {0};

void board_led_init(void) {
  gpio_config_t cfg = {
      .pin_bit_mask = (1ULL << BOARD_LED_GPIO),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&cfg);
  gpio_set_level(BOARD_LED_GPIO, 0);
}

void board_led_set(int on) {
  gpio_set_level(BOARD_LED_GPIO, on ? 1 : 0);
}

static esp_err_t i2c_bus_init(void) {
  i2c_master_bus_config_t cfg = {
      .i2c_port = BOARD_I2C_PORT,
      .sda_io_num = BOARD_I2C_SDA_PIN,
      .scl_io_num = BOARD_I2C_SCL_PIN,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags = {.enable_internal_pullup = 1},
  };
  esp_err_t ret = i2c_new_master_bus(&cfg, &g_i2c_bus);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(ret));
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "I2C bus initialized (SDA=%d, SCL=%d)",
           BOARD_I2C_SDA_PIN, BOARD_I2C_SCL_PIN);
  return ESP_OK;
}

esp_err_t board_init_all(void) {
  if (i2c_bus_init() != ESP_OK) {
    return ESP_FAIL;
  }
  if (pca9557_init(&g_pca9557, g_i2c_bus, PCA9557_I2C_ADDR) != 0) {
    ESP_LOGE(TAG, "PCA9557 init failed");
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "Board I2C + PCA9557 ready");
  return ESP_OK;
}
