#ifndef ESP_WIFI_STORE_H
#define ESP_WIFI_STORE_H
#include "ai_provision.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Two CRC snapshots in NVS wifi/profiles0,profiles1. Existing ssid/password
 * keys are read only when neither snapshot exists; legacy keys are retained. */
bool esp_wifi_store_load(ai_wifi_profiles_t *current);
bool esp_wifi_store_save(ai_wifi_profiles_t *next, const ai_wifi_profiles_t *current);
#ifdef __cplusplus
}
#endif
#endif
