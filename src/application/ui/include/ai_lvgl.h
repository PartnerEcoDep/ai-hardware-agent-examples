#ifndef AI_LVGL_H
#define AI_LVGL_H
#include "ai_ui.h"
/* Platform UI owner calls these functions; no OS or board dependency. */
void ai_lvgl_create(const char *board_name, void (*command)(ai_ui_command_t));
void ai_lvgl_render(const ai_view_t *view);
void ai_lvgl_network(const char *description);
#endif
