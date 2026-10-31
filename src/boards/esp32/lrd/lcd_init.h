#ifndef LCD_INIT_H
#define LCD_INIT_H
#include "esp_lcd_panel_dev.h"
#include "esp_err.h"
extern esp_lcd_panel_handle_t g_lcd_panel;
extern esp_lcd_panel_io_handle_t g_lcd_io;
esp_err_t lcd_init(void);
void lcd_backlight_init(void);
#endif
