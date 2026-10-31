#ifndef ESP_NETWORK_H
#define ESP_NETWORK_H
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
int esp_network_init(void);
bool esp_network_connected(void);
void esp_network_provision(void);
void esp_network_description(char *text, size_t capacity);
void esp_network_poll(void);
bool esp_network_time_ready(void);
#ifdef __cplusplus
}
#endif
#endif
