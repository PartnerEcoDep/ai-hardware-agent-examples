/**
 * @file platform/convai_platform_esp32.h
 * @brief ESP32-S3 platform abstraction layer for the ConvAI SDK.
 *
 * Implements @c convai_platform_t on top of FreeRTOS + lwIP + mbedTLS.
 * The layer implementations live under platform/; this header is the only
 * entry point application code should use.
 */

#ifndef CONVAI_PLATFORM_ESP32_H
#define CONVAI_PLATFORM_ESP32_H

#include "convai_platform.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Name this platform registers itself under in the platform registry. */
#define CONVAI_PLATFORM_ESP32_NAME  "esp32"

/** Lightweight registry of a platform implementation (folded in from the
 *  former standalone platform_factory module). A single platform is
 *  registered at startup; callers select it by name without depending on the
 *  concrete HAL. */
typedef struct platform_factory_s {
  const char *name;            /**< Platform name (e.g. "esp32"). */
  esp_err_t (*init)(void);     /**< Register + init the platform HAL. */
} platform_factory_t;

/**
 * @brief Register a platform implementation.
 * @param f Platform factory (must not be NULL).
 * @return ESP_OK on success.
 */
esp_err_t platform_factory_register(const platform_factory_t *f);

/**
 * @brief Initialize the registered platform by name.
 * @param name Platform name (NULL selects the registered platform).
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no match.
 */
esp_err_t platform_factory_init_by_name(const char *name);

/**
 * @brief Build the platform vtable and register it with the SDK.
 *
 * Must be called before convai_create().
 *
 * @return 0 on success, non-zero on failure (SDK convention).
 */
int convai_platform_esp32_init(void);

/**
 * @brief Register this platform with the local platform registry.
 *
 * Registration only records the implementation; the HAL is not initialized
 * until platform_factory_init_by_name() is called.
 *
 * @return ESP_OK on success.
 */
esp_err_t convai_platform_esp32_register(void);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_PLATFORM_ESP32_H */
