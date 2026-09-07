/**
 * @file wifi_prov_ui.c
 * @brief WiFi provisioning panel implementation (dark theme, raw framebuffer).
 *
 * Organized in three layers:
 *   1. Screen painters  — draw_*() render one full screen and flush it.
 *   2. Event source     — wifi_prov_ui_poll_event() maps the provisioning
 *                         return codes onto a single UI event.
 *   3. Panel entry      — wifi_prov_ui_init() / wifi_prov_ui_show() implement
 *                         the ui_panel_t contract used by ui_panel_factory.
 */

#include "wifi_prov_ui.h"

#include "lcd_ui.h"
#include "wifi_provisioning.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "wifi_prov_ui";

/* ---- Colors (RGB565, dark theme) ---- */
#define COLOR_BG       0x0000 /**< Pure black background. */
#define COLOR_WHITE    0xFFFF /**< Primary text. */
#define COLOR_CYAN     0x07FF /**< Accent. */
#define COLOR_GREEN    0x07E0 /**< Success. */
#define COLOR_YELLOW   0xFFE0 /**< Warning / hint. */
#define COLOR_GRAY     0x8410 /**< Secondary text. */
#define COLOR_ERROR_BG 0xF800 /**< Full-screen error background. */

/* ---- Layout ---- */
#define TITLE_Y      16
#define ICON_SIZE    32
#define ICON_X       ((LCD_UI_WIDTH - ICON_SIZE) / 2)
#define ICON_Y       60
#define STATUS_Y     110
#define CARD_Y       138
#define CARD_LINE1_Y (CARD_Y + 8)
#define CARD_LINE2_Y (CARD_Y + 48)
#define HINT_Y       222
#define HINT_LINE_H  16
#define ERROR_TEXT_Y 100

/* ---- Splash layout ---- */
#define SPLASH_TITLE_Y 80
#define SPLASH_RULE_W  200
#define SPLASH_RULE_H  2
#define SPLASH_RULE_X  ((LCD_UI_WIDTH - SPLASH_RULE_W) / 2)
#define SPLASH_RULE_Y  118
#define SPLASH_HINT_Y  140

/* ---- Timing / animation ---- */
#define SPLASH_HOLD_MS     800  /**< Splash screen dwell time. */
#define POLL_INTERVAL_MS   500  /**< Provisioning poll period. */
#define SUCCESS_HOLD_MS    2000 /**< Success screen dwell time. */
#define CONNECTING_DOT_MAX 3    /**< Max trailing dots in "connecting...". */

/* ---- WiFi icon signal levels ---- */
#define ICON_LEVEL_AP        1
#define ICON_LEVEL_CONNECTING 2
#define ICON_LEVEL_CONNECTED 3

/** Captive-portal address shown in AP mode. */
#define AP_CONFIG_URL "192.168.4.1"

/** True once wifi_prov_init() has succeeded. */
static bool s_ready = false;

/* ===================================================================
 *  Screen painters
 * =================================================================== */

/** Boot splash shown while the provisioning stack starts. */
static void draw_splash_screen(void) {
  lcd_ui_clear(COLOR_BG);
  lcd_ui_center_text_scaled(SPLASH_TITLE_Y, "AI Agent", 2, COLOR_WHITE,
                            COLOR_BG);
  lcd_ui_draw_rect(SPLASH_RULE_X, SPLASH_RULE_Y, SPLASH_RULE_W, SPLASH_RULE_H,
                   COLOR_CYAN);
  lcd_ui_center_text(SPLASH_HINT_Y, "Starting...", COLOR_GRAY, COLOR_BG);
  lcd_ui_flush();
}

/** Full-screen error message on a red background. */
static void draw_error_screen(const char *message) {
  lcd_ui_clear(COLOR_ERROR_BG);
  lcd_ui_center_text(ERROR_TEXT_Y, message, COLOR_WHITE, COLOR_ERROR_BG);
  lcd_ui_flush();
}

/** AP provisioning screen: hotspot name + configuration URL. */
static void draw_ap_screen(const char *ap_ssid, const char *url) {
  lcd_ui_clear(COLOR_BG);
  lcd_ui_center_text_scaled(TITLE_Y, "WiFi Setup", 2, COLOR_WHITE, COLOR_BG);
  lcd_ui_draw_wifi_icon(ICON_X, ICON_Y, ICON_SIZE, ICON_LEVEL_AP);
  lcd_ui_center_text(STATUS_Y, "AP Mode", COLOR_YELLOW, COLOR_BG);
  lcd_ui_center_text_scaled(CARD_LINE1_Y, ap_ssid ? ap_ssid : "N/A", 2,
                            COLOR_WHITE, COLOR_BG);
  lcd_ui_center_text(CARD_LINE2_Y, url ? url : AP_CONFIG_URL, COLOR_CYAN,
                     COLOR_BG);
  lcd_ui_center_text(HINT_Y, "Connect phone to hotspot", COLOR_GRAY, COLOR_BG);
  lcd_ui_center_text(HINT_Y + HINT_LINE_H, "Open browser -> config page",
                     COLOR_GRAY, COLOR_BG);
  lcd_ui_flush();
}

/** Station screen while connecting; @p dot_count animates the ellipsis. */
static void draw_connecting_screen(const char *target_ssid, int dot_count) {
  char status[32];

  lcd_ui_clear(COLOR_BG);
  lcd_ui_center_text_scaled(TITLE_Y, "Connecting", 2, COLOR_WHITE, COLOR_BG);
  lcd_ui_draw_wifi_icon(ICON_X, ICON_Y, ICON_SIZE, ICON_LEVEL_CONNECTING);

  snprintf(status, sizeof(status), "connecting%.*s", dot_count, "....");
  lcd_ui_center_text(STATUS_Y, status, COLOR_WHITE, COLOR_BG);

  lcd_ui_center_text_scaled(CARD_Y + 16, target_ssid ? target_ssid : "...", 2,
                            COLOR_CYAN, COLOR_BG);
  lcd_ui_flush();
}

/** Final screen: connected SSID + assigned IP. */
static void draw_connected_screen(const char *ssid, const char *ip) {
  lcd_ui_clear(COLOR_BG);
  lcd_ui_center_text_scaled(TITLE_Y, "Connected", 2, COLOR_WHITE, COLOR_BG);
  lcd_ui_draw_wifi_icon(ICON_X, ICON_Y, ICON_SIZE, ICON_LEVEL_CONNECTED);
  lcd_ui_center_text(STATUS_Y, "Success", COLOR_GREEN, COLOR_BG);
  lcd_ui_center_text_scaled(CARD_LINE1_Y, ssid ? ssid : "N/A", 2, COLOR_WHITE,
                            COLOR_BG);
  lcd_ui_center_text(CARD_LINE2_Y, ip ? ip : "0.0.0.0", COLOR_CYAN, COLOR_BG);
  lcd_ui_center_text(HINT_Y, "Enjoy your WiFi!", COLOR_GRAY, COLOR_BG);
  lcd_ui_flush();
}

/** Repaint whichever "still waiting" screen matches the current mode. */
static void draw_waiting_screen(bool ap_mode, int anim_frame) {
  if (ap_mode) {
    draw_ap_screen(wifi_prov_get_ap_ssid(), AP_CONFIG_URL);
    return;
  }
  const char *ssid = wifi_prov_get_ssid();
  draw_connecting_screen((strlen(ssid) > 0) ? ssid : "...",
                         anim_frame % (CONNECTING_DOT_MAX + 1));
}

/* ===================================================================
 *  Event source
 * =================================================================== */
wifi_prov_ui_event_t wifi_prov_ui_poll_event(void) {
  switch (wifi_prov_wait_connected_timeout(0)) {
    case 0:
      return WIFI_PROV_UI_EVENT_CONNECTED;
    case -1:
      return WIFI_PROV_UI_EVENT_ERROR;
    default:
      return WIFI_PROV_UI_EVENT_WAITING;
  }
}

/* ===================================================================
 *  Panel entry points (ui_panel_t contract)
 * =================================================================== */
esp_err_t wifi_prov_ui_init(void) {
  ESP_LOGI(TAG, "starting WiFi provisioning UI");

  lcd_ui_init();
  draw_splash_screen();
  vTaskDelay(pdMS_TO_TICKS(SPLASH_HOLD_MS));

  if (wifi_prov_init() != 0) {
    ESP_LOGE(TAG, "wifi_prov_init failed");
    draw_error_screen("WiFi Init Failed");
    s_ready = false;
    return ESP_FAIL;
  }

  s_ready = true;
  return ESP_OK;
}

void wifi_prov_ui_show(void) {
  if (!s_ready) {
    ESP_LOGW(TAG, "show: provisioning not initialized, skipping");
    return;
  }

  bool ap_mode = wifi_prov_is_ap_mode();
  int anim_frame = 0;

  ESP_LOGI(TAG, "%s mode active", ap_mode ? "AP" : "station");
  draw_waiting_screen(ap_mode, anim_frame);

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    anim_frame++;

    const wifi_prov_ui_event_t ev = wifi_prov_ui_poll_event();
    if (ev == WIFI_PROV_UI_EVENT_CONNECTED) {
      const char *ssid = wifi_prov_get_ssid();
      const char *ip = wifi_prov_get_ip();
      ESP_LOGI(TAG, "connected: ssid=%s ip=%s", ssid, ip);
      draw_connected_screen(ssid, ip);
      vTaskDelay(pdMS_TO_TICKS(SUCCESS_HOLD_MS));
      return;
    }
    if (ev == WIFI_PROV_UI_EVENT_ERROR) {
      ESP_LOGE(TAG, "provisioning error");
      draw_error_screen("WiFi Connect Error");
      return;
    }

    /* Still waiting: follow AP/station switches, animate the dots. */
    const bool now_ap = wifi_prov_is_ap_mode();
    const bool mode_changed = (now_ap != ap_mode);
    if (mode_changed) {
      ap_mode = now_ap;
      anim_frame = 0; /* restart the animation when the screen changes */
    }
    /* The AP screen is static, so only repaint it on a mode switch. */
    if (mode_changed || !ap_mode) {
      draw_waiting_screen(ap_mode, anim_frame);
    }
  }
}
