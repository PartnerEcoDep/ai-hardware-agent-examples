#ifndef AI_PROVISION_H
#define AI_PROVISION_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AI_WIFI_SAVED_MAX 10
typedef struct { char ssid[33], password[65]; } ai_wifi_profile_t;
typedef struct {
    uint32_t magic, generation, count;
    ai_wifi_profile_t items[AI_WIFI_SAVED_MAX];
    uint32_t checksum;
} ai_wifi_profiles_t;
bool ai_wifi_credentials(const char *ssid, const char *password);
int ai_wifi_index(const char *query, unsigned count);
uint32_t ai_wifi_profiles_checksum(const ai_wifi_profiles_t *store);
bool ai_wifi_profiles_valid(const ai_wifi_profiles_t *store);
/* Parse a complete, NUL-terminated HTTP header (maximum 2048 bytes).
 * Reject ambiguous lengths, unsupported transfer encodings and oversized body.
 * Output buffers are method[8], path[128]. */
bool ai_provision_headers(char *header, char *method, char *path, size_t *body_length);
#ifdef __cplusplus
}
#endif
#endif
