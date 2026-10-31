#include "wifi_store.h"
#include "errcode.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static ai_wifi_profiles_t slots[2];
static bool present[2], fail_write, fail_read;
errcode_t uapi_nv_read(uint16_t id, uint16_t capacity, uint16_t *length, uint8_t *data) {
    unsigned i = id - WS_WIFI_NV_KEY0; assert(i < 2);
    if (fail_read) return ERRCODE_NV_READ_FLASH_ERR;
    if (!present[i]) return ERRCODE_NV_KEY_NOT_FOUND;
    assert(capacity >= sizeof(slots[i]));
    *length = sizeof(slots[i]); memcpy(data, &slots[i], *length); return 0;
}
errcode_t uapi_nv_write(uint16_t id, const uint8_t *data, uint16_t length) {
    unsigned i = id - WS_WIFI_NV_KEY0; assert(i < 2 && length == sizeof(slots[i]));
    if (fail_write) return ERRCODE_NV_NO_ENOUGH_SPACE;
    memcpy(&slots[i], data, length); present[i] = true; return 0;
}
int main(void) {
    ai_wifi_profiles_t current, next, restored;
    ws_wifi_store_load(&current); assert(!current.count && !current.magic);
    next = current; next.count = 1; strcpy(next.items[0].ssid, "first");
    assert(ws_wifi_store_save(&next, &current)); current = next;
    ws_wifi_store_load(&restored); assert(!memcmp(&restored, &current, sizeof(current)));
    next = current; strcpy(next.items[0].ssid, "second"); fail_write = true;
    assert(!ws_wifi_store_save(&next, &current)); fail_write = false;
    ws_wifi_store_load(&restored); assert(!strcmp(restored.items[0].ssid, "first"));
    fail_read = true; assert(!ws_wifi_store_save(&next, &current)); fail_read = false;
    present[0] = true; slots[0].magic = 0x12345678;
    assert(!ws_wifi_store_save(&next, &current)); assert(slots[0].magic == 0x12345678);
    present[0] = false; assert(ws_wifi_store_save(&next, &current));
    ws_wifi_store_load(&restored); assert(!strcmp(restored.items[0].ssid, "second"));
    slots[0].checksum ^= 1; ws_wifi_store_load(&restored);
    assert(!strcmp(restored.items[0].ssid, "first"));
    next = restored; next.count = 0; memset(next.items, 0, sizeof(next.items));
    assert(ws_wifi_store_save(&next, &restored)); ws_wifi_store_load(&restored);
    assert(restored.magic && !restored.count);
    puts("NV restart, write/read failures, foreign key, CRC fallback and deletion passed");
}
