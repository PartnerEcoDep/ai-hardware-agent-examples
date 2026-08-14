/**
 * @file sdk_init.h
 * @brief SDK platform registration, engine creation and session start.
 */

#ifndef SDK_INIT_H
#define SDK_INIT_H

#include "convai_api.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shared SDK engine handle (defined in sdk_init.c). */
extern convai_engine_t g_engine;

/**
 * @brief Register the platform HAL, create the SDK engine, start the
 *        session and launch the audio-capture task.
 * @return ESP_OK on success, ESP_FAIL otherwise.
 */
esp_err_t sdk_init(void);

#ifdef __cplusplus
}
#endif

#endif /* SDK_INIT_H */
