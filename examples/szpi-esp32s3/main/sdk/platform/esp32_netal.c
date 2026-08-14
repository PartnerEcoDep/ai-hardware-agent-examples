/**
 * @file platform/esp32_netal.c
 * @brief NetAL layer of the ESP32-S3 ConvAI platform HAL (lwIP BSD sockets).
 *
 * Sockets are non-blocking end to end: after creation the fd gets O_NONBLOCK,
 * connect() returns EINPROGRESS immediately, and the SDK drives completion
 * with socket_poll(CONVAI_POLL_WRITE) + socket_get_error(SO_ERROR).
 *
 * Return convention used by send/recv here:
 *   0  = success (byte count reported via the out parameter, may be 0 on
 *        would-block)
 *   -1 = fatal error or peer close
 */

#include "convai_platform_esp32_internal.h"

#include "esp_log.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *TAG = CONVAI_HAL_TAG;

/** True when errno says "no data / buffer full right now, retry later". */
static inline bool is_would_block(int err) {
  return err == EAGAIN || err == EWOULDBLOCK;
}

int esp32_socket_create(convai_socket_t **sock) {
  if (sock == NULL) {
    return -1;
  }
  *sock = (convai_socket_t *)calloc(1, sizeof(**sock));
  if (*sock == NULL) {
    return -1;
  }
  (*sock)->fd = -1;
  return 0;
}

int esp32_socket_destroy(convai_socket_t *sock) {
  if (sock == NULL) {
    return 0;
  }
  if (sock->fd >= 0) {
    close(sock->fd);
    sock->fd = -1;
  }
  free(sock);
  return 0;
}

int esp32_socket_connect(convai_socket_t *sock, const char *host,
                         uint16_t port) {
  if (sock == NULL || host == NULL) {
    return -1;
  }

  /* 1. Resolve the host name. */
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
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

  /* 2. Create the TCP socket. */
  int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (fd < 0) {
    ESP_LOGE(TAG, "socket() failed, errno=%d", errno);
    freeaddrinfo(res);
    return -1;
  }

  /* 3. Switch to non-blocking before connecting. */
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);

  /* 4. Kick off the connect; EINPROGRESS is the expected result. */
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

int esp32_socket_send(convai_socket_t *sock, const uint8_t *buf, size_t len,
                      size_t *sent) {
  if (sent != NULL) {
    *sent = 0;
  }
  if (sock == NULL || buf == NULL || sock->fd < 0) {
    return -1;
  }

  ssize_t ret = send(sock->fd, buf, len, 0);
  if (ret < 0) {
    return is_would_block(errno) ? 0 : -1;
  }
  if (sent != NULL) {
    *sent = (size_t)ret;
  }
  return 0;
}

int esp32_socket_recv(convai_socket_t *sock, uint8_t *buf, size_t len,
                      size_t *recvd) {
  if (recvd != NULL) {
    *recvd = 0;
  }
  if (sock == NULL || buf == NULL || sock->fd < 0) {
    return -1;
  }

  ssize_t ret = recv(sock->fd, buf, len, 0);
  if (ret == 0) {
    return -1; /* peer closed the connection */
  }
  if (ret < 0) {
    return is_would_block(errno) ? 0 : -1;
  }
  if (recvd != NULL) {
    *recvd = (size_t)ret;
  }
  return 0;
}

int esp32_socket_set_nonblock(convai_socket_t *sock, int non_block) {
  if (sock == NULL || sock->fd < 0) {
    return -1;
  }
  int flags = fcntl(sock->fd, F_GETFL, 0);
  if (flags < 0) {
    return -1;
  }
  if (non_block) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~O_NONBLOCK;
  }
  return fcntl(sock->fd, F_SETFL, flags);
}

int esp32_socket_is_connected(convai_socket_t *sock) {
  if (sock == NULL) {
    return 0;
  }
  return (sock->fd >= 0) ? 1 : 0;
}

int esp32_socket_get_fd(convai_socket_t *sock) {
  if (sock == NULL) {
    return -1;
  }
  return sock->fd;
}

int esp32_socket_get_error(convai_socket_t *sock) {
  if (sock == NULL || sock->fd < 0) {
    return -1;
  }
  int so_error = 0;
  socklen_t optlen = sizeof(so_error);
  if (getsockopt(sock->fd, SOL_SOCKET, SO_ERROR, &so_error, &optlen) < 0) {
    return -1;
  }
  return so_error; /* 0 = no error, positive = errno value */
}

int esp32_socket_poll(convai_socket_t *sock, int events, int *revents,
                      int timeout_ms) {
  if (sock == NULL || revents == NULL) {
    return -1;
  }
  *revents = 0;

  int fd = sock->fd;
  if (fd < 0) {
    return -1;
  }

  fd_set rfds;
  fd_set wfds;
  FD_ZERO(&rfds);
  FD_ZERO(&wfds);
  if (events & CONVAI_POLL_READ) {
    FD_SET(fd, &rfds);
  }
  if (events & CONVAI_POLL_WRITE) {
    FD_SET(fd, &wfds);
  }

  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  int ret = select(fd + 1, &rfds, &wfds, NULL, &tv);
  if (ret < 0) {
    ESP_LOGE(TAG, "select() failed fd=%d errno=%d", fd, errno);
    return -1;
  }
  if (FD_ISSET(fd, &rfds)) {
    *revents |= CONVAI_POLL_READ;
  }
  if (FD_ISSET(fd, &wfds)) {
    *revents |= CONVAI_POLL_WRITE;
  }
  return 0;
}
