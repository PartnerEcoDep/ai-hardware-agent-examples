/**
 * @file platform/convai_platform_esp32.c
 * @brief ESP32-S3 platform HAL entry point for the ConvAI SDK.
 *
 * This file assembles the @c convai_platform_t vtable and registers it, both
 * with the SDK and with a lightweight local registry. The actual
 * implementations live one layer down:
 *
 *   platform/esp32_osal.c   FreeRTOS  (memory / time / mutex / thread)
 *   platform/esp32_netal.c  lwIP      (non-blocking BSD sockets)
 *   platform/esp32_tlsal.c  mbedTLS   (TLS client over a raw fd)
 *   platform/esp32_misc.c   ESP-IDF   (log / device id / uuid / network)
 *
 * Reference implementation: examples/goldieos/platform/convai_platform_ws63.c
 */

#include "convai_platform_esp32.h"
#include "convai_platform_esp32_internal.h"

#include "esp_err.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = CONVAI_HAL_TAG;

/** The vtable handed to the SDK; static storage, never freed. */
static const convai_platform_t g_convai_platform = {
    .abi_version = CONVAI_ABI_VERSION,
    ._reserved = 0,
    .osal = {
        .malloc = esp32_malloc,
        .free = esp32_free,
        .get_time_ms = esp32_get_time_ms,
        .sleep_ms = esp32_sleep_ms,
        .get_tick_ms = esp32_get_tick_ms,
        .mutex_create = esp32_mutex_create,
        .mutex_destroy = esp32_mutex_destroy,
        .mutex_lock = esp32_mutex_lock,
        .mutex_unlock = esp32_mutex_unlock,
        .thread_create = esp32_thread_create,
        .thread_join = esp32_thread_join,
        .thread_destroy = esp32_thread_destroy,
        .fill_random = esp32_fill_random,
        .strdup = esp32_strdup,
    },
    .netal = {
        .socket_create = esp32_socket_create,
        .socket_destroy = esp32_socket_destroy,
        .socket_connect = esp32_socket_connect,
        .socket_send = esp32_socket_send,
        .socket_recv = esp32_socket_recv,
        .socket_set_nonblock = esp32_socket_set_nonblock,
        .socket_is_connected = esp32_socket_is_connected,
        .socket_get_fd = esp32_socket_get_fd,
        .socket_poll = esp32_socket_poll,
        .socket_get_error = esp32_socket_get_error,
    },
    .tlsal = {
        .tls_create = esp32_tls_create,
        .tls_destroy = esp32_tls_destroy,
        .tls_connect = esp32_tls_connect,
        .tls_handshake_step = esp32_tls_handshake_step,
        .tls_read = esp32_tls_read,
        .tls_write = esp32_tls_write,
        .tls_close = esp32_tls_close,
    },
    .misc = {
        .log = esp32_log,
        .device_id = esp32_device_id,
        .random = esp32_random,
        .uuid = esp32_uuid,
        .info = esp32_info,
        .network_available = esp32_network_available,
        .network_get_type = esp32_network_get_type,
    },
};

int convai_platform_esp32_init(void) {
  ESP_LOGI(TAG, "Registering ESP32 platform HAL (ABI 0x%04x)",
           CONVAI_ABI_VERSION);
  return convai_platform_init(&g_convai_platform);
}

