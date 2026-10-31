/**
 * @file convai_platform_ws63.c
 * @brief WS63 platform abstraction implementation.
 *
 * 基于goldieos项目架构实现:
 *  - OSAL: goldie_osal (LiteOS API封装)
 *  - NetAL: lwIP sockets
 *  - TLSAL: mbedTLS
 */

#include "convai_platform_ws63.h"
#include "delivery_diag.h"
#include "ws63_config.h"
#ifndef WS_AI_CA_CERT_PEM
#define WS_AI_CA_CERT_PEM ""
#endif
#include "goldie_osal.h"
#include "services/ntp/ntp_service.h"
#include "core/service_manager.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"
#include "mbedtls/platform_time.h"

/* Forward-declare poll_table for lwip/sockets.h (WS63 lwip header uses it
 * in a function prototype without including poll.h). */
typedef struct poll_table poll_table;

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"  /* sys_now() — monotonic ms systick */

#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define NTP_SERVICE_INDEX 7


/* ===== Opaque type definitions (must match SDK internal layout) ===== */
struct convai_mutex_s {
    goldie_mutex mutex;
};

struct convai_thread_s {
    void *handle;
    goldie_sem exit_sem;  /* notify when thread exits */
};

struct convai_socket_s {
    mbedtls_net_context net;
    ws63_delivery_diag_t diag;
};

struct convai_tls_s {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    mbedtls_x509_crt cacert;
    convai_socket_t *sock;  /* 持有 socket 引用 */
    int connected;
    uint32_t diag_read_report, diag_write_report;
};

/* Manual implementation of timegm for platforms that don't have it (like WS63) */
static time_t my_timegm(const struct tm *tm) {
    static const int mon_lengths[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    int year = tm->tm_year + 1900 - 1970;
    if (year < 0) return (time_t)-1;
    
    time_t days = 0;
    
    for (int y = 1970; y < tm->tm_year + 1900; y++) {
        days += 365;
        if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) {
            days++;
        }
    }
    
    for (int m = 0; m < tm->tm_mon; m++) {
        days += mon_lengths[m];
        if (m == 1 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
            days++;
        }
    }
    
    days += tm->tm_mday - 1;
    
    time_t seconds = days * 24 * 3600;
    seconds += tm->tm_hour * 3600;
    seconds += tm->tm_min * 60;
    seconds += tm->tm_sec;
    
    return seconds;
}

/* ===== OSAL – Memory ===== */
static void *ws63_malloc(size_t size) {
    return goldie_malloc(size);
}

static void ws63_free(void *ptr) {
    goldie_free(ptr);
}

/* ===== OSAL – Time ===== */

extern void* get_service(int service_index);
NTPService* ntp_service = NULL;
uint64_t ws63_get_time_ms(void) {
    uint64_t timestamp_s = 0;
    
    struct tm tm_now;
    if (!ntp_service) {
        ntp_service = get_service(NTP_SERVICE_INDEX);
    }

    if (ntp_service && ntp_service->get_time(&tm_now) == 0) {
        time_t t = my_timegm(&tm_now);
        
        timestamp_s = (uint64_t)t - 86400;
    }
    return timestamp_s * 1000;
}

static void ws63_sleep_ms(uint32_t ms) {
    goldie_msleep((int)ms);
}

static mbedtls_time_t ws63_tls_time(mbedtls_time_t *out) {
    mbedtls_time_t now = (mbedtls_time_t)(ws63_get_time_ms() / 1000);
    if (out) *out = now;
    return now;
}

/* Monotonic millisecond tick for interval/timeout math (PING/reconnect/stop).
 * ws63_get_time_ms is NTP wall-clock with only SECOND resolution and it jumps on
 * NTP sync — unusable for sub-second PING timing (a 30s PING would actually fire
 * at ~31s, racing a ~30s server idle timeout, and NTP jumps corrupt the math).
 * sys_now() is lwIP's systick-based monotonic ms counter — millisecond-accurate
 * and non-jumping. */
static uint64_t ws63_tick_ms(void) {
    return (uint64_t)sys_now();
}

/* ===== OSAL – Mutex ===== */
static int ws63_mutex_create(convai_mutex_t **mutex) {
    if (mutex == NULL) return -1;
    convai_mutex_t *m = (convai_mutex_t*)goldie_malloc(sizeof(*m));
    if (m == NULL) return -1;
    goldie_mutex_init(&m->mutex);
    *mutex = m;
    return 0;
}

static void ws63_mutex_destroy(convai_mutex_t *mutex) {
    if (mutex == NULL) return;
    goldie_mutex_destroy(&mutex->mutex);
    goldie_free(mutex);
}

static void ws63_mutex_lock(convai_mutex_t *mutex) {
    if (mutex == NULL) return;
    goldie_mutex_lock(&mutex->mutex);
}

static void ws63_mutex_unlock(convai_mutex_t *mutex) {
    if (mutex == NULL) return;
    goldie_mutex_unlock(&mutex->mutex);
}

/* ===== OSAL – Thread ===== */
static int ws63_thread_wrapper(void *data) {
    void **args = (void **)data;
    convai_thread_func_t func = (convai_thread_func_t)args[0];
    void *arg = args[1];
    convai_thread_t *thread = (convai_thread_t *)args[2];
    goldie_free(args);
    if (func) func(arg);
    /* notify thread exit for join/destroy */
    if (thread) {
        goldie_sem_post(&thread->exit_sem);
    }
    return 0;
}

static int ws63_thread_create(convai_thread_t **thread,
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
        (goldie_thread_handler)ws63_thread_wrapper, args, n, ss);
    if (t->handle == NULL) {
        goldie_sem_destroy(&t->exit_sem);
        goldie_free(args);
        goldie_free(t);
        return -1;
    }

    /* Forward explicit priorities using this BSP's OSAL convention.
     * Zero preserves goldie_thread_create's default scheduling. */
    if (priority != 0) {
        int rc = goldie_thread_set_priority(t->handle, (unsigned int)priority);
        printf("[ws_sdk_diag] thread=%s requested_priority=%d set_rc=%d stack=%lu\n",
               n, priority, rc, (unsigned long)ss);
    } else {
        printf("[ws_sdk_diag] thread=%s priority=BSP_default stack=%lu\n", n, (unsigned long)ss);
    }

    *thread = t;
    return 0;
}

/* wait for thread to call exit_sem_post */
static void ws63_thread_join(convai_thread_t *thread) {
    if (thread == NULL) return;
    goldie_sem_wait(&thread->exit_sem);
    goldie_sem_post(&thread->exit_sem); /* Repeatable completion latch. */
}

/* safe destroy: wait if thread not exited, then release resources */
static void ws63_thread_destroy(convai_thread_t *thread) {
    if (thread == NULL) return;
    goldie_sem_wait(&thread->exit_sem);
    /* Destroy the underlying goldie/LiteOS thread so its task stack is returned
     * to the LiteOS task pool. Without this the convai_thread_t struct is freed
     * (heap) but the LiteOS task (e.g. the 16KB convai_io stack) leaks in the
     * task pool — repeated start/stop (or reconnect cycles) exhaust the pool and
     * subsequent LOS_TaskCreate fails with 0x3000200 (LOS_ERRNO_TSK_TSKMEMOUT),
     * taking down convai_audio / convai_playback. */
    if (thread->handle) {
        goldie_thread_destroy(thread->handle);
        thread->handle = NULL;
    }
    goldie_sem_destroy(&thread->exit_sem);
    goldie_free(thread);
}

/* ===== OSAL – Misc ===== */
static int ws63_fill_random(uint8_t *buf, size_t len) {
    if (buf == NULL) return -1;
    for (size_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(rand() & 0xFF);
    }
    return 0;
}

static char *ws63_strdup(const char *s) {
    if (s == NULL) return NULL;
    size_t len = strlen(s) + 1;
    char *dup = (char*)goldie_malloc(len);
    if (dup) memcpy(dup, s, len);
    return dup;
}

/* ===== NetAL – Socket ===== */
static int ws63_socket_create(convai_socket_t **sock) {
    if (sock == NULL) return -1;
    *sock = (convai_socket_t*)goldie_malloc(sizeof(**sock));
    if (*sock == NULL) return -1;
    memset(*sock, 0, sizeof(**sock));
    mbedtls_net_init(&(*sock)->net);
    (*sock)->diag.report_at = sys_now();
    return 0;
}

static int ws63_socket_destroy(convai_socket_t *sock) {
    if (sock == NULL) return 0;
    /* Explicitly close the lwIP fd. mbedtls_net_free() in the prebuilt mbedtls
     * lib may call musl close() (not lwip_close), which does NOT release the
     * lwIP socket — the fd then leaks (observed fd 0→1→... across reconnects),
     * and each leaked socket keeps its send/recv buffers (~tens of KB), driving
     * the heap toward exhaustion over reconnect cycles. lwip_close is the
     * authoritative close for an fd created by lwip_socket. */
    if (sock->net.fd >= 0) {
        lwip_close(sock->net.fd);
        sock->net.fd = -1;
    }
    mbedtls_net_free(&sock->net);
    goldie_free(sock);
    return 0;
}

/* Non-blocking TCP connect: resolve host, create a socket, set it non-blocking,
 * and initiate connect (returns immediately; EINPROGRESS is the expected "in
 * progress" result). The fd is stored in sock->net.fd so the SDK poll loop can
 * drive it (poll WRITE + getsockopt SO_ERROR) and the TLS BIO can use it.
 * Returns 0 if connect was initiated (including EINPROGRESS), <0 on error. */
static int ws63_socket_connect(convai_socket_t *sock, const char *host, uint16_t port) {
    if (sock == NULL || host == NULL) return -1;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", port);

    printf("[I] ws63_net: resolving %s:%s ...\n", host, port_str);
    struct addrinfo *res = NULL;
    int gai = lwip_getaddrinfo(host, port_str, &hints, &res);
    if (gai != 0 || res == NULL) {
        printf("[E] ws63_net: DNS resolve failed for %s (gai=%d)\n", host, gai);
        return -1;
    }

    /* Log the resolved address so we can confirm the address conversion worked. */
    {
        char addr_str[INET_ADDRSTRLEN] = {0};
        struct sockaddr_in *sin = (struct sockaddr_in *)res->ai_addr;
        const char *s = lwip_inet_ntop(AF_INET, &sin->sin_addr, addr_str, sizeof(addr_str));
        printf("[I] ws63_net: resolved %s -> %s (port=%u)\n", host,
               s ? addr_str : "?", port);
    }

    int fd = lwip_socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        printf("[E] ws63_net: lwip_socket failed (errno=%d)\n", errno);
        lwip_freeaddrinfo(res);
        return -1;
    }
    printf("[I] ws63_net: socket created fd=%d\n", fd);

    /* Non-blocking mode before connect so lwip_connect returns EINPROGRESS. */
    unsigned long nb = 1UL;
    lwip_ioctl(fd, FIONBIO, &nb);

    int cr = lwip_connect(fd, res->ai_addr, res->ai_addrlen);
    int saved_errno = errno;
    lwip_freeaddrinfo(res);

    if (cr != 0 && saved_errno != EINPROGRESS) {
        printf("[E] ws63_net: lwip_connect failed (errno=%d)\n", saved_errno);
        lwip_close(fd);
        return -1;
    }

    printf("[I] ws63_net: connect initiated (fd=%d, %s)\n", fd,
           (cr == 0) ? "connected" : "EINPROGRESS");

    /* Connect initiated (cr == 0 already connected, or EINPROGRESS pending).
     * Store the lwIP fd in the mbedtls_net_context so the TLS BIO (which calls
     * mbedtls_net_send/recv -> lwip_send/recv) and socket_poll both use it. */
    sock->net.fd = fd;
    return 0;
}

static int ws63_socket_send(convai_socket_t *sock, const uint8_t *buf, size_t len, size_t *sent) {
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

static int ws63_socket_recv(convai_socket_t *sock, uint8_t *buf, size_t len, size_t *recvd) {
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

static int ws63_socket_set_nonblock(convai_socket_t *sock, int non_block) {
    if (sock == NULL) return -1;
    if (sock->net.fd < 0) return -1;
    /* Use lwip_ioctl(FIONBIO) directly because mbedtls_net_set_nonblock
     * depends on fcntl() which is unavailable in the WS63 LiteOS build. */
    unsigned long nonblock = non_block ? 1UL : 0UL;

    return lwip_ioctl(sock->net.fd, FIONBIO, &nonblock);
}

static int ws63_socket_is_connected(convai_socket_t *sock) {
    if (sock == NULL) return 0;
    return sock->net.fd >= 0 ? 1 : 0;
}

static int ws63_socket_get_fd(convai_socket_t *sock) {
    if (sock == NULL) return -1;
    return sock->net.fd;
}

static int ws63_socket_get_error(convai_socket_t *sock) {
    if (sock == NULL || sock->net.fd < 0) return -1;
    int so_error = 0;
    socklen_t optlen = sizeof(so_error);
    if (lwip_getsockopt(sock->net.fd, SOL_SOCKET, SO_ERROR, &so_error, &optlen) < 0)
        return -1;
    return so_error;  /* 0 = no error, positive = errno */
}

/* ===== TLSAL – mbedTLS implementation (platform layer; SDK is mbedtls-free) ===== */

/* BIO callbacks for mbedTLS (internal). Call mbedtls_net_* directly so that on a
 * non-blocking socket a would-block surfaces as MBEDTLS_ERR_SSL_WANT_WRITE/WANT_READ
 * (the ws63_socket_* wrappers collapse it to -1, which would be a fatal error). */
static int ws63_tls_bio_send(void *ctx, const unsigned char *buf, size_t len)
{
    convai_socket_t *sock = (convai_socket_t *)ctx;
    if (sock == NULL) return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    int ret = mbedtls_net_send(&sock->net, buf, len);
    if (ret > 0) sock->diag.window.bio_tx += (uint32_t)ret;
    return ret;
}

static int ws63_tls_bio_recv(void *ctx, unsigned char *buf, size_t len)
{
    convai_socket_t *sock = (convai_socket_t *)ctx;
    if (sock == NULL) return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    int ret = mbedtls_net_recv(&sock->net, buf, len);
    if (ret > 0) sock->diag.window.bio_rx += (uint32_t)ret;
    return ret;
}

static int ws63_tls_create(convai_tls_t **tls)
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
    mbedtls_ssl_conf_authmode(&t->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    /* Silence mbedTLS debug output (e.g. "mbedtls_ssl_read_record ..." prints) which
     * floods the log and burns CPU during non-blocking reads. */
    mbedtls_ssl_conf_dbg(&t->conf, NULL, NULL);

    ret = mbedtls_ssl_setup(&t->ssl, &t->conf);
    if (ret != 0) goto tls_create_fail;

    *tls = t;
    return 0;

tls_create_fail:
    /* Release every mbedTLS sub-object that was init'd, in reverse order, so a
     * setup failure doesn't leak the entropy/drbg/ssl internals already allocated.
     * mbedtls_*_free are safe on a context that was init'd but not fully set up. */
    mbedtls_ssl_free(&t->ssl);
    mbedtls_ssl_config_free(&t->conf);
    mbedtls_ctr_drbg_free(&t->ctr_drbg);
    mbedtls_entropy_free(&t->entropy);
    mbedtls_x509_crt_free(&t->cacert);
    goldie_free(t);
    return -1;
}

static int ws63_tls_destroy(convai_tls_t *tls)
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
 * The handshake itself is driven incrementally by ws63_tls_handshake_step
 * (called from the SDK poll loop) so the SDK's IO thread is never blocked.
 * @param ca_cert  PEM CA cert; non-NULL → parse + VERIFY_REQUIRED,
 *                  NULL is rejected: configure a CA for custom endpoints. */
static int ws63_tls_connect(convai_tls_t *tls, void *sock, const char *host,
                            const char *ca_cert)
{
    if (tls == NULL || sock == NULL || host == NULL) return -1;

    convai_socket_t *socket = (convai_socket_t *)sock;
    tls->sock = socket;
    socket->diag = (ws63_delivery_diag_t){.report_at = sys_now()};

    mbedtls_ssl_set_hostname(&tls->ssl, host);
    mbedtls_ssl_set_bio(&tls->ssl, socket,
                        ws63_tls_bio_send, ws63_tls_bio_recv, NULL);

    if (!ca_cert || !ca_cert[0]) ca_cert = WS_AI_CA_CERT_PEM;
    if (ca_cert[0]) {
        /* Load the CA cert and require server certificate verification. */
        int ret = mbedtls_x509_crt_parse(&tls->cacert,
                                          (const unsigned char *)ca_cert,
                                          strlen(ca_cert) + 1);
        if (ret < 0) {
            printf("[E] ws63_tls: CA cert parse failed: -0x%x\n", (unsigned int)(-ret));
            return -1;
        }
        mbedtls_ssl_conf_ca_chain(&tls->conf, &tls->cacert, NULL);
        mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
        printf("[I] VERIFY_REQUIRED (CA cert loaded, %d bytes)\n",
               (int)strlen(ca_cert));
    } else {
        printf("[E] ws63_tls: CA certificate required\n");
        return -1;
    }

    return 0;
}

/* One non-blocking step of the TLS handshake.
 *   *done=1            : handshake complete
 *   *want_flags=POLL_* : need to poll socket in that direction, then re-call
 *   return <0           : fatal handshake error */
static int ws63_tls_handshake_step(convai_tls_t *tls, int *want_flags, int *done)
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
    printf("[E] ws63_tls: handshake failed: -0x%x\n", (unsigned int)(-ret));
    uint32_t flags = mbedtls_ssl_get_verify_result(&tls->ssl);
    printf("[E] ws63_tls: verify flags=0x%08x, TLS time=%lld, SDK time=%llu\n",
           (unsigned)flags, (long long)mbedtls_time(NULL),
           (unsigned long long)(ws63_get_time_ms() / 1000));
    if (flags) {
        char reason[384];
        mbedtls_x509_crt_verify_info(reason, sizeof(reason), "  ", flags);
        printf("[E] ws63_tls: %s\n", reason);
    }
    return -1;
}

static int ws63_tls_read(convai_tls_t *tls, uint8_t *buf, size_t len, size_t *nread)
{
    if (nread) *nread = 0;
    if (tls == NULL || buf == NULL) return -1;

    uint32_t start = sys_now();
    int ret = mbedtls_ssl_read(&tls->ssl, (unsigned char *)buf, (int)len);
    uint32_t now = sys_now();
    uint32_t pending = (uint32_t)mbedtls_ssl_get_bytes_avail(&tls->ssl);
    if (tls->sock)
        ws63_delivery_read(&tls->sock->diag, now, now - start, ret,
                           ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE,
                           pending);
    if (now - start >= 100 && now - tls->diag_read_report >= 1000) {
        tls->diag_read_report = now;
        printf("[ws_sdk_diag] t_ms=%lu tls_read_ms=%lu rc=%d pending=%lu\n",
               (unsigned long)now, (unsigned long)(now - start), ret, (unsigned long)pending);
    }
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

static int ws63_tls_write(convai_tls_t *tls, const uint8_t *buf, size_t len, size_t *nwrite)
{
    if (nwrite) *nwrite = 0;
    if (tls == NULL || buf == NULL) return -1;

    uint32_t start = sys_now();
    int ret = mbedtls_ssl_write(&tls->ssl, (const unsigned char *)buf, (int)len);
    uint32_t now = sys_now();
    if (ret > 0 && tls->sock) tls->sock->diag.window.plain_tx += (uint32_t)ret;
    if (now - start >= 100 && now - tls->diag_write_report >= 1000) {
        tls->diag_write_report = now;
        printf("[ws_sdk_diag] t_ms=%lu tls_write_ms=%lu rc=%d requested=%lu\n",
               (unsigned long)now, (unsigned long)(now - start), ret, (unsigned long)len);
    }
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

static int ws63_tls_close(convai_tls_t *tls)
{
    if (tls == NULL) return -1;
    mbedtls_ssl_close_notify(&tls->ssl);
    tls->connected = 0;
    if (tls->sock) {
        tls->sock->diag.read_seen = false;
        tls->sock->diag.pending = 0;
    }
    return 0;
}

static void ws63_poll_diag(convai_socket_t *sock, int events, int ready,
                           uint32_t started, int timeout_ms) {
    ws63_delivery_diag_t *d = &sock->diag;
    uint32_t now = sys_now();
    ws63_delivery_poll(d, (events & CONVAI_POLL_READ) != 0,
                       (ready & CONVAI_POLL_READ) != 0, (ready & CONVAI_POLL_WRITE) != 0);
    if (!ws63_delivery_due(d, now)) return;
    const ws63_delivery_window_t *w = &d->window;
    printf("[ws_sdk_diag] t_ms=%lu fd=%d window_ms=%lu poll_ms=%lu requested_ms=%d events=%d ready=%d polls=%lu rd=%lu wr=%lu pending_wait=%lu pending_after_read=%lu read_seen=%d read_age_ms=%lu bio_rx=%lu bio_tx=%lu tls_rx=%lu tls_tx=%lu reads=%lu wants=%lu errors=%lu read_max_ms=%lu\n",
           (unsigned long)now, sock->net.fd, (unsigned long)(now - d->report_at),
           (unsigned long)(now - started), timeout_ms, events, ready,
           (unsigned long)w->polls, (unsigned long)w->read_ready, (unsigned long)w->write_ready,
           (unsigned long)w->pending_wait, (unsigned long)d->pending, d->read_seen,
           (unsigned long)(d->read_seen ? now - d->read_at : 0),
           (unsigned long)w->bio_rx, (unsigned long)w->bio_tx,
           (unsigned long)w->plain_rx, (unsigned long)w->plain_tx,
           (unsigned long)w->reads, (unsigned long)w->wants, (unsigned long)w->errors,
           (unsigned long)w->read_max_ms);
    ws63_delivery_reported(d, now);
}

/* ===== NetAL: socket_poll (required by the poll architecture) =====
 *
 * WS63 lwip_select() does NOT honour a non-zero timeout when there are no ready
 * fds: it returns ret=0 immediately (confirmed by tracing select ret=0 printed
 * very fast). This busy-loops the IO thread (CPU 100%, mbedtls debug flood) and
 * eventually overflows the stack on the 401 path. musl poll() is not linked on
 * this target (undefined reference to `poll`).
 *
 * Workaround: poll with a zero-timeout select (non-blocking readiness check) and
 * sleep in small slices when nothing is ready, retrying up to timeout_ms/slice.
 * Event latency is bounded by the slice (10ms). No time-of-day measurement is
 * used (ws63_get_time_ms has only second resolution), just a slice counter. */
static int ws63_socket_poll(convai_socket_t *sock, int events, int *revents, int timeout_ms) {
    if (sock == NULL || revents == NULL) return -1;
    *revents = 0;

    int fd = sock->net.fd;
    if (fd < 0) return -1;

    #define WS63_POLL_SLICE_MS 10
    int slices = (timeout_ms > 0) ? (timeout_ms / WS63_POLL_SLICE_MS) : 0;
    uint32_t poll_start = sys_now();

    for (int i = 0; i <= slices; i++) {
        fd_set rfds, wfds;
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        if (events & CONVAI_POLL_READ)  FD_SET(fd, &rfds);
        if (events & CONVAI_POLL_WRITE) FD_SET(fd, &wfds);

        struct timeval ztv;
        ztv.tv_sec = 0;
        ztv.tv_usec = 0;
        int ret = select(fd + 1, &rfds, &wfds, NULL, &ztv);
        if (ret < 0) {
            printf("[E] ws63_net: select failed fd=%d errno=%d\n", fd, errno);
            return -1;
        }
        if (FD_ISSET(fd, &rfds)) *revents |= CONVAI_POLL_READ;
        if (FD_ISSET(fd, &wfds)) *revents |= CONVAI_POLL_WRITE;
        if (*revents != 0) {
            ws63_poll_diag(sock, events, *revents, poll_start, timeout_ms);
            return 0;  /* event ready */
        }
        if (i < slices) {
            goldie_msleep(WS63_POLL_SLICE_MS);  /* wait a slice, then re-check */
        }
    }
    ws63_poll_diag(sock, events, *revents, poll_start, timeout_ms);
    return 0;  /* timed out, no event */
}

/* ===== Misc ===== */
static void ws63_log(int level, const char *file, int line, const char *fmt, ...) {
    char buf[256];
    va_list args;
    uint64_t now_ms = ws63_get_time_ms();
    uint32_t sec = (uint32_t)(now_ms / 1000);
    uint32_t ms = (uint32_t)(now_ms % 1000);
    int pos = snprintf(buf, sizeof(buf), "[%u.%03u] [%c] [%s:%d] ",
                       sec, ms,
                       level == 0 ? 'E' : level == 1 ? 'W' : level == 2 ? 'I' : 'D',
                       file ? file : "???", line);
    if (pos < 0) return;
    if (pos > (int)sizeof(buf) - 3) pos = sizeof(buf) - 3;
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

int ws63_device_id(char *buf, size_t len) {
    if (buf == NULL || len < 18) return -1;

    WifiService *wifi_svc = (WifiService *)get_service(WIFI_SERVICE_INDEX);
    if (wifi_svc != NULL && wifi_svc->svr_get_hwddr != NULL) {
        uint8_t hw_addr[6] = {0};
        if (wifi_svc->svr_get_hwddr(hw_addr, 6) == 0) {
            int is_valid = 0;
            for (int i = 0; i < 6; i++) {
                if (hw_addr[i] != 0x00 && hw_addr[i] != 0xFF) {
                    is_valid = 1;
                    break;
                }
            }
            if (is_valid) {
                snprintf(buf, len, "%02X%02X%02X%02X%02X%02X",
                         hw_addr[0], hw_addr[1], hw_addr[2],
                         hw_addr[3], hw_addr[4], hw_addr[5]);
                return (int)strlen(buf);
            }
        }
    }

    snprintf(buf, len, "ws63-device");
    return (int)strlen(buf);
}

static int ws63_random(uint8_t *buf, size_t len) {
    return ws63_fill_random(buf, len);
}

static int ws63_uuid(char *buf, size_t size) {
    if (buf == NULL || size == 0) return -1;
    snprintf(buf, size, "00000000-0000-0000-0000-000000000000");
    return (int)strlen(buf);
}

static int ws63_info(char *buf, size_t size) {
    if (buf == NULL || size == 0) return -1;
    snprintf(buf, size, "ws63-liteos");
    return (int)strlen(buf);
}

static int ws63_network_available(void) {
    return 1;
}

static int ws63_network_get_type(char *buf, size_t size) {
    if (buf == NULL || size == 0) return -1;
    snprintf(buf, size, "wifi");
    return (int)strlen(buf);
}

/* ===== Platform structure instance ===== */
const convai_platform_t g_convai_platform = {
    .abi_version = CONVAI_ABI_VERSION,
    ._reserved = 0,
    .osal = {
        .malloc = ws63_malloc,
        .free = ws63_free,
        .get_time_ms = ws63_get_time_ms,
        .sleep_ms = ws63_sleep_ms,
        .get_tick_ms = ws63_tick_ms,
        .mutex_create = ws63_mutex_create,
        .mutex_destroy = ws63_mutex_destroy,
        .mutex_lock = ws63_mutex_lock,
        .mutex_unlock = ws63_mutex_unlock,
        .thread_create = ws63_thread_create,
        .thread_join = ws63_thread_join,
        .thread_destroy = ws63_thread_destroy,
        .fill_random = ws63_fill_random,
        .strdup = ws63_strdup,
    },
    .netal = {
        .socket_create = ws63_socket_create,
        .socket_destroy = ws63_socket_destroy,
        .socket_connect = ws63_socket_connect,
        .socket_send = ws63_socket_send,
        .socket_recv = ws63_socket_recv,
        .socket_set_nonblock = ws63_socket_set_nonblock,
        .socket_is_connected = ws63_socket_is_connected,
        .socket_get_fd = ws63_socket_get_fd,
        .socket_poll = ws63_socket_poll,
        .socket_get_error = ws63_socket_get_error,
    },
    .tlsal = {
        .tls_create = ws63_tls_create,
        .tls_destroy = ws63_tls_destroy,
        .tls_connect = ws63_tls_connect,
        .tls_handshake_step = ws63_tls_handshake_step,
        .tls_read = ws63_tls_read,
        .tls_write = ws63_tls_write,
        .tls_close = ws63_tls_close,
    },
    .misc = {
        .log = ws63_log,
        .device_id = ws63_device_id,
        .random = ws63_random,
        .uuid = ws63_uuid,
        .info = ws63_info,
        .network_available = ws63_network_available,
        .network_get_type = ws63_network_get_type,
    },
};

int convai_platform_ws63_init(void) {
    if (mbedtls_platform_set_time(ws63_tls_time) != 0) return -1;
    return convai_platform_init(&g_convai_platform);
}
