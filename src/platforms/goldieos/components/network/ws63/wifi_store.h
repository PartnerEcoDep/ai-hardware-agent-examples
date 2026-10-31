#ifndef WS_WIFI_STORE_H
#define WS_WIFI_STORE_H
#include "ai_provision.h"
/* Application-owned NV keys in SDK user-normal region [0x5000,0xffff).
 * Reserve these two IDs when integrating with another product. */
#define WS_WIFI_NV_KEY0 0x5100
#define WS_WIFI_NV_KEY1 0x5101
void ws_wifi_store_load(ai_wifi_profiles_t *store);
bool ws_wifi_store_save(ai_wifi_profiles_t *next, const ai_wifi_profiles_t *current);
#endif
