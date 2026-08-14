/**
 * @file sdk_init.h
 * @brief Platform HAL bring-up + handoff to convai_bridge.
 *
 * Session lifecycle, SDK callbacks and the mic capture task live in
 * convai_bridge.{c,h}.
 */
#ifndef SDK_INIT_H
#define SDK_INIT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register the platform HAL, create the SDK engine and start the
 *        first session. On success, the mic-capture task is running and
 *        convai_bridge_is_started() returns non-zero.
 * @return ESP_OK on success, ESP_FAIL otherwise.
 */
esp_err_t sdk_init(void);

#ifdef __cplusplus
}
#endif

#endif /* SDK_INIT_H */
