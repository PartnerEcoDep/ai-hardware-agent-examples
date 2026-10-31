#ifndef AI_UI_H
#define AI_UI_H
#include "ai_app.h"
#include <stddef.h>

typedef enum { AI_UI_TOGGLE, AI_UI_PROVISION } ai_ui_command_t;
int ai_ui_init(void (*command)(ai_ui_command_t));
/* Thread-safe, copy-only updates; LVGL is owned by a dedicated UI task. */
void ai_ui_render(void *unused, const ai_view_t *view);
void ai_ui_network(const char *description);
#endif
