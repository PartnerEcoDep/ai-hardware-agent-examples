/**
 * @file ui_panel.h
 * @brief Generic UI panel interface + factory.
 *
 * Every top-level screen (boot diagnostics, WiFi provisioning, main chat)
 * implements the same @c ui_panel_t contract and registers itself with the
 * factory, so the boot sequence can switch panels without hard-coded calls.
 */

#ifndef UI_PANEL_H
#define UI_PANEL_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque panel identifier used by the factory. */
typedef enum {
  UI_PANEL_BOOT_DIAG = 0,  /**< Startup diagnostic / progress screen. */
  UI_PANEL_WIFI_PROV,      /**< WiFi provisioning screen. */
  UI_PANEL_CHAT_MAIN,      /**< Main voice-assistant chat screen. */
  UI_PANEL_COUNT,
} ui_panel_id_t;

/** A switchable UI panel. Any member except @c name may be NULL. */
typedef struct ui_panel_s {
  const char *name;        /**< Panel name for logging. */
  esp_err_t (*init)(void); /**< One-time initialization (NULL = none). */
  void (*show)(void);      /**< Make the panel visible / run it. */
  void (*hide)(void);      /**< Hide the panel (NULL = none). */
  void (*tick)(void);      /**< Periodic update (NULL = none). */
  void (*destroy)(void);   /**< Tear down (NULL = none). */
} ui_panel_t;

/**
 * @brief Get the registered panel for an id.
 * @param id Panel identifier.
 * @return Panel pointer, or NULL if @p id is invalid.
 */
const ui_panel_t *ui_panel_factory_get(ui_panel_id_t id);

/**
 * @brief Show a panel by id (calls @c init once if present, then @c show).
 * @param id Panel identifier.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if @p id is invalid.
 */
esp_err_t ui_panel_factory_show(ui_panel_id_t id);

/**
 * @brief Hide a panel by id.
 * @param id Panel identifier.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if @p id is invalid.
 */
esp_err_t ui_panel_factory_hide(ui_panel_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* UI_PANEL_H */
