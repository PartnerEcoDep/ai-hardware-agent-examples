/**
 * @file convai_config_file.c
 * @brief Configuration file reader — ESP32 stub.
 *
 * No filesystem-backed implementation yet. The public API is defined so
 * that convai_bridge_defaults.c's cfg_or() always falls back to the
 * hardcoded defaults. Wire up SPIFFS / FATFS / NVS later by implementing
 * convai_config_file_init_path().
 */

#include "convai_config_file.h"

const char *convai_config_file_get(const char *key)
{
    (void)key;
    return NULL;
}

int convai_config_file_init_path(const char *path)
{
    (void)path;
    return -1;
}

int convai_config_file_init(void)
{
    return -1;
}

void convai_config_file_deinit(void)
{
}
