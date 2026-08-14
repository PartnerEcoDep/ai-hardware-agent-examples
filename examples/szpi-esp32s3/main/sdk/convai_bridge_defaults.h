/**
 * @file convai_bridge_defaults.h
 * @brief Internal config defaults and JSON builder for the ConvAI engine
 *        on ESP32 — kept identical in shape to goldieos so the SDK is
 *        initialised the same way across all platforms.
 *
 * Private header — used only by sdk_init.c.
 */
#ifndef CONVAI_BRIDGE_DEFAULTS_H
#define CONVAI_BRIDGE_DEFAULTS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Get the default agent (bot) ID. */
const char *bridge_get_default_agent_id(void);

/** Get the default startup config JSON string. */
const char *bridge_get_default_startup_config(void);

/**
 * Build the create-time JSON config string into @p buf.
 * @param device_name  Device name for the config (e.g. WiFi MAC from the app
 *                     layer). If NULL or empty, the hardcoded default is used.
 *                     Priority: device_name param > hardcoded default.
 *                     (Config file "device_name" is intentionally NOT supported
 *                     to avoid ambiguity — the device ID should be unique and
 *                     automatic, or fall back to the default.)
 * Returns pointer to @p buf.
 */
const char *bridge_build_config_json(char *buf, size_t buf_size,
                                     const char *device_name);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_BRIDGE_DEFAULTS_H */
