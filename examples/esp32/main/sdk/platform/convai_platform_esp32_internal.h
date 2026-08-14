/**
 * @file platform/convai_platform_esp32_internal.h
 * @brief Shared definitions for the ESP32-S3 ConvAI platform HAL.
 *
 * The HAL is split by layer, mirroring @c convai_platform_t:
 *
 *   esp32_osal.c   OSAL  — memory, time, mutex, thread, random, strdup
 *   esp32_netal.c  NetAL — lwIP BSD sockets (non-blocking)
 *   esp32_tlsal.c  TLSAL — mbedTLS client over a raw socket fd
 *   esp32_misc.c   Misc  — logging, device id, uuid, chip/network info
 *
 * convai_platform_esp32.c only assembles these into the vtable handed to the
 * SDK. This header carries the opaque handle layouts (shared by NetAL/TLSAL)
 * and the prototypes of every layer entry point.
 *
 * @note Internal to the HAL. Application code must use
 *       convai_platform_esp32.h.
 */

#ifndef CONVAI_PLATFORM_ESP32_INTERNAL_H
#define CONVAI_PLATFORM_ESP32_INTERNAL_H

#include "convai_platform_esp32.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Shared ESP_LOG tag for every HAL layer. */
#define CONVAI_HAL_TAG  "convai_hal"

/* ===================================================================
 *  Opaque handle layouts
 *
 *  The SDK only ever holds pointers to these; the layout is ours to
 *  define. They live here because NetAL and TLSAL both touch the socket.
 * =================================================================== */

struct convai_mutex_s {
  SemaphoreHandle_t handle;
};

struct convai_thread_s {
  TaskHandle_t handle;
  SemaphoreHandle_t exit_sem; /**< Signalled when the task body returns. */
  int exited;
};

struct convai_socket_s {
  int fd; /**< lwIP socket descriptor, -1 when closed. */
};

struct convai_tls_s {
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_x509_crt cacert;
  convai_socket_t *sock; /**< Borrowed socket, owned by the SDK. */
  int connected;
};

/* ===================================================================
 *  OSAL (esp32_osal.c)
 * =================================================================== */
void *esp32_malloc(size_t size);
void esp32_free(void *ptr);
uint64_t esp32_get_time_ms(void);
void esp32_sleep_ms(uint32_t ms);
uint64_t esp32_get_tick_ms(void);
int esp32_mutex_create(convai_mutex_t **mutex);
void esp32_mutex_destroy(convai_mutex_t *mutex);
void esp32_mutex_lock(convai_mutex_t *mutex);
void esp32_mutex_unlock(convai_mutex_t *mutex);
int esp32_thread_create(convai_thread_t **thread, convai_thread_func_t func,
                        void *arg, const char *name, size_t stack_size,
                        int priority);
void esp32_thread_join(convai_thread_t *thread);
void esp32_thread_destroy(convai_thread_t *thread);
int esp32_fill_random(uint8_t *buf, size_t len);
char *esp32_strdup(const char *s);

/* ===================================================================
 *  NetAL (esp32_netal.c)
 * =================================================================== */
int esp32_socket_create(convai_socket_t **sock);
int esp32_socket_destroy(convai_socket_t *sock);
int esp32_socket_connect(convai_socket_t *sock, const char *host,
                         uint16_t port);
int esp32_socket_send(convai_socket_t *sock, const uint8_t *buf, size_t len,
                      size_t *sent);
int esp32_socket_recv(convai_socket_t *sock, uint8_t *buf, size_t len,
                      size_t *recvd);
int esp32_socket_set_nonblock(convai_socket_t *sock, int non_block);
int esp32_socket_is_connected(convai_socket_t *sock);
int esp32_socket_get_fd(convai_socket_t *sock);
int esp32_socket_get_error(convai_socket_t *sock);
int esp32_socket_poll(convai_socket_t *sock, int events, int *revents,
                      int timeout_ms);

/* ===================================================================
 *  TLSAL (esp32_tlsal.c)
 * =================================================================== */
int esp32_tls_create(convai_tls_t **tls);
int esp32_tls_destroy(convai_tls_t *tls);
int esp32_tls_connect(convai_tls_t *tls, void *sock, const char *host,
                      const char *ca_cert);
int esp32_tls_handshake_step(convai_tls_t *tls, int *want_flags, int *done);
int esp32_tls_read(convai_tls_t *tls, uint8_t *buf, size_t len, size_t *nread);
int esp32_tls_write(convai_tls_t *tls, const uint8_t *buf, size_t len,
                    size_t *nwrite);
int esp32_tls_close(convai_tls_t *tls);

/* ===================================================================
 *  Misc (esp32_misc.c)
 * =================================================================== */
void esp32_log(int level, const char *file, int line, const char *fmt, ...);
int esp32_device_id(char *buf, size_t len);
int esp32_random(uint8_t *buf, size_t len);
int esp32_uuid(char *buf, size_t size);
int esp32_info(char *buf, size_t size);
int esp32_network_available(void);
int esp32_network_get_type(char *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_PLATFORM_ESP32_INTERNAL_H */
