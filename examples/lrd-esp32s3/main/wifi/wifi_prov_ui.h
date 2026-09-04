/**
 * @file wifi_prov_ui.h
 * @brief WiFi provisioning panel (pre-LVGL, raw framebuffer).
 *
 * Implements the @c ui_panel_t contract: @c init brings up the framebuffer
 * and the provisioning stack, @c show runs the progress screens until the
 * device is connected (or provisioning fails).
 *
 * Screens: AP mode (hotspot + URL) -> connecting (animated dots) ->
 * connected (SSID + IP). Drawing goes through lcd_ui.h because LVGL is not
 * started yet at this point of the boot sequence.
 */

#ifndef WIFI_PROV_UI_H
#define WIFI_PROV_UI_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Provisioning progress, derived from the wifi_provisioning polling API.
 *
 * Kept separate from @c wifi_prov_event_t (the driver-level connect/disconnect
 * callback event) so UI code has a single value to switch on.
 */
typedef enum {
  WIFI_PROV_UI_EVENT_WAITING = 0, /**< Still waiting for a connection. */
  WIFI_PROV_UI_EVENT_CONNECTED,   /**< Station connected successfully. */
  WIFI_PROV_UI_EVENT_ERROR,       /**< Provisioning reported an error. */
} wifi_prov_ui_event_t;

/**
 * @brief Poll the provisioning stack once (non-blocking).
 * @return Current provisioning progress.
 */
wifi_prov_ui_event_t wifi_prov_ui_poll_event(void);

/**
 * @brief Initialize the framebuffer UI and the provisioning stack.
 *
 * Draws the splash screen, then calls wifi_prov_init(). On failure an error
 * screen stays on the display and wifi_prov_ui_show() becomes a no-op.
 *
 * @return ESP_OK on success, ESP_FAIL if provisioning could not start.
 */
esp_err_t wifi_prov_ui_init(void);

/**
 * @brief Run the provisioning screens until connected or failed.
 *
 * Non-blocking polling every 500 ms; the display is refreshed in between so
 * the connecting animation keeps running.
 */
void wifi_prov_ui_show(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_PROV_UI_H */
