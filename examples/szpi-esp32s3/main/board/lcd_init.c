/**
 * @file lcd_init.c
 * @brief ST7789 320x240 SPI LCD bring-up.
 *
 * Owns the shared @c g_lcd_panel / @c g_lcd_io handles referenced by
 * lvgl_port.c and the boot-diagnostic module.
 */

#include "lcd_init.h"
#include "board_init.h"       /* g_pca9557 */
#include "board_lckfb_szpi.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

static const char *TAG = "lcd_init";

/* Shared LCD panel handles. */
esp_lcd_panel_handle_t g_lcd_panel = NULL;
esp_lcd_panel_io_handle_t g_lcd_io = NULL;

esp_err_t lcd_init(void) {
  /* SPI bus */
  spi_bus_config_t bus_cfg = {
      .mosi_io_num = LCD_MOSI_PIN,
      .miso_io_num = GPIO_NUM_NC,
      .sclk_io_num = LCD_CLK_PIN,
      .quadwp_io_num = GPIO_NUM_NC,
      .quadhd_io_num = GPIO_NUM_NC,
      .max_transfer_sz = LCD_WIDTH * 40 * 2, /* ~40 rows; fits DMA pool */
  };
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

  /* Panel IO (CS is driven by PCA9557, not a direct GPIO).
   * trans_queue_depth=3: each pending tx pre-allocates a DMA-capable
   * "priv" buffer of max_transfer_sz bytes. With max=25.6KB, the previous
   * depth=10 reserved 256KB of DMA pool, exceeding the bus's per-instance
   * budget and producing spicommon_dma_setup_priv_buffer(430) failures on
   * every LVGL flush. Depth 3 keeps in-flight ~75KB; ST7789 driver
   * internally splits full-frame bitmaps into many small transactions
   * (each <= max_transfer_sz) which is depth-tolerant. */
  esp_lcd_panel_io_spi_config_t io_cfg = {
      .cs_gpio_num = GPIO_NUM_NC,
      .dc_gpio_num = LCD_DC_PIN,
      .spi_mode = LCD_SPI_MODE,
      .pclk_hz = LCD_PIXEL_CLOCK_HZ,
      .trans_queue_depth = 3,
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_cfg, &g_lcd_io));

  /* ST7789 panel */
  esp_lcd_panel_dev_config_t panel_cfg = {
      .reset_gpio_num = GPIO_NUM_NC,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
      .bits_per_pixel = 16,
      .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
  };
  ESP_ERROR_CHECK(
      esp_lcd_new_panel_st7789(g_lcd_io, &panel_cfg, &g_lcd_panel));

  /* Reset + select LCD via PCA9557 CS (reset leaves CS released). */
  esp_lcd_panel_reset(g_lcd_panel);
  pca9557_set_output(&g_pca9557, PCA9557_BIT_LCD_CS, 0); /* bit0 LOW = select */
  esp_lcd_panel_init(g_lcd_panel); /* SLPOUT + MADCTL + COLMOD + RAMCTRL */

  /* ---- Board-specific ST7789 register tuning ---- */
  esp_lcd_panel_io_handle_t io = g_lcd_io;
  esp_lcd_panel_io_tx_param(io, 0xB2,
                            (uint8_t[]){0x0C, 0x0C, 0x00, 0x33, 0x33}, 5);
  esp_lcd_panel_io_tx_param(io, 0xB7, (uint8_t[]){0x35}, 1);
  esp_lcd_panel_io_tx_param(io, 0xBB, (uint8_t[]){0x19}, 1);
  esp_lcd_panel_io_tx_param(io, 0xC0, (uint8_t[]){0x2C}, 1);
  esp_lcd_panel_io_tx_param(io, 0xC2, (uint8_t[]){0x01}, 1);
  esp_lcd_panel_io_tx_param(io, 0xC3, (uint8_t[]){0x12}, 1);
  esp_lcd_panel_io_tx_param(io, 0xC4, (uint8_t[]){0x20}, 1);
  esp_lcd_panel_io_tx_param(io, 0xD0, (uint8_t[]){0xA4, 0xA1}, 2);
  /* PVGAMCTRL (0xE0): positive gamma */
  esp_lcd_panel_io_tx_param(io, 0xE0,
                            (uint8_t[]){0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B,
                                        0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B,
                                        0x1F, 0x23},
                            14);
  /* NVGAMCTRL (0xE1): negative gamma */
  esp_lcd_panel_io_tx_param(io, 0xE1,
                            (uint8_t[]){0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C,
                                        0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F,
                                        0x20, 0x23},
                            14);

  esp_lcd_panel_invert_color(g_lcd_panel, true);
  esp_lcd_panel_swap_xy(g_lcd_panel, true);
  esp_lcd_panel_mirror(g_lcd_panel, true, false);

  ESP_LOGI(TAG, "LCD ST7789 %dx%d initialized", LCD_WIDTH, LCD_HEIGHT);
  return ESP_OK;
}

void lcd_backlight_init(void) {
  gpio_config_t bl_cfg = {
      .pin_bit_mask = (1ULL << LCD_BACKLIGHT_PIN),
      .mode = GPIO_MODE_OUTPUT,
  };
  gpio_config(&bl_cfg);
  gpio_set_level(LCD_BACKLIGHT_PIN, 0); /* active-low: 0 = on */
  ESP_LOGI(TAG, "Backlight ON (GPIO%d LOW)", LCD_BACKLIGHT_PIN);
}

void lcd_fill(uint16_t color) {
  uint16_t *buf = (uint16_t *)heap_caps_malloc(
      LCD_WIDTH * 20 * sizeof(uint16_t), MALLOC_CAP_DMA);
  if (buf == NULL) {
    ESP_LOGE(TAG, "lcd_fill: malloc failed");
    return;
  }
  uint16_t swapped = (color << 8) | (color >> 8);
  for (int i = 0; i < LCD_WIDTH * 20; i++) {
    buf[i] = swapped;
  }

  for (int y = 0; y < LCD_HEIGHT; y += 20) {
    int end_y = y + 20;
    if (end_y > LCD_HEIGHT) {
      end_y = LCD_HEIGHT;
    }
    esp_err_t r = esp_lcd_panel_draw_bitmap(g_lcd_panel, 0, y,
                                            LCD_WIDTH, end_y, buf);
    if (r != ESP_OK) {
      ESP_LOGE(TAG, "lcd_fill draw_bitmap y=%d failed: %s", y,
               esp_err_to_name(r));
      break;
    }
  }
  free(buf);
}
