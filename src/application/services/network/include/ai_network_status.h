#ifndef AI_NETWORK_STATUS_H
#define AI_NETWORK_STATUS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    AI_NET_STARTING, AI_NET_CONNECTING, AI_NET_CONNECTED,
    AI_NET_PROVISIONING, AI_NET_SETUP_FAILED, AI_NET_HOST_ONLINE, AI_NET_HOST_OFFLINE
} ai_network_state_t;
/* Public status never contains passwords. Platform owns acquisition/synchronization. */
typedef struct {
    ai_network_state_t state;
    char address[64];
    char setup_ssid[33];
    char setup_url[80];
} ai_network_status_t;
void ai_network_format(const ai_network_status_t *status, char *text, size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
