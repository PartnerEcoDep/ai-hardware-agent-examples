#ifndef WS_MEDIA_H
#define WS_MEDIA_H
#include "ai_media.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Resolve the backend service before UI startup, preserving BSP ordering. */
void ws_media_prepare(void);
int ws_media_init(void);
void ws_media_session(uint32_t session);
ai_media_ops_t ws_media_ops(void);
#ifdef __cplusplus
}
#endif
#endif
