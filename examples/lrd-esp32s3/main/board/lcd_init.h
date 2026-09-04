/**
 * @file lcd_init.h
 * @brief ST7789 LCD initialization, backlight and low-level fill.
 */

#ifndef LCD_INIT_H
#define LCD_INIT_H

#include "esp_lcd_panel_dev.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shared LCD panel handles (defined in lcd_init.c). */
extern esp_lcd_panel_handle_t g_lcd_panel;  /**< ST7789 panel handle. */
extern esp_lcd_panel_io_handle_t g_lcd_io;  /**< ST7789 panel IO handle. */

/**
 * @brief Initialize the ST7789 SPI LCD and apply the board-specific
 *        register sequence. Must run before lcd_fill().
 * @return ESP_OK on success.
 */
esp_err_t lcd_init(void);

/** Enable the LCD backlight (GPIO42, active-low). */
void lcd_backlight_init(void);

/** Fill the entire screen with a single RGB565 color. */
void lcd_fill(uint16_t color);

#ifdef __cplusplus
}
#endif

#endif /* LCD_INIT_H */
