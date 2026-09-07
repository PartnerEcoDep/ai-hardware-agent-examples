/**
 * @file ui/ui_panel.c
 * @brief UI panel registry implementation.
 *
 * Aggregates the three top-level panels. The boot-diagnostic panel exposes
 * only @c init; its step-wise progress is drawn via lcd_boot_diag_show_step()
 * directly during startup. The chat panel is the persistent base layer, so
 * its show/hide are intentionally lightweight.
 */

#include "ui_panel.h"
#include "lcd_boot_diag.h"
#include "wifi_prov_ui.h"
#include "ai_chat_ui.h"
#include "esp_err.h"
#include "esp_log.h"

#include <stdbool.h>

static const char *TAG = "ui_panel";

static const ui_panel_t s_panels[UI_PANEL_COUNT] = {
    [UI_PANEL_BOOT_DIAG] = {
        .name = "boot_diag",
        .init = lcd_boot_diag_init,
        .show = NULL,  /* step-wise show via lcd_boot_diag_show_step() */
        .hide = NULL,
    },
    [UI_PANEL_WIFI_PROV] = {
        .name = "wifi_prov",
        .init = wifi_prov_ui_init,
        .show = wifi_prov_ui_show,
        .hide = NULL,
    },
    [UI_PANEL_CHAT_MAIN] = {
        .name = "chat_main",
        .init = ai_chat_ui_init,
        .show = ai_chat_ui_show,
        .hide = ai_chat_ui_hide,
        .tick = ai_chat_ui_tick,
    },
};

/* Tracks which panels already ran their one-time init. */
static bool s_initialized[UI_PANEL_COUNT] = {false};

const ui_panel_t *ui_panel_factory_get(ui_panel_id_t id) {
  if (id >= UI_PANEL_COUNT) {
    return NULL;
  }
  return &s_panels[id];
}

esp_err_t ui_panel_factory_show(ui_panel_id_t id) {
  const ui_panel_t *p = ui_panel_factory_get(id);
  if (p == NULL) {
    ESP_LOGE(TAG, "show: invalid panel id %d", id);
    return ESP_ERR_INVALID_ARG;
  }
  if (p->init != NULL && !s_initialized[id]) {
    esp_err_t err = p->init();
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "%s: init failed: %s", p->name, esp_err_to_name(err));
      return err;
    }
    s_initialized[id] = true;
  }
  if (p->show) {
    p->show();
  }
  return ESP_OK;
}

esp_err_t ui_panel_factory_hide(ui_panel_id_t id) {
  const ui_panel_t *p = ui_panel_factory_get(id);
  if (p == NULL) {
    ESP_LOGE(TAG, "hide: invalid panel id %d", id);
    return ESP_ERR_INVALID_ARG;
  }
  if (p->hide) {
    p->hide();
  }
  return ESP_OK;
}
