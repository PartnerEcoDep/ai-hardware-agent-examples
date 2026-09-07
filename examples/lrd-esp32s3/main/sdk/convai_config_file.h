/**
 * @file convai_config_file.h
 * @brief Configuration file reader (key=value parser) — ESP32 stub.
 *
 * On ESP32 there is no `/proc/self/exe`-style way to locate the running
 * firmware's directory, and the project does not currently ship an
 * embedded config file in the image. The public API is defined (returning
 * NULL / -1) so convai_bridge_defaults.c's cfg_or() fallback to hardcoded
 * defaults keeps working unchanged. A future implementation could back
 * this with SPIFFS / NVS / FATFS using convai_config_file_init_path().
 */
#ifndef CONVAI_CONFIG_FILE_H
#define CONVAI_CONFIG_FILE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- config value getter ---- */

/**
 * Get a raw config value by key, or NULL if not found.
 */
const char *convai_config_file_get(const char *key);

/* ---- lifecycle ---- */

/**
 * Initialise the config file module.
 *
 * On ESP32 this is a no-op stub: always returns -1 (no file system path
 * to the firmware). The defaults layer falls back to hardcoded values.
 *
 * @return -1 (file not found / not supported).
 */
int convai_config_file_init(void);

/**
 * Initialise the config file module from an explicit file path.
 * Reserved for a future SPIFFS / FATFS / NVS-backed implementation.
 *
 * @param path  Absolute or relative path to the config file.
 * @return -1 on failure.
 */
int convai_config_file_init_path(const char *path);

/**
 * Release all resources held by the config file module.
 * No-op on ESP32 stub.
 */
void convai_config_file_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_CONFIG_FILE_H */
