/*!
 * Copyright (C) 2026 Huawei Cloud Computing Technologies Co., Ltd.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef CONVAI_PLATFORM_H
#define CONVAI_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque types */
typedef struct convai_mutex_s         convai_mutex_t;
typedef struct convai_thread_s        convai_thread_t;
typedef struct convai_event_queue_s   convai_event_queue_t;
typedef struct convai_socket_s        convai_socket_t;
typedef struct convai_tls_s           convai_tls_t;

/* Thread types */
typedef void *(*convai_thread_func_t)(void *arg);

typedef struct {
    const char *name;
    size_t      stack_size;
    int         priority;
} convai_thread_param_t;

/* ABI version */
#define CONVAI_ABI_VERSION_1_0  0x0100
#define CONVAI_ABI_VERSION_1_1  0x0101  /* adds tlsal.tls_handshake_step (async TLS handshake) and netal.socket_get_error (non-blocking connect error check)*/
#define CONVAI_ABI_VERSION      CONVAI_ABI_VERSION_1_1

/* Poll flags for socket_poll */
#define CONVAI_POLL_READ   0x01
#define CONVAI_POLL_WRITE  0x02

/* OSAL callbacks */
typedef struct convai_osal_s {
    void *(*malloc)(size_t size);
    void (*free)(void *ptr);
    uint64_t (*get_time_ms)(void);
    void (*sleep_ms)(uint32_t ms);
    uint64_t (*get_tick_ms)(void);

    /* Mutex */
    int (*mutex_create)(convai_mutex_t **mutex);
    void (*mutex_destroy)(convai_mutex_t *mutex);
    void (*mutex_lock)(convai_mutex_t *mutex);
    void (*mutex_unlock)(convai_mutex_t *mutex);

    /* Thread */
    int (*thread_create)(convai_thread_t **thread,
                         convai_thread_func_t func,
                         void *arg,
                         const char *name,
                         size_t stack_size,
                         int priority);
    void (*thread_join)(convai_thread_t *thread);
    void (*thread_destroy)(convai_thread_t *thread);

    /* Random */
    int (*fill_random)(uint8_t *buf, size_t len);

    /* String */
    char *(*strdup)(const char *s);
} convai_osal_t;

/* NetAL callbacks */
typedef struct convai_netal_s {
    int (*socket_create)(convai_socket_t **sock);
    int (*socket_destroy)(convai_socket_t *sock);
    int (*socket_connect)(convai_socket_t *sock, const char *host, uint16_t port);
    int (*socket_send)(convai_socket_t *sock, const uint8_t *buf, size_t len, size_t *sent);
    int (*socket_recv)(convai_socket_t *sock, uint8_t *buf, size_t len, size_t *recvd);
    int (*socket_set_nonblock)(convai_socket_t *sock, int non_block);
    int (*socket_is_connected)(convai_socket_t *sock);
    int (*socket_get_fd)(convai_socket_t *sock);
    int (*socket_poll)(convai_socket_t *sock, int events, int *revents, int timeout_ms);
    int (*socket_get_error)(convai_socket_t *sock);
} convai_netal_t;

/* TLSAL callbacks */
typedef struct convai_tlsal_s {
    int (*tls_create)(convai_tls_t **tls);
    int (*tls_destroy)(convai_tls_t *tls);
    int (*tls_connect)(convai_tls_t *tls, void *sock, const char *host,
                       const char *ca_cert);
    int (*tls_handshake_step)(convai_tls_t *tls, int *want_flags, int *done);
    int (*tls_read)(convai_tls_t *tls, uint8_t *buf, size_t len, size_t *nread);
    int (*tls_write)(convai_tls_t *tls, const uint8_t *buf, size_t len, size_t *nwrite);
    int (*tls_close)(convai_tls_t *tls);
} convai_tlsal_t;

/* Misc callbacks */
typedef struct convai_misc_s {
    void (*log)(int level, const char *file, int line, const char *fmt, ...);
    int (*device_id)(char *buf, size_t len);
    int (*random)(uint8_t *buf, size_t len);
    int (*uuid)(char *buf, size_t size);
    int (*info)(char *buf, size_t size);
    int (*network_available)(void);
    int (*network_get_type)(char *buf, size_t size);
} convai_misc_t;

/* Unified platform struct */
typedef struct convai_platform_s {
    uint16_t        abi_version;
    uint16_t        _reserved;
    convai_osal_t   osal;
    convai_netal_t  netal;
    convai_tlsal_t  tlsal;
    convai_misc_t   misc;
} convai_platform_t;

/* Initialize platform abstraction layer */
int convai_platform_init(const convai_platform_t *platform);

/* Check whether a platform has been registered via convai_platform_init().
 * Returns 1 if registered, 0 otherwise. */
int convai_platform_is_registered(void);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_PLATFORM_H */