#ifndef WS_UI_H
#define WS_UI_H
#include "ai_app.h"
#ifdef __cplusplus
extern "C" {
#endif
int ws_ui_init(void);
void ws_ui_render(void *ctx, const ai_view_t *view);
void ws_ui_network(const char *text);
#ifdef __cplusplus
}
#endif
#endif
