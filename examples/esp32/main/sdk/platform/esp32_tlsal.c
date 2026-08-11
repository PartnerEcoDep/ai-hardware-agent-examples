/**
 * @file platform/esp32_tlsal.c
 * @brief TLSAL layer of the ESP32-S3 ConvAI platform HAL (mbedTLS client).
 *
 * The BIO callbacks talk to the raw socket fd directly instead of using
 * mbedtls_net_*, so the non-blocking socket owned by NetAL keeps its
 * semantics: a would-block turns into MBEDTLS_ERR_SSL_WANT_READ/WRITE and the
 * SDK re-drives the handshake from socket_poll().
 *
 * mbedTLS 4.x is used through the PSA Crypto backend, which owns the RNG
 * (hardware TRNG); no ctr_drbg/entropy contexts are needed here.
 */

#include "convai_platform_esp32_internal.h"

#include "esp_log.h"
#include "lwip/sockets.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "psa/crypto.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = CONVAI_HAL_TAG;

/* ===================================================================
 *  BIO callbacks (raw socket fd)
 * =================================================================== */

static int esp32_tls_bio_send(void *ctx, const unsigned char *buf,
                              size_t len) {
  convai_socket_t *sock = (convai_socket_t *)ctx;
  if (sock == NULL || sock->fd < 0) {
    return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
  }
  int ret = send(sock->fd, buf, len, 0);
  if (ret < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return MBEDTLS_ERR_SSL_WANT_WRITE;
    }
    return MBEDTLS_ERR_NET_SEND_FAILED;
  }
  return ret;
}

static int esp32_tls_bio_recv(void *ctx, unsigned char *buf, size_t len) {
  convai_socket_t *sock = (convai_socket_t *)ctx;
  if (sock == NULL || sock->fd < 0) {
    return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
  }
  int ret = recv(sock->fd, buf, len, 0);
  if (ret == 0) {
    return MBEDTLS_ERR_SSL_CONN_EOF;
  }
  if (ret < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return MBEDTLS_ERR_SSL_WANT_READ;
    }
    return MBEDTLS_ERR_NET_RECV_FAILED;
  }
  return ret;
}

/* ===================================================================
 *  Lifecycle
 * =================================================================== */

int esp32_tls_create(convai_tls_t **tls) {
  if (tls == NULL) {
    return -1;
  }

  convai_tls_t *t = (convai_tls_t *)calloc(1, sizeof(*t));
  if (t == NULL) {
    return -1;
  }

  psa_crypto_init();

  mbedtls_ssl_init(&t->ssl);
  mbedtls_ssl_config_init(&t->conf);
  mbedtls_x509_crt_init(&t->cacert);

  int ret = mbedtls_ssl_config_defaults(&t->conf, MBEDTLS_SSL_IS_CLIENT,
                                        MBEDTLS_SSL_TRANSPORT_STREAM,
                                        MBEDTLS_SSL_PRESET_DEFAULT);
  if (ret != 0) {
    ESP_LOGE(TAG, "ssl_config_defaults failed: -0x%x", (unsigned int)(-ret));
    goto fail;
  }

  /* Verification mode is finalized in esp32_tls_connect() once we know
   * whether a CA certificate was supplied. */
  mbedtls_ssl_conf_authmode(&t->conf, MBEDTLS_SSL_VERIFY_NONE);
  mbedtls_ssl_conf_dbg(&t->conf, NULL, NULL);

  ret = mbedtls_ssl_setup(&t->ssl, &t->conf);
  if (ret != 0) {
    ESP_LOGE(TAG, "ssl_setup failed: -0x%x", (unsigned int)(-ret));
    goto fail;
  }

  *tls = t;
  return 0;

fail:
  mbedtls_ssl_free(&t->ssl);
  mbedtls_ssl_config_free(&t->conf);
  mbedtls_x509_crt_free(&t->cacert);
  free(t);
  return -1;
}

int esp32_tls_destroy(convai_tls_t *tls) {
  if (tls == NULL) {
    return 0;
  }
  mbedtls_ssl_free(&tls->ssl);
  mbedtls_ssl_config_free(&tls->conf);
  mbedtls_x509_crt_free(&tls->cacert);
  free(tls);
  return 0;
}

int esp32_tls_connect(convai_tls_t *tls, void *sock, const char *host,
                      const char *ca_cert) {
  if (tls == NULL || sock == NULL || host == NULL) {
    return -1;
  }

  convai_socket_t *socket_handle = (convai_socket_t *)sock;
  tls->sock = socket_handle;

  mbedtls_ssl_set_hostname(&tls->ssl, host); /* SNI */
  mbedtls_ssl_set_bio(&tls->ssl, socket_handle, esp32_tls_bio_send,
                      esp32_tls_bio_recv, NULL);

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

/* ===================================================================
 *  Handshake / IO
 * =================================================================== */

int esp32_tls_handshake_step(convai_tls_t *tls, int *want_flags, int *done) {
  if (tls == NULL || want_flags == NULL || done == NULL) {
    return -1;
  }
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

int esp32_tls_read(convai_tls_t *tls, uint8_t *buf, size_t len,
                   size_t *nread) {
  if (nread != NULL) {
    *nread = 0;
  }
  if (tls == NULL || buf == NULL) {
    return -1;
  }

  int ret = mbedtls_ssl_read(&tls->ssl, buf, len);
  if (ret < 0) {
    if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
        ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
      return 0; /* would-block */
    }
    return -1;
  }
  if (ret == 0) {
    return -1; /* peer closed */
  }
  if (nread != NULL) {
    *nread = (size_t)ret;
  }
  return 0;
}

int esp32_tls_write(convai_tls_t *tls, const uint8_t *buf, size_t len,
                    size_t *nwrite) {
  if (nwrite != NULL) {
    *nwrite = 0;
  }
  if (tls == NULL || buf == NULL) {
    return -1;
  }

  int ret = mbedtls_ssl_write(&tls->ssl, buf, len);
  if (ret < 0) {
    if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
        ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
      return 0; /* would-block */
    }
    return -1;
  }
  if (nwrite != NULL) {
    *nwrite = (size_t)ret;
  }
  return 0;
}

int esp32_tls_close(convai_tls_t *tls) {
  if (tls == NULL) {
    return -1;
  }
  mbedtls_ssl_close_notify(&tls->ssl);
  tls->connected = 0;
  return 0;
}
