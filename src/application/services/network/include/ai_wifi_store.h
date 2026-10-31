#ifndef AI_WIFI_STORE_H
#define AI_WIFI_STORE_H
#include "ai_provision.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Caller serializes mutations. Backend owns durable format, checksum/generation,
 * commit/readback, and must return false on failure. Current RAM state is only
 * published after success; this alone does not guarantee backend atomicity. */
typedef struct {
    void *ctx;
    bool (*save)(void *ctx, ai_wifi_profiles_t *next, const ai_wifi_profiles_t *current);
} ai_wifi_store_ops_t;
bool ai_wifi_store_put(ai_wifi_profiles_t *current, const ai_wifi_store_ops_t *ops,
                       const char *ssid, const char *password);
bool ai_wifi_store_remove(ai_wifi_profiles_t *current, const ai_wifi_store_ops_t *ops, unsigned index);
bool ai_wifi_store_default(ai_wifi_profiles_t *current, const ai_wifi_store_ops_t *ops, unsigned index);
#ifdef __cplusplus
}
#endif
#endif
