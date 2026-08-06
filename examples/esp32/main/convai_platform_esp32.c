/**
 * @file convai_platform_esp32.c
 * @brief ESP32-S3 platform abstraction implementation for ConvAI SDK.
 *
 * 将 convai_platform_t 的四层 HAL 映射到 ESP-IDF API:
 *  - OSAL  : FreeRTOS (xTaskCreate / xSemaphoreCreateMutex / pvPortMalloc)
 *  - NetAL : lwIP  BSD sockets (socket / connect / send / recv / select)
 *  - TLSAL : mbedTLS (mbedtls_ssl_*)
 *  - Misc  : esp_efuse / esp_fill_random / esp_netif
 *
 * 参考: examples/goldieos/platform/convai_platform_ws63.c
 */

#include "convai_platform_esp32.h"

/* ---- ESP-IDF headers ---- */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_chip_info.h"

/* ---- lwIP ---- */
#include "lwip/sockets.h"
#include "lwip/netdb.h"

/* ---- mbedTLS 4.x (PSA Crypto backend, ESP-IDF built-in) ---- */
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "psa/crypto.h"

/* ---- Standard C ---- */
#include <sys/time.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <unistd.h>

static const char *TAG = "convai_hal";

/* ===================================================================
 *  Opaque type definitions
 *
 *  SDK 通过这些结构体指针访问平台资源，不直接读写成员。
 *  结构体内部布局由 HAL 自由定义。
 * =================================================================== */

struct convai_mutex_s {
    SemaphoreHandle_t handle;
};

struct convai_thread_s {
    TaskHandle_t   handle;
    SemaphoreHandle_t exit_sem;  /* 线程退出信号 */
    int            exited;
};

struct convai_socket_s {
    int fd;
};

struct convai_tls_s {
    mbedtls_ssl_context     ssl;
    mbedtls_ssl_config      conf;
    mbedtls_x509_crt        cacert;
    convai_socket_t        *sock;      /* 持有的 socket 引用 */
    int                     connected;
};

/* ===================================================================
 *  OSAL — Memory
 * =================================================================== */

static void *esp32_malloc(size_t size) {
    return malloc(size);
}

static void esp32_free(void *ptr) {
    free(ptr);
}

/* ===================================================================
 *  OSAL — Time
 *
 *  get_time_ms:  UTC 毫秒时间戳(SNTP 同步后可用), 用于日志 / token 过期
 *  get_tick_ms:  单调毫秒计数(boot 起), 用于超时/间隔计算, 不会跳变
 * =================================================================== */

static uint64_t esp32_get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
}

static void esp32_sleep_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static uint64_t esp32_get_tick_ms(void) {
    /* esp_timer_get_time() returns microseconds since boot (monotonic, int64_t) */
    return (uint64_t)(esp_timer_get_time() / 1000);
}

/* ===================================================================
 *  OSAL — Mutex (FreeRTOS recursive mutex)
 * =================================================================== */

static int esp32_mutex_create(convai_mutex_t **mutex) {
    if (mutex == NULL) return -1;
    convai_mutex_t *m = (convai_mutex_t *)malloc(sizeof(*m));
    if (m == NULL) return -1;
    m->handle = xSemaphoreCreateRecursiveMutex();
    if (m->handle == NULL) {
        free(m);
        return -1;
    }
    *mutex = m;
    return 0;
}

static void esp32_mutex_destroy(convai_mutex_t *mutex) {
    if (mutex == NULL) return;
    if (mutex->handle) vSemaphoreDelete(mutex->handle);
    free(mutex);
}

static void esp32_mutex_lock(convai_mutex_t *mutex) {
    if (mutex == NULL) return;
    xSemaphoreTakeRecursive(mutex->handle, portMAX_DELAY);
}

static void esp32_mutex_unlock(convai_mutex_t *mutex) {
    if (mutex == NULL) return;
    xSemaphoreGiveRecursive(mutex->handle);
}

/* ===================================================================
 *  OSAL — Thread (FreeRTOS xTaskCreate)
 * =================================================================== */

typedef struct {
    convai_thread_func_t func;
    void                *arg;
    convai_thread_t     *thread;
} esp32_thread_args_t;

static void esp32_thread_entry(void *pv) {
    esp32_thread_args_t *a = (esp32_thread_args_t *)pv;
    convai_thread_func_t func   = a->func;
    void                *arg    = a->arg;
    convai_thread_t     *thread = a->thread;
    free(a);

    if (func) func(arg);

    /* 通知线程已退出, 供 join/destroy 等待 */
    if (thread) {
        thread->exited = 1;
        xSemaphoreGive(thread->exit_sem);
    }
    vTaskDelete(NULL);
}

static int esp32_thread_create(convai_thread_t **thread,
                               convai_thread_func_t func,
                               void *arg,
                               const char *name,
                               size_t stack_size,
                               int priority) {
    if (thread == NULL || func == NULL) return -1;

    convai_thread_t *t = (convai_thread_t *)calloc(1, sizeof(*t));
    if (t == NULL) return -1;

    esp32_thread_args_t *a = (esp32_thread_args_t *)malloc(sizeof(*a));
    if (a == NULL) { free(t); return -1; }
    a->func   = func;
    a->arg    = arg;
    a->thread = t;

    t->exit_sem = xSemaphoreCreateBinary();
    if (t->exit_sem == NULL) {
        free(a); free(t);
        return -1;
    }

    UBaseType_t uxPriority = (priority > 0) ? (UBaseType_t)priority
                            : (tskIDLE_PRIORITY + 2);
    uint32_t    usStack    = (stack_size > 0) ? (uint32_t)stack_size : 4096;
    const char *taskName   = (name != NULL) ? name : "convai";

    BaseType_t ret = xTaskCreate(esp32_thread_entry,
                                 taskName,
                                 usStack,
                                 a,
                                 uxPriority,
                                 &t->handle);
    if (ret != pdPASS) {
        vSemaphoreDelete(t->exit_sem);
        free(a);
        free(t);
        return -1;
    }

    *thread = t;
    return 0;
}

static void esp32_thread_join(convai_thread_t *thread) {
    if (thread == NULL) return;
    xSemaphoreTake(thread->exit_sem, portMAX_DELAY);
}

static void esp32_thread_destroy(convai_thread_t *thread) {
    if (thread == NULL) return;
    /* 等线程退出后再释放资源 */
    if (!thread->exited) {
        xSemaphoreTake(thread->exit_sem, portMAX_DELAY);
    }
    if (thread->exit_sem) vSemaphoreDelete(thread->exit_sem);
    /* FreeRTOS 任务退出时已调用 vTaskDelete(NULL), 不需要再删 */
    thread->handle = NULL;
    free(thread);
}

/* ===================================================================
 *  OSAL — Misc (Random / String)
 * =================================================================== */

static int esp32_fill_random(uint8_t *buf, size_t len) {
    if (buf == NULL) return -1;
    esp_fill_random(buf, len);
    return 0;
}

static char *esp32_strdup(const char *s) {
    if (s == NULL) return NULL;
    return strdup(s);
}

/* ===================================================================
 *  NetAL — Socket (lwIP BSD sockets)
 *
 *  全程非阻塞: socket 创建后设 O_NONBLOCK, connect 立即返回 EINPROGRESS,
 *  SDK 通过 socket_poll(POLL_WRITE) + socket_get_error(SO_ERROR) 等待连接完成.
 * =================================================================== */

static int esp32_socket_create(convai_socket_t **sock) {
    if (sock == NULL) return -1;
    *sock = (convai_socket_t *)calloc(1, sizeof(**sock));
    if (*sock == NULL) return -1;
    (*sock)->fd = -1;
    return 0;
}

static int esp32_socket_destroy(convai_socket_t *sock) {
    if (sock == NULL) return 0;
    if (sock->fd >= 0) {
        close(sock->fd);
        sock->fd = -1;
    }
    free(sock);
    return 0;
}

static int esp32_socket_connect(convai_socket_t *sock,
                                const char *host,
                                uint16_t port) {
    if (sock == NULL || host == NULL) return -1;

    /* 1. 解析 DNS */
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", port);

    ESP_LOGI(TAG, "resolving %s:%s ...", host, port_str);
    struct addrinfo *res = NULL;
    int gai = getaddrinfo(host, port_str, &hints, &res);
    if (gai != 0 || res == NULL) {
        ESP_LOGE(TAG, "DNS resolve failed for %s (gai=%d)", host, gai);
        return -1;
    }

    /* 2. 创建 TCP socket */
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        ESP_LOGE(TAG, "socket() failed, errno=%d", errno);
        freeaddrinfo(res);
        return -1;
    }

    /* 3. 设为非阻塞 */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    /* 4. 发起连接 (非阻塞, EINPROGRESS 是预期结果) */
    int cr = connect(fd, res->ai_addr, res->ai_addrlen);
    int saved_errno = errno;
    freeaddrinfo(res);

    if (cr != 0 && saved_errno != EINPROGRESS) {
        ESP_LOGE(TAG, "connect() failed, errno=%d", saved_errno);
        close(fd);
        return -1;
    }

    ESP_LOGI(TAG, "connect initiated fd=%d (%s)", fd,
             (cr == 0) ? "connected" : "EINPROGRESS");

    sock->fd = fd;
    return 0;
}

static int esp32_socket_send(convai_socket_t *sock,
                             const uint8_t *buf, size_t len,
                             size_t *sent) {
    if (sent) *sent = 0;
    if (sock == NULL || buf == NULL) return -1;
    if (sock->fd < 0) return -1;

    ssize_t ret = send(sock->fd, buf, len, 0);
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -1;
    }
    if (sent) *sent = (size_t)ret;
    return 0;
}

static int esp32_socket_recv(convai_socket_t *sock,
                             uint8_t *buf, size_t len,
                             size_t *recvd) {
    if (recvd) *recvd = 0;
    if (sock == NULL || buf == NULL) return -1;
    if (sock->fd < 0) return -1;

    ssize_t ret = recv(sock->fd, buf, len, 0);
    if (ret == 0) return -1;   /* peer closed */
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -1;
    }
    if (recvd) *recvd = (size_t)ret;
    return 0;
}

static int esp32_socket_set_nonblock(convai_socket_t *sock, int non_block) {
    if (sock == NULL || sock->fd < 0) return -1;
    int flags = fcntl(sock->fd, F_GETFL, 0);
    if (flags < 0) return -1;
    if (non_block) flags |= O_NONBLOCK;
    else           flags &= ~O_NONBLOCK;
    return fcntl(sock->fd, F_SETFL, flags);
}

static int esp32_socket_is_connected(convai_socket_t *sock) {
    if (sock == NULL) return 0;
    return sock->fd >= 0 ? 1 : 0;
}

static int esp32_socket_get_fd(convai_socket_t *sock) {
    if (sock == NULL) return -1;
    return sock->fd;
}

static int esp32_socket_get_error(convai_socket_t *sock) {
    if (sock == NULL || sock->fd < 0) return -1;
    int so_error = 0;
    socklen_t optlen = sizeof(so_error);
    if (getsockopt(sock->fd, SOL_SOCKET, SO_ERROR, &so_error, &optlen) < 0)
        return -1;
    return so_error;  /* 0 = no error, positive = errno */
}

static int esp32_socket_poll(convai_socket_t *sock,
                             int events, int *revents,
                             int timeout_ms) {
    if (sock == NULL || revents == NULL) return -1;
    *revents = 0;

    int fd = sock->fd;
    if (fd < 0) return -1;

    fd_set rfds, wfds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    if (events & CONVAI_POLL_READ)  FD_SET(fd, &rfds);
    if (events & CONVAI_POLL_WRITE) FD_SET(fd, &wfds);

    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(fd + 1, &rfds, &wfds, NULL, &tv);
    if (ret < 0) {
        ESP_LOGE(TAG, "select() failed fd=%d errno=%d", fd, errno);
        return -1;
    }
    if (FD_ISSET(fd, &rfds)) *revents |= CONVAI_POLL_READ;
    if (FD_ISSET(fd, &wfds)) *revents |= CONVAI_POLL_WRITE;
    return 0;
}

/* ===================================================================
 *  TLSAL — mbedTLS (ESP-IDF built-in)
 *
 *  BIO 直接读写 socket fd, 非阻塞模式下 would-block 正确返回
 *  WANT_READ / WANT_WRITE, SDK 通过 socket_poll 驱动握手和数据传输.
 * =================================================================== */

/* BIO send: 直接对 socket fd 调用 send() */
static int esp32_tls_bio_send(void *ctx,
                              const unsigned char *buf, size_t len) {
    convai_socket_t *sock = (convai_socket_t *)ctx;
    if (sock == NULL || sock->fd < 0) return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    int ret = send(sock->fd, buf, len, 0);
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return MBEDTLS_ERR_SSL_WANT_WRITE;
        return MBEDTLS_ERR_NET_SEND_FAILED;
    }
    return ret;
}

/* BIO recv: 直接对 socket fd 调用 recv() */
static int esp32_tls_bio_recv(void *ctx,
                              unsigned char *buf, size_t len) {
    convai_socket_t *sock = (convai_socket_t *)ctx;
    if (sock == NULL || sock->fd < 0) return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    int ret = recv(sock->fd, buf, len, 0);
    if (ret == 0) return MBEDTLS_ERR_SSL_CONN_EOF;
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return MBEDTLS_ERR_SSL_WANT_READ;
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }
    return ret;
}

static int esp32_tls_create(convai_tls_t **tls) {
    if (tls == NULL) return -1;

    convai_tls_t *t = (convai_tls_t *)calloc(1, sizeof(*t));
    if (t == NULL) return -1;

    /* mbedTLS 4.x: PSA Crypto 内置硬件 RNG, 无需手动管理 ctr_drbg/entropy */
    psa_crypto_init();

    mbedtls_ssl_init(&t->ssl);
    mbedtls_ssl_config_init(&t->conf);
    mbedtls_x509_crt_init(&t->cacert);

    /* 默认配置 (TLS client) */
    int ret = mbedtls_ssl_config_defaults(&t->conf,
                                           MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) goto fail;

    /* mbedTLS 4.x: PSA Crypto 内部已管理 RNG (硬件 TRNG), 无需显式配置 */
    (void)ret;
    mbedtls_ssl_conf_authmode(&t->conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_dbg(&t->conf, NULL, NULL);

    ret = mbedtls_ssl_setup(&t->ssl, &t->conf);
    if (ret != 0) goto fail;

    *tls = t;
    return 0;

fail:
    mbedtls_ssl_free(&t->ssl);
    mbedtls_ssl_config_free(&t->conf);
    mbedtls_x509_crt_free(&t->cacert);
    free(t);
    return -1;
}

static int esp32_tls_destroy(convai_tls_t *tls) {
    if (tls == NULL) return 0;
    mbedtls_ssl_free(&tls->ssl);
    mbedtls_ssl_config_free(&tls->conf);
    mbedtls_x509_crt_free(&tls->cacert);
    mbedtls_x509_crt_free(&tls->cacert);
    free(tls);
    return 0;
}

static int esp32_tls_connect(convai_tls_t *tls,
                             void *sock,
                             const char *host,
                             const char *ca_cert) {
    if (tls == NULL || sock == NULL || host == NULL) return -1;

    convai_socket_t *socket = (convai_socket_t *)sock;
    tls->sock = socket;

    /* 设置 SNI hostname */
    mbedtls_ssl_set_hostname(&tls->ssl, host);

    /* 绑定 BIO: 直接对 socket fd 读写 */
    mbedtls_ssl_set_bio(&tls->ssl, socket,
                        esp32_tls_bio_send,
                        esp32_tls_bio_recv,
                        NULL);

    if (ca_cert != NULL) {
        int ret = mbedtls_x509_crt_parse(&tls->cacert,
                                          (const unsigned char *)ca_cert,
                                          strlen(ca_cert) + 1);
        if (ret < 0) {
            ESP_LOGE(TAG, "CA cert parse failed: -0x%x", (unsigned int)(-ret));
            return -1;
        }
        mbedtls_ssl_conf_ca_chain(&tls->conf, &tls->cacert, NULL);
        mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
        ESP_LOGI(TAG, "TLS VERIFY_REQUIRED (CA cert loaded, %d bytes)",
                 (int)strlen(ca_cert));
    } else {
        mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_NONE);
        ESP_LOGW(TAG, "TLS VERIFY_NONE (no CA cert)");
    }

    return 0;
}

static int esp32_tls_handshake_step(convai_tls_t *tls,
                                    int *want_flags,
                                    int *done) {
    if (tls == NULL || want_flags == NULL || done == NULL) return -1;
    *want_flags = 0;
    *done = 0;

    int ret = mbedtls_ssl_handshake(&tls->ssl);
    if (ret == 0) {
        tls->connected = 1;
        *done = 1;
        return 0;
    }
    if (ret == MBEDTLS_ERR_SSL_WANT_READ) {
        *want_flags = CONVAI_POLL_READ;
        return 0;
    }
    if (ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
        *want_flags = CONVAI_POLL_WRITE;
        return 0;
    }
    ESP_LOGE(TAG, "TLS handshake failed: -0x%x", (unsigned int)(-ret));
    return -1;
}

static int esp32_tls_read(convai_tls_t *tls,
                          uint8_t *buf, size_t len,
                          size_t *nread) {
    if (nread) *nread = 0;
    if (tls == NULL || buf == NULL) return -1;

    int ret = mbedtls_ssl_read(&tls->ssl, buf, len);
    if (ret < 0) {
        if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
            ret == MBEDTLS_ERR_SSL_WANT_WRITE)
            return 0;  /* would-block */
        return -1;
    }
    if (ret == 0) return -1;  /* peer closed */
    if (nread) *nread = (size_t)ret;
    return 0;
}

static int esp32_tls_write(convai_tls_t *tls,
                           const uint8_t *buf, size_t len,
                           size_t *nwrite) {
    if (nwrite) *nwrite = 0;
    if (tls == NULL || buf == NULL) return -1;

    int ret = mbedtls_ssl_write(&tls->ssl, buf, len);
    if (ret < 0) {
        if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
            ret == MBEDTLS_ERR_SSL_WANT_WRITE)
            return 0;
        return -1;
    }
    if (nwrite) *nwrite = (size_t)ret;
    return 0;
}

static int esp32_tls_close(convai_tls_t *tls) {
    if (tls == NULL) return -1;
    mbedtls_ssl_close_notify(&tls->ssl);
    tls->connected = 0;
    return 0;
}

/* ===================================================================
 *  Misc — Logging / Device ID / UUID / Network
 * =================================================================== */

static void esp32_log(int level,
                      const char *file, int line,
                      const char *fmt, ...) {
    char buf[256];
    va_list args;

    /* 时间前缀 */
    uint64_t now_ms = esp32_get_time_ms();
    uint32_t sec = (uint32_t)(now_ms / 1000);
    uint32_t ms  = (uint32_t)(now_ms % 1000);
    int pos = snprintf(buf, sizeof(buf), "[%" PRIu32 ".%03" PRIu32 "] [%c] [%s:%d] ",
                       sec, ms,
                       level == 0 ? 'E' : level == 1 ? 'W'
                                   : level == 2 ? 'I' : 'D',
                       file ? file : "???", line);

    va_start(args, fmt);
    vsnprintf(buf + pos, sizeof(buf) - pos - 2, fmt, args);
    va_end(args);

    int len = strlen(buf);
    if (len < (int)sizeof(buf) - 1) {
        buf[len]     = '\n';
        buf[len + 1] = '\0';
    }
    printf("%s", buf);
}

static int esp32_device_id(char *buf, size_t len) {
    if (buf == NULL || len < 18) return -1;

    uint8_t mac[6];
    esp_err_t ret = esp_efuse_mac_get_default(mac);
    if (ret == ESP_OK) {
        /* 检查是否有效 MAC (非全 0 或全 F) */
        int valid = 0;
        for (int i = 0; i < 6; i++) {
            if (mac[i] != 0x00 && mac[i] != 0xFF) { valid = 1; break; }
        }
        if (valid) {
            snprintf(buf, len, "%02X%02X%02X%02X%02X%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            return (int)strlen(buf);
        }
    }

    snprintf(buf, len, "esp32s3-device");
    return (int)strlen(buf);
}

static int esp32_random(uint8_t *buf, size_t len) {
    return esp32_fill_random(buf, len);
}

static int esp32_uuid(char *buf, size_t size) {
    if (buf == NULL || size == 0) return -1;
    /* 基于 MAC + 随机数生成伪 UUID */
    uint8_t rnd[16];
    esp_fill_random(rnd, sizeof(rnd));
    snprintf(buf, size,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             rnd[0], rnd[1], rnd[2], rnd[3],
             rnd[4], rnd[5],
             rnd[6], rnd[7],
             rnd[8], rnd[9],
             rnd[10], rnd[11], rnd[12], rnd[13], rnd[14], rnd[15]);
    return (int)strlen(buf);
}

static int esp32_info(char *buf, size_t size) {
    if (buf == NULL || size == 0) return -1;
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    snprintf(buf, size, "esp32s3-freertos-idf%d", chip_info.revision);
    return (int)strlen(buf);
}

static int esp32_network_available(void) {
    /* 检查默认 netif 是否已连接 */
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) return 0;
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(netif, &ip) != ESP_OK) return 0;
    return ip.ip.addr != 0 ? 1 : 0;
}

static int esp32_network_get_type(char *buf, size_t size) {
    if (buf == NULL || size == 0) return -1;
    snprintf(buf, size, "wifi");
    return (int)strlen(buf);
}

/* ===================================================================
 *  Platform struct — 注册到 SDK
 * =================================================================== */

static const convai_platform_t g_convai_platform = {
    .abi_version = CONVAI_ABI_VERSION,
    ._reserved   = 0,
    .osal = {
        .malloc        = esp32_malloc,
        .free          = esp32_free,
        .get_time_ms   = esp32_get_time_ms,
        .sleep_ms      = esp32_sleep_ms,
        .get_tick_ms   = esp32_get_tick_ms,
        .mutex_create  = esp32_mutex_create,
        .mutex_destroy = esp32_mutex_destroy,
        .mutex_lock    = esp32_mutex_lock,
        .mutex_unlock  = esp32_mutex_unlock,
        .thread_create = esp32_thread_create,
        .thread_join   = esp32_thread_join,
        .thread_destroy = esp32_thread_destroy,
        .fill_random   = esp32_fill_random,
        .strdup        = esp32_strdup,
    },
    .netal = {
        .socket_create       = esp32_socket_create,
        .socket_destroy      = esp32_socket_destroy,
        .socket_connect      = esp32_socket_connect,
        .socket_send         = esp32_socket_send,
        .socket_recv         = esp32_socket_recv,
        .socket_set_nonblock = esp32_socket_set_nonblock,
        .socket_is_connected = esp32_socket_is_connected,
        .socket_get_fd       = esp32_socket_get_fd,
        .socket_poll         = esp32_socket_poll,
        .socket_get_error    = esp32_socket_get_error,
    },
    .tlsal = {
        .tls_create        = esp32_tls_create,
        .tls_destroy       = esp32_tls_destroy,
        .tls_connect       = esp32_tls_connect,
        .tls_handshake_step = esp32_tls_handshake_step,
        .tls_read           = esp32_tls_read,
        .tls_write          = esp32_tls_write,
        .tls_close          = esp32_tls_close,
    },
    .misc = {
        .log              = esp32_log,
        .device_id        = esp32_device_id,
        .random           = esp32_random,
        .uuid             = esp32_uuid,
        .info             = esp32_info,
        .network_available = esp32_network_available,
        .network_get_type  = esp32_network_get_type,
    },
};

int convai_platform_esp32_init(void) {
    ESP_LOGI(TAG, "Registering ESP32 platform HAL (ABI 0x%04x)",
             CONVAI_ABI_VERSION);
    return convai_platform_init(&g_convai_platform);
}
