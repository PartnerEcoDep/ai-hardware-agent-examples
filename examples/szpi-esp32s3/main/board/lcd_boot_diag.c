/**
 * @file lcd_boot_diag.c
 * @brief Startup diagnostic / progress-screen helpers.
 *
 * Moved out of the former monolithic main.c. All functions render to the
 * raw ST7789 panel via lcd_fill() / esp_lcd_panel_draw_bitmap(), so they
 * have no LVGL dependency and are safe before the UI is up.
 */

#include "lcd_boot_diag.h"
#include "lcd_init.h"
#include "board_lckfb_szpi.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"

static const char *TAG = "lcd_boot_diag";

static const uint16_t k_step_colors[8] = {
    0xF800, 0xF840, 0xFC00, 0x07E0, 0x07FF, 0x001F, 0x7FFF, 0xFFFF,
};

esp_err_t lcd_boot_diag_init(void) {
  /* Nothing to pre-allocate; kept for ui_panel_t signature compatibility. */
  return ESP_OK;
}

void lcd_show_status(int step) {
  lcd_fill(k_step_colors[step % 8]);
}

void lcd_diagnostic(void) {
  const int half_w = LCD_WIDTH / 2;
  const int half_h = LCD_HEIGHT / 2;
  struct {
    int x0, y0, x1, y1;
    uint16_t c;
    const char *name;
  } q[4] = {
      {0, 0, half_w, half_h, 0xF800, "RED"},
      {half_w, 0, LCD_WIDTH, half_h, 0x07E0, "GREEN"},
      {0, half_h, half_w, LCD_HEIGHT, 0x001F, "BLUE"},
      {half_w, half_h, LCD_WIDTH, LCD_HEIGHT, 0xFFFF, "WHITE"},
  };
  for (int i = 0; i < 4; i++) {
    uint16_t px = (q[i].c << 8) | (q[i].c >> 8); /* byte-swap for ST7789 */
    int n = (q[i].x1 - q[i].x0) * (q[i].y1 - q[i].y0);
    uint16_t *buf = (uint16_t *)heap_caps_malloc(
        n * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (buf == NULL) {
      ESP_LOGE(TAG, "diag %s malloc fail", q[i].name);
      continue;
    }
    for (int k = 0; k < n; k++) {
      buf[k] = px;
    }
    esp_err_t r = esp_lcd_panel_draw_bitmap(g_lcd_panel,
                                            q[i].x0, q[i].y0,
                                            q[i].x1, q[i].y1, buf);
    ESP_LOGI(TAG, "diag %s draw_bitmap -> %s", q[i].name,
             esp_err_to_name(r));
    free(buf);
  }
}

void lcd_test_pattern(void) {
  uint16_t *buf = (uint16_t *)heap_caps_malloc(
      LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t), MALLOC_CAP_DMA);
  if (buf == NULL) {
    lcd_fill(0xFFFF);
    return;
  }

  for (int y = 0; y < LCD_HEIGHT; y++) {
    for (int x = 0; x < LCD_WIDTH; x++) {
      uint16_t c;
      if (y < LCD_HEIGHT / 3) {
        c = 0xF800; /* red */
      } else if (y < LCD_HEIGHT * 2 / 3) {
        c = 0x07E0; /* green */
      } else {
        c = 0x001F; /* blue */
      }
      buf[y * LCD_WIDTH + x] = (c >> 8) | (c << 8); /* RGB565 swap */
    }
  }
  esp_lcd_panel_draw_bitmap(g_lcd_panel, 0, 0, LCD_WIDTH, LCD_HEIGHT, buf);
  free(buf);
}

void lcd_boot_diag_show_step(int step, const char *label) {
  lcd_show_status(step);
  ESP_LOGI(TAG, "boot diag step %d: %s", step, label ? label : "");
}
