#ifndef ESP_MEDIA_H
#define ESP_MEDIA_H
#include "ai_app.h"
#include "ai_media.h"
#ifdef __cplusplus
extern "C" {
#endif
int esp_media_init(void (*post)(void *, const ai_event_t *), void *ctx);
void esp_media_set_session(uint32_t session);
ai_media_ops_t esp_media_ops(void);
#ifdef __cplusplus
}
#endif
#endif
