/**
 * @file convai_platform_esp32.h
 * @brief ESP32-S3 platform abstraction layer header.
 *
 * 基于 FreeRTOS + lwIP + mbedTLS 实现 convai_platform_t。
 * SDK 通过调用 convai_platform_esp32_init() 注册平台接口。
 */

#ifndef CONVAI_PLATFORM_ESP32_H
#define CONVAI_PLATFORM_ESP32_H

#include "convai_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化并注册 ESP32 平台抽象层到 SDK。
 *
 * 必须在 convai_create() 之前调用。
 *
 * @return 0 成功, 非 0 失败。
 */
int convai_platform_esp32_init(void);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_PLATFORM_ESP32_H */
