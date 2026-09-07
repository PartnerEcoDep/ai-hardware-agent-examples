/**
 * @file lcd_boot_diag.h
 * @brief Startup diagnostic / progress-screen helpers.
 *
 * These draw directly to the raw ST7789 framebuffer (no LVGL dependency),
 * so they are safe to call immediately after lcd_init(). Useful for a
 * quick visual boot progress and for hardware sanity checks.
 */

#ifndef LCD_BOOT_DIAG_H
#define LCD_BOOT_DIAG_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the boot-diagnostic module.
 *
 * Nothing to allocate today; the signature matches @c ui_panel_t::init so the
 * panel can be registered with ui_panel_factory.
 *
 * @return Always ESP_OK.
 */
esp_err_t lcd_boot_diag_init(void);

/**
 * @brief Render one boot step on the raw LCD.
 * @param step Zero-based step index (drives the progress color).
 * @param label Short human-readable label, logged for reference.
 */
void lcd_boot_diag_show_step(int step, const char *label);

/* ---- Low-level diagnostic draw helpers (kept public for debugging) ---- */
void lcd_diagnostic(void);   /**< Draw 4 colored quadrants. */
void lcd_test_pattern(void);  /**< Draw R/G/B horizontal bands. */
void lcd_show_status(int step); /**< Fill screen with the step color. */

#ifdef __cplusplus
}
#endif

#endif /* LCD_BOOT_DIAG_H */
