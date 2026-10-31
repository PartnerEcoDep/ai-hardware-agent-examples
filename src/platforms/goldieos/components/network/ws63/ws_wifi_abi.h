#ifndef WS_WIFI_ABI_H
#define WS_WIFI_ABI_H
#include <stdint.h>
#include <stddef.h>
/* Minimal ABI declarations for the Wi-Fi library shipped with the Hoperun BSP.
 * Source: HiSpark/fbb_ws63/src/include/middleware/services/wifi/
 * wifi_hotspot{,_config}.h and wifi_device_config.h (OPEN = 0).
 * Use explicit 32-bit fields: the application uses -fshort-enums, the vendor
 * library ABI has security/channel/PSK at offsets 100/104/108. */
typedef struct {
    int8_t ssid[33];
    int8_t pre_shared_key[65];
    int8_t reserved[2];
    int32_t security_type;
    int32_t channel_num;
    int32_t wifi_psk_type;
} ws_softap_config;
_Static_assert(sizeof(ws_softap_config) == 112, "WS63 hotspot ABI size");
_Static_assert(offsetof(ws_softap_config, security_type) == 100, "WS63 hotspot ABI offset");
extern uint32_t wifi_softap_enable(const ws_softap_config *config);
extern uint32_t wifi_softap_disable(void);
#endif
