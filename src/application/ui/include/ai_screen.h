#ifndef AI_SCREEN_H
#define AI_SCREEN_H
#include "ai_app.h"
#include <stdio.h>

/* Shared presentation contract; renderers may use LVGL or native widgets. */
enum {
    AI_SCREEN_WIDTH = 320, AI_SCREEN_HEIGHT = 240,
    AI_SCREEN_LEFT = 16, AI_SCREEN_TEXT_WIDTH = 288,
    AI_SCREEN_TITLE_Y = 12, AI_SCREEN_STATE_Y = 44, AI_SCREEN_NETWORK_Y = 76,
    AI_SCREEN_BUTTON_Y = 165, AI_SCREEN_BUTTON_HEIGHT = 52,
    AI_SCREEN_TOGGLE_WIDTH = 174, AI_SCREEN_WIFI_X = 206, AI_SCREEN_WIFI_WIDTH = 98
};
#define AI_SCREEN_BACKGROUND 0x15202b
#define AI_SCREEN_FOREGROUND 0xf2f5f7
#define AI_SCREEN_START "Start"
#define AI_SCREEN_STOP "Stop"
#define AI_SCREEN_WIFI "Wi-Fi"
static inline void ai_screen_status(const ai_view_t *view, char *text, size_t size) {
    if (view->error) snprintf(text, size, "Error %d - tap Start to retry", view->error);
    else snprintf(text, size, "%s", ai_state_name(view->state));
}
#endif
