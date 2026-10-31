#ifndef WS_DHCP_GATEWAY_H
#define WS_DHCP_GATEWAY_H
#include <stddef.h>
#include <stdint.h>
/* Add missing router to padded DHCP OFFER/ACK; keep packet length unchanged.
 * Operates on a private IPv4 packet copy. Returns 1 only when modified. */
int ws_dhcp_add_gateway(uint8_t *packet, size_t length, const uint8_t gateway[4]);
#endif
