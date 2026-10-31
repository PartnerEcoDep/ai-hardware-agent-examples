#ifndef BOARD_DISPLAY_H
#define BOARD_DISPLAY_H
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
/* ESP-IDF display adapter only. These handles never enter the portable core. */
esp_lcd_panel_handle_t board_display_panel(void);
esp_lcd_panel_io_handle_t board_display_io(void);
#define BOARD_DISPLAY_WIDTH 320
#define BOARD_DISPLAY_HEIGHT 240
#endif
