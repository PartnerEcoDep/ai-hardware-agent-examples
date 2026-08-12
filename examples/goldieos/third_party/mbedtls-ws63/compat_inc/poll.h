/*
 * Minimal poll.h shim for building the WS63-hardened mbedTLS net_sockets.c
 * outside the full LiteOS tree.
 *
 * lwip/sockets.h (HiSilicon fork, LWIP_LITEOS_COMPAT + LWIP_EXT_POLL_SUPPORT)
 * includes <poll.h> for the LiteOS kernel type poll_table, which is used only
 * as an opaque pointer in the lwip_poll() declaration.  The musl toolchain
 * poll.h does not provide it, so shadow it for this target only.
 * net_sockets.c itself never calls poll().
 */
#ifndef MBEDTLS_WS63_COMPAT_POLL_H
#define MBEDTLS_WS63_COMPAT_POLL_H

/* LiteOS 内核类型, 此处仅作不透明指针使用 */
typedef struct poll_table { int _unused; } poll_table;

#endif /* MBEDTLS_WS63_COMPAT_POLL_H */
