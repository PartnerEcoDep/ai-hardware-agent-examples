/**
 * @file convai_platform_win.c
 * @brief Windows simulator platform abstraction implementation.
 *
 * 与 convai_platform_ws63.c 同构:
 *  - OSAL: goldie_osal (由 libwinvm.a 提供的 Windows 实现)
 *  - NetAL: Winsock2 (非阻塞 connect + select poll)
 *  - TLSAL: mbedTLS (libs/win10 预编译库)
 */

#include "convai_platform_win.h"
#include "goldie_osal.h"

/* winsock2.h 必须先于 windows.h 包含，避免旧版 winsock.h 被拉入导致符号冲突。 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600  /* Vista+: GetTickCount64、inet_ntop 需要 */
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ===== Opaque type definitions (must match SDK internal layout) ===== */
struct convai_mutex_s {
    goldie_mutex mutex;
};

struct convai_thread_s {
    void *handle;
    goldie_sem exit_sem;  /* notify when thread exits */
    int exited;           /* exit flag */
};

struct convai_socket_s {
    mbedtls_net_context net;  /* 持有 winsock fd, TLS BIO 复用 */
};

struct convai_tls_s {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    mbedtls_x509_crt cacert;
    convai_socket_t *sock;  /* 持有 socket 引用 */
    int connected;
};

/* ===== Winsock 惰性初始化 (WSAStartup 只需调用一次) ===== */
static int win_wsa_init(void)
{
    static int wsa_started = 0;
    WSADATA wsa_data;

    if (wsa_started) {
        return 0;
    }
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        printf("[E] win_net: WSAStartup failed\n");
        return -1;
    }
    wsa_started = 1;
    return 0;
}

/* ===== OSAL – Memory ===== */
static void *win_malloc(size_t size) {
    return goldie_malloc(size);
}

static void win_free(void *ptr) {
    goldie_free(ptr);
}

/* ===== OSAL – Time ===== */

/* Wall-clock epoch ms (winvm 的 goldie_gettimeofday 映射到系统实时时钟). */
uint64_t win_get_time_ms(void) {
    goldie_timeval tv;
    goldie_gettimeofday(&tv);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
}

static void win_sleep_ms(uint32_t ms) {
    goldie_msleep((int)ms);
}

/* Monotonic millisecond tick for interval/timeout math (PING/reconnect/stop).
 * GetTickCount64 单调递增、不受系统校时影响 — 对应 ws63 用 sys_now() 的设计意图. */
static uint64_t win_tick_ms(void) {
    return (uint64_t)GetTickCount64();
}

/* ===== OSAL – Mutex ===== */
static int win_mutex_create(convai_mutex_t **mutex) {
    if (mutex == NULL) return -1;
    convai_mutex_t *m = (convai_mutex_t*)goldie_malloc(sizeof(*m));
    if (m == NULL) return -1;
    goldie_mutex_init(&m->mutex);
    *mutex = m;
    return 0;
}

static void win_mutex_destroy(convai_mutex_t *mutex) {
    if (mutex == NULL) return;
    goldie_mutex_destroy(&mutex->mutex);
    goldie_free(mutex);
}

static void win_mutex_lock(convai_mutex_t *mutex) {
    if (mutex == NULL) return;
    goldie_mutex_lock(&mutex->mutex);
}

static void win_mutex_unlock(convai_mutex_t *mutex) {
    if (mutex == NULL) return;
    goldie_mutex_unlock(&mutex->mutex);
}

/* ===== OSAL – Thread ===== */
static int win_thread_wrapper(void *data) {
    void **args = (void **)data;
    convai_thread_func_t func = (convai_thread_func_t)args[0];
    void *arg = args[1];
    convai_thread_t *thread = (convai_thread_t *)args[2];
    goldie_free(args);
    if (func) func(arg);
    /* notify thread exit for join/destroy */
    if (thread) {
        thread->exited = 1;
        goldie_sem_post(&thread->exit_sem);
    }
    return 0;
}

static int win_thread_create(convai_thread_t **thread,
                             convai_thread_func_t func, void *arg,
                             const char *name, size_t stack_size, int priority) {
    if (thread == NULL || func == NULL) return -1;

    convai_thread_t *t = (convai_thread_t*)goldie_malloc(sizeof(*t));
    if (t == NULL) return -1;
    memset(t, 0, sizeof(*t));

    void **args = (void**)goldie_malloc(3 * sizeof(void*));
    if (args == NULL) {
        goldie_free(t);
        return -1;
    }
    args[0] = (void*)func;
    args[1] = arg;
    args[2] = (void*)t;

    unsigned int ss = stack_size > 0 ? stack_size : 4096;
    const char *n = name ? name : "convai";

    goldie_sem_init(&t->exit_sem);

    t->handle = goldie_thread_create(
        (goldie_thread_handler)win_thread_wrapper, args, n, ss);
    if (t->handle == NULL) {
        goldie_sem_destroy(&t->exit_sem);
        goldie_free(args);
        goldie_free(t);
        return -1;
    }

    /* 仅当调用方显式指定优先级时才转发 (priority != 0);
     * 0 表示使用 goldie 默认调度, 与 ws63 行为一致. */
    if (priority != 0) {
        goldie_thread_set_priority(t->handle, (unsigned int)priority);
    }

    *thread = t;
    return 0;
}

/* wait for thread to call exit_sem_post */
static void win_thread_join(convai_thread_t *thread) {
    if (thread == NULL) return;
    goldie_sem_wait(&thread->exit_sem);
}

/* safe destroy: wait if thread not exited, then release resources */
static void win_thread_destroy(convai_thread_t *thread) {
    if (thread == NULL) return;
    if (!thread->exited) {
        goldie_sem_wait(&thread->exit_sem);
    }
    if (thread->handle) {
        goldie_thread_destroy(thread->handle);
        thread->handle = NULL;
    }
    goldie_sem_destroy(&thread->exit_sem);
    goldie_free(thread);
}

/* ===== OSAL – Misc ===== */
static int win_fill_random(uint8_t *buf, size_t len) {
    if (buf == NULL) return -1;
    for (size_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(rand() & 0xFF);
    }
    return 0;
}

static char *win_strdup(const char *s) {
    if (s == NULL) return NULL;
    size_t len = strlen(s) + 1;
    char *dup = (char*)goldie_malloc(len);
    if (dup) memcpy(dup, s, len);
    return dup;
}

/* ===== NetAL – Socket ===== */
static int win_socket_create(convai_socket_t **sock) {
    if (sock == NULL) return -1;
    if (win_wsa_init() != 0) return -1;
    *sock = (convai_socket_t*)goldie_malloc(sizeof(**sock));
    if (*sock == NULL) return -1;
    memset(*sock, 0, sizeof(**sock));
    mbedtls_net_init(&(*sock)->net);
    return 0;
}

static int win_socket_destroy(convai_socket_t *sock) {
    if (sock == NULL) return 0;
    /* 显式关闭 winsock fd, 与 ws63 显式 lwip_close 同理:
     * 确保 fd 及其收发缓冲及时释放, 重连场景不泄漏. */
    if (sock->net.fd >= 0) {
        closesocket((SOCKET)sock->net.fd);
        sock->net.fd = -1;
    }
    mbedtls_net_free(&sock->net);
    goldie_free(sock);
    return 0;
}

/* Non-blocking TCP connect: resolve host, create a socket, set it non-blocking,
 * and initiate connect (returns immediately; WSAEWOULDBLOCK is the expected "in
 * progress" result). The fd is stored in sock->net.fd so the SDK poll loop can
 * drive it (poll WRITE + getsockopt SO_ERROR) and the TLS BIO can use it.
 * Returns 0 if connect was initiated, <0 on error. */
static int win_socket_connect(convai_socket_t *sock, const char *host, uint16_t port) {
    if (sock == NULL || host == NULL) return -1;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", port);

    printf("[I] win_net: resolving %s:%s ...\n", host, port_str);
    struct addrinfo *res = NULL;
    int gai = getaddrinfo(host, port_str, &hints, &res);
    if (gai != 0 || res == NULL) {
        printf("[E] win_net: DNS resolve failed for %s (gai=%d)\n", host, gai);
        return -1;
    }

    {
        char addr_str[INET_ADDRSTRLEN] = {0};
        struct sockaddr_in *sin = (struct sockaddr_in *)res->ai_addr;
        const char *s = inet_ntop(AF_INET, &sin->sin_addr, addr_str, sizeof(addr_str));
        printf("[I] win_net: resolved %s -> %s (port=%u)\n", host,
               s ? addr_str : "?", port);
    }

    SOCKET fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == INVALID_SOCKET) {
        printf("[E] win_net: socket failed (WSA error=%d)\n", WSAGetLastError());
        freeaddrinfo(res);
        return -1;
    }
    printf("[I] win_net: socket created fd=%d\n", (int)fd);

    /* Non-blocking mode before connect so connect returns WSAEWOULDBLOCK. */
    u_long nb = 1UL;
    ioctlsocket(fd, FIONBIO, &nb);

    int cr = connect(fd, res->ai_addr, (int)res->ai_addrlen);
    int wsa_err = WSAGetLastError();
    freeaddrinfo(res);

    if (cr == SOCKET_ERROR && wsa_err != WSAEWOULDBLOCK && wsa_err != WSAEINPROGRESS) {
        printf("[E] win_net: connect failed (WSA error=%d)\n", wsa_err);
        closesocket(fd);
        return -1;
    }

    printf("[I] win_net: connect initiated (fd=%d, %s)\n", (int)fd,
           (cr == 0) ? "connected" : "WSAEWOULDBLOCK");

    /* Connect initiated (cr == 0 already connected, or WSAEWOULDBLOCK pending).
     * Store the fd in the mbedtls_net_context so the TLS BIO (which calls
     * mbedtls_net_send/recv) and socket_poll both use it. */
    sock->net.fd = (int)fd;
    return 0;
}

static int win_socket_send(convai_socket_t *sock, const uint8_t *buf, size_t len, size_t *sent) {
    if (sent) *sent = 0;
    if (sock == NULL || buf == NULL) return -1;
    int ret = mbedtls_net_send(&sock->net, buf, len);
    if (ret < 0) {
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
            return 0;  /* would-block: 0 bytes transferred, ret 0 */
        return -1;
    }
    if (sent) *sent = (size_t)ret;
    return 0;
}

static int win_socket_recv(convai_socket_t *sock, uint8_t *buf, size_t len, size_t *recvd) {
    if (recvd) *recvd = 0;
    if (sock == NULL || buf == NULL) return -1;
    int ret = mbedtls_net_recv(&sock->net, buf, len);
    if (ret == 0)
        return -1;  /* peer closed */
    if (ret < 0) {
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
            return 0;  /* would-block: 0 bytes transferred, ret 0 */
        return -1;  /* error */
    }
    if (recvd) *recvd = (size_t)ret;
    return 0;
}

static int win_socket_set_nonblock(convai_socket_t *sock, int non_block) {
    if (sock == NULL) return -1;
    if (sock->net.fd < 0) return -1;
    u_long mode = non_block ? 1UL : 0UL;
    return ioctlsocket((SOCKET)sock->net.fd, FIONBIO, &mode) == 0 ? 0 : -1;
}

static int win_socket_is_connected(convai_socket_t *sock) {
    if (sock == NULL) return 0;
    return sock->net.fd >= 0 ? 1 : 0;
}

static int win_socket_get_fd(convai_socket_t *sock) {
    if (sock == NULL) return -1;
    return sock->net.fd;
}

static int win_socket_get_error(convai_socket_t *sock) {
    if (sock == NULL || sock->net.fd < 0) return -1;
    int so_error = 0;
    int optlen = sizeof(so_error);
    if (getsockopt((SOCKET)sock->net.fd, SOL_SOCKET, SO_ERROR, (char *)&so_error, &optlen) != 0)
        return -1;
    return so_error;  /* 0 = no error, positive = WSA error code */
}

/* ===== TLSAL – mbedTLS implementation (platform layer; SDK is mbedtls-free) ===== */

/* BIO callbacks for mbedTLS (internal). Call mbedtls_net_* directly so that on a
 * non-blocking socket a would-block surfaces as MBEDTLS_ERR_SSL_WANT_WRITE/WANT_READ
 * (the win_socket_* wrappers collapse it to -1, which would be a fatal error). */
static int win_tls_bio_send(void *ctx, const unsigned char *buf, size_t len)
{
    convai_socket_t *sock = (convai_socket_t *)ctx;
    if (sock == NULL) return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    return mbedtls_net_send(&sock->net, buf, len);
}

static int win_tls_bio_recv(void *ctx, unsigned char *buf, size_t len)
{
    convai_socket_t *sock = (convai_socket_t *)ctx;
    if (sock == NULL) return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    return mbedtls_net_recv(&sock->net, buf, len);
}

static int win_tls_create(convai_tls_t **tls)
{
    if (tls == NULL) return -1;

    convai_tls_t *t = (convai_tls_t *)goldie_malloc(sizeof(*t));
    if (t == NULL) return -1;
    memset(t, 0, sizeof(*t));

    mbedtls_ssl_init(&t->ssl);
    mbedtls_ssl_config_init(&t->conf);
    mbedtls_ctr_drbg_init(&t->ctr_drbg);
    mbedtls_entropy_init(&t->entropy);
    mbedtls_x509_crt_init(&t->cacert);

    // Seed RNG
    int ret = mbedtls_ctr_drbg_seed(&t->ctr_drbg, mbedtls_entropy_func,
                                     &t->entropy,
                                     (const unsigned char *)"convai_tls", 10);
    if (ret != 0) goto tls_create_fail;

    // Config SSL defaults
    ret = mbedtls_ssl_config_defaults(&t->conf,
                                      MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) goto tls_create_fail;

    mbedtls_ssl_conf_rng(&t->conf, mbedtls_ctr_drbg_random, &t->ctr_drbg);
    mbedtls_ssl_conf_authmode(&t->conf, MBEDTLS_SSL_VERIFY_NONE);
    /* Silence mbedTLS debug output which floods the log and burns CPU
     * during non-blocking reads. */
    mbedtls_ssl_conf_dbg(&t->conf, NULL, NULL);

    ret = mbedtls_ssl_setup(&t->ssl, &t->conf);
    if (ret != 0) goto tls_create_fail;

    *tls = t;
    return 0;

tls_create_fail:
    /* Release every mbedTLS sub-object that was init'd, in reverse order.
     * mbedtls_*_free are safe on a context that was init'd but not fully set up. */
    mbedtls_ssl_free(&t->ssl);
    mbedtls_ssl_config_free(&t->conf);
    mbedtls_ctr_drbg_free(&t->ctr_drbg);
    mbedtls_entropy_free(&t->entropy);
    mbedtls_x509_crt_free(&t->cacert);
    goldie_free(t);
    return -1;
}

static int win_tls_destroy(convai_tls_t *tls)
{
    if (tls == NULL) return 0;
    mbedtls_ssl_free(&tls->ssl);
    mbedtls_ssl_config_free(&tls->conf);
    mbedtls_ctr_drbg_free(&tls->ctr_drbg);
    mbedtls_entropy_free(&tls->entropy);
    mbedtls_x509_crt_free(&tls->cacert);
    goldie_free(tls);
    return 0;
}

/* Setup only: bind the socket + set hostname + load CA cert (if provided).
 * The handshake itself is driven incrementally by win_tls_handshake_step
 * (called from the SDK poll loop) so the SDK's IO thread is never blocked.
 * @param ca_cert  PEM CA cert; non-NULL → parse + VERIFY_REQUIRED,
 *                  NULL → VERIFY_NONE (skip verification). */
static int win_tls_connect(convai_tls_t *tls, void *sock, const char *host,
                           const char *ca_cert)
{
    if (tls == NULL || sock == NULL || host == NULL) return -1;

    convai_socket_t *socket = (convai_socket_t *)sock;
    tls->sock = socket;

    mbedtls_ssl_set_hostname(&tls->ssl, host);
    mbedtls_ssl_set_bio(&tls->ssl, socket,
                        win_tls_bio_send, win_tls_bio_recv, NULL);

    if (ca_cert != NULL) {
        /* Load the CA cert and require server certificate verification. */
        int ret = mbedtls_x509_crt_parse(&tls->cacert,
                                          (const unsigned char *)ca_cert,
                                          strlen(ca_cert) + 1);
        if (ret < 0) {
            printf("[E] win_tls: CA cert parse failed: -0x%x\n", (unsigned int)(-ret));
            return -1;
        }
        mbedtls_ssl_conf_ca_chain(&tls->conf, &tls->cacert, NULL);
        mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
        printf("[I] VERIFY_REQUIRED (CA cert loaded, %d bytes)\n",
               (int)strlen(ca_cert));
    } else {
        /* No CA cert — skip verification (test/custom environments). */
        mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_NONE);
        printf("[W] VERIFY_NONE (no CA cert provided)\n");
    }

    return 0;
}

/* One non-blocking step of the TLS handshake.
 *   *done=1            : handshake complete
 *   *want_flags=POLL_* : need to poll socket in that direction, then re-call
 *   return <0           : fatal handshake error */
static int win_tls_handshake_step(convai_tls_t *tls, int *want_flags, int *done)
{
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
    printf("[E] win_tls: handshake failed: -0x%x\n", (unsigned int)(-ret));
    return -1;
}

static int win_tls_read(convai_tls_t *tls, uint8_t *buf, size_t len, size_t *nread)
{
    if (nread) *nread = 0;
    if (tls == NULL || buf == NULL) return -1;

    int ret = mbedtls_ssl_read(&tls->ssl, (unsigned char *)buf, (int)len);
    if (ret < 0) {
        if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
            ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            return 0;  /* would-block: 0 bytes, ret 0 */
        }
        return -1;  /* error or peer-closed */
    }
    if (ret == 0) {
        return -1;  /* peer closed the connection */
    }
    if (nread) *nread = (size_t)ret;
    return 0;
}

static int win_tls_write(convai_tls_t *tls, const uint8_t *buf, size_t len, size_t *nwrite)
{
    if (nwrite) *nwrite = 0;
    if (tls == NULL || buf == NULL) return -1;

    int ret = mbedtls_ssl_write(&tls->ssl, (const unsigned char *)buf, (int)len);
    if (ret < 0) {
        if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
            ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            return 0;
        }
        return -1;
    }
    if (nwrite) *nwrite = (size_t)ret;
    return 0;
}

static int win_tls_close(convai_tls_t *tls)
{
    if (tls == NULL) return -1;
    mbedtls_ssl_close_notify(&tls->ssl);
    tls->connected = 0;
    return 0;
}

/* ===== NetAL: socket_poll (required by the poll architecture) =====
 *
 * Windows select() honours a real timeout (unlike WS63 lwip_select), so a single
 * select call suffices — no slice-polling workaround needed, zero CPU spin.
 * exceptfds is mapped to POLL_WRITE so a failed non-blocking connect still
 * wakes the SDK loop, which then reads SO_ERROR via socket_get_error. */
static int win_socket_poll(convai_socket_t *sock, int events, int *revents, int timeout_ms) {
    if (sock == NULL || revents == NULL) return -1;
    *revents = 0;

    int fd = sock->net.fd;
    if (fd < 0) return -1;

    fd_set rfds, wfds, efds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);
    if (events & CONVAI_POLL_READ)  FD_SET((SOCKET)fd, &rfds);
    if (events & CONVAI_POLL_WRITE) FD_SET((SOCKET)fd, &wfds);
    FD_SET((SOCKET)fd, &efds);

    struct timeval tv;
    struct timeval *ptv = NULL;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (long)(timeout_ms % 1000) * 1000L;
        ptv = &tv;
    }

    /* Windows select() ignores the nfds argument; pass 0. */
    int ret = select(0, &rfds, &wfds, &efds, ptv);
    if (ret == SOCKET_ERROR) {
        printf("[E] win_net: select failed fd=%d WSA error=%d\n", fd, WSAGetLastError());
        return -1;
    }
    if (FD_ISSET((SOCKET)fd, &rfds)) *revents |= CONVAI_POLL_READ;
    if (FD_ISSET((SOCKET)fd, &wfds)) *revents |= CONVAI_POLL_WRITE;
    if (FD_ISSET((SOCKET)fd, &efds)) *revents |= CONVAI_POLL_WRITE;
    return 0;  /* ret==0: timed out, no event */
}

/* ===== Misc ===== */
static void win_log(int level, const char *file, int line, const char *fmt, ...) {
    char buf[256];
    va_list args;
    uint64_t now_ms = win_tick_ms();
    uint32_t sec = (uint32_t)(now_ms / 1000);
    uint32_t ms = (uint32_t)(now_ms % 1000);
    int pos = snprintf(buf, sizeof(buf), "[%u.%03u] [%c] [%s:%d] ",
                       sec, ms,
                       level == 0 ? 'E' : level == 1 ? 'W' : level == 2 ? 'I' : 'D',
                       file ? file : "???", line);
    va_start(args, fmt);
    vsnprintf(buf + pos, sizeof(buf) - pos - 2, fmt, args);
    va_end(args);
    int len = strlen(buf);
    if (len < (int)sizeof(buf) - 1) {
        buf[len] = '\n';
        buf[len + 1] = '\0';
    }
    printf("%s", buf);
}

int win_device_id(char *buf, size_t len) {
    if (buf == NULL || len < 16) return -1;

    /* 取 NetBIOS 主机名 (Windows 10+ 装机随机生成 15 字符名, 如
     * "DESKTOP-ABC1234")。比固定串 "goldieos-sim" 更有区分度, 且语义上
     * 接近 ws63 用 WiFi MAC 做设备标识的意图。GetComputerNameA 由 kernel32
     * 导出, Windows 恒链接, 无需在 CMake 加库。 */
    DWORD name_len = (DWORD)len;
    if (GetComputerNameA(buf, &name_len)) {
        return (int)strlen(buf);
    }

    /* 兜底: 取名失败时退回原固定串行为。 */
    snprintf(buf, len, "goldieos-sim");
    return (int)strlen(buf);
}

static int win_random(uint8_t *buf, size_t len) {
    return win_fill_random(buf, len);
}

static int win_uuid(char *buf, size_t size) {
    if (buf == NULL || size == 0) return -1;
    snprintf(buf, size, "00000000-0000-0000-0000-000000000000");
    return (int)strlen(buf);
}

static int win_info(char *buf, size_t size) {
    if (buf == NULL || size == 0) return -1;
    snprintf(buf, size, "win-simulator");
    return (int)strlen(buf);
}

static int win_network_available(void) {
    return 1;
}

static int win_network_get_type(char *buf, size_t size) {
    if (buf == NULL || size == 0) return -1;
    snprintf(buf, size, "ethernet");
    return (int)strlen(buf);
}

/* ===== Platform structure instance ===== */
static const convai_platform_t g_convai_platform = {
    .abi_version = CONVAI_ABI_VERSION,
    ._reserved = 0,
    .osal = {
        .malloc = win_malloc,
        .free = win_free,
        .get_time_ms = win_get_time_ms,
        .sleep_ms = win_sleep_ms,
        .get_tick_ms = win_tick_ms,
        .mutex_create = win_mutex_create,
        .mutex_destroy = win_mutex_destroy,
        .mutex_lock = win_mutex_lock,
        .mutex_unlock = win_mutex_unlock,
        .thread_create = win_thread_create,
        .thread_join = win_thread_join,
        .thread_destroy = win_thread_destroy,
        .fill_random = win_fill_random,
        .strdup = win_strdup,
    },
    .netal = {
        .socket_create = win_socket_create,
        .socket_destroy = win_socket_destroy,
        .socket_connect = win_socket_connect,
        .socket_send = win_socket_send,
        .socket_recv = win_socket_recv,
        .socket_set_nonblock = win_socket_set_nonblock,
        .socket_is_connected = win_socket_is_connected,
        .socket_get_fd = win_socket_get_fd,
        .socket_poll = win_socket_poll,
        .socket_get_error = win_socket_get_error,
    },
    .tlsal = {
        .tls_create = win_tls_create,
        .tls_destroy = win_tls_destroy,
        .tls_connect = win_tls_connect,
        .tls_handshake_step = win_tls_handshake_step,
        .tls_read = win_tls_read,
        .tls_write = win_tls_write,
        .tls_close = win_tls_close,
    },
    .misc = {
        .log = win_log,
        .device_id = win_device_id,
        .random = win_random,
        .uuid = win_uuid,
        .info = win_info,
        .network_available = win_network_available,
        .network_get_type = win_network_get_type,
    },
};

int convai_platform_win_init(void) {
    /* SDK 内部 convai_tick_ms() 调用 convai_platform_get_osal() 后用 cltq 将返回
     * 的指针截断为 32 位（SDK ABI bug，在 32 位 ws63 上无害，在 64 位 Windows 上
     * 丢失高 32 位导致 SIGSEGV）。若 g_convai_platform 被链接器放在高地址（如
     * 0x7ff6_156933e0），截断后的 0x156933e0 是无效地址。
     *
     * 规避方法：用 VirtualAlloc 在低 4GB 地址空间分配一份 platform 结构体副本，
     * 传给 convai_platform_init。低 4GB 地址截断为 32 位后仍有效，bug 被绕过。
     * 内存永久存活（与进程同生命周期），无需释放。
     */
    static convai_platform_t *p = NULL;
    if (p == NULL) {
        /* VirtualAlloc(NULL,...) 在 64 位 Windows 上可能返回高 4GB 外的地址
         * (如 0x1_5fc80000)，cltq 截断后仍无效。必须在低 4GB 内显式占位：
         * 从 0x10000000 起，每次跳 4MB 向上扫描，直至成功 reserve+commit。 */
        const uintptr_t kLowCeiling = 0x100000000ULL; /* 4GB 边界 */
        const uintptr_t kStep       = 0x400000;       /* 4MB 步进 */
        for (uintptr_t base = 0x10000000; base + sizeof(convai_platform_t) <= kLowCeiling; base += kStep) {
            p = (convai_platform_t *)VirtualAlloc((void *)base, sizeof(convai_platform_t),
                                                  MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (p != NULL) {
                break; /* 命中低 4GB 内的空闲区域 */
            }
        }
        if (p == NULL) {
            return -1;
        }
        memcpy(p, &g_convai_platform, sizeof(convai_platform_t));
    }
    return convai_platform_init(p);
}
