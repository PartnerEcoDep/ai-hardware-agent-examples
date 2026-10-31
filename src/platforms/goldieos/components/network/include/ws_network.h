#ifndef WS_NETWORK_H
#define WS_NETWORK_H
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
int ws_network_init(void);
bool ws_network_connected(void);
void ws_network_provision(void);
void ws_network_description(char *text, size_t capacity);
void ws_network_enable(bool enabled);
#ifdef __cplusplus
}
#endif
#endif
