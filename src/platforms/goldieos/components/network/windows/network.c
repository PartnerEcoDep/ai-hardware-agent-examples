#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <stdatomic.h>
#include "ws_network.h"
#include "ai_network_status.h"
static atomic_bool enabled = true;
int ws_network_init(void) { return 0; }
bool ws_network_connected(void) {
    DWORD flags;
    return atomic_load(&enabled) && InternetGetConnectedState(&flags,0);
}
void ws_network_enable(bool value) { atomic_store(&enabled,value); }
void ws_network_provision(void) { /* No provisioning on the desktop target. */ }
static void ws_network_status(ai_network_status_t *status) {
    if (!status) return;
    *status = (ai_network_status_t){.state = ws_network_connected() ? AI_NET_HOST_ONLINE : AI_NET_HOST_OFFLINE};
}
void ws_network_description(char *text, size_t capacity) {
    ai_network_status_t status;
    ws_network_status(&status);
    ai_network_format(&status, text, capacity);
}
