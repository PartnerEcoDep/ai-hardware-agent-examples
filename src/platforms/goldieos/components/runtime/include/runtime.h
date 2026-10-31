#ifndef WS_RUNTIME_H
#define WS_RUNTIME_H
#include <stdbool.h>
#include <stddef.h>
void ws_runtime_prepare(void);
int ws_runtime_init(void);
int ws_runtime_sync_time(void);
int ws_runtime_sdk_init(void);
int ws_runtime_device_id(char *text, size_t size);
#endif
