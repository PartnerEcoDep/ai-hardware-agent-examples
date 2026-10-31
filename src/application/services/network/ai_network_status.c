#include "ai_network_status.h"
#include <stdio.h>
void ai_network_format(const ai_network_status_t *s, char *text, size_t capacity) {
    if (!text || !capacity) return;
    if (!s) { text[0] = 0; return; }
    switch (s->state) {
    case AI_NET_CONNECTED: snprintf(text, capacity, "Wi-Fi connected\n%.63s", s->address); break;
    case AI_NET_PROVISIONING: snprintf(text, capacity, "Setup: %.32s\n%.79s", s->setup_ssid, s->setup_url); break;
    case AI_NET_CONNECTING: snprintf(text, capacity, "Connecting Wi-Fi...\nTap Wi-Fi to configure"); break;
    case AI_NET_SETUP_FAILED: snprintf(text, capacity, "Wi-Fi setup failed\nTap Wi-Fi to retry"); break;
    case AI_NET_HOST_ONLINE: snprintf(text, capacity, "Windows network: available"); break;
    case AI_NET_HOST_OFFLINE: snprintf(text, capacity, "Windows network: offline"); break;
    default: snprintf(text, capacity, "Starting Wi-Fi..."); break;
    }
}
