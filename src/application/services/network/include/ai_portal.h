#ifndef AI_PORTAL_H
#define AI_PORTAL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AI_PORTAL_HTTP_STATUS "302 Found"
#define AI_PORTAL_CACHE_CONTROL "no-store"
/* In-place UDP DNS reply. Address is four bytes in network order.
 * Returns zero for malformed/unsupported queries or insufficient capacity.
 * IN/A resolves to the portal; other IN types return NOERROR with no answers.
 * Single, uncompressed standard questions only; EDNS additions are stripped. */
size_t ai_portal_dns_reply(uint8_t *packet, size_t length, size_t capacity,
                          const uint8_t address[4]);
#ifdef __cplusplus
}
#endif
#endif
