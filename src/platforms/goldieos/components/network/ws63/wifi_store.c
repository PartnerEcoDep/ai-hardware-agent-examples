#include "wifi_store.h"
#include "errcode.h"
#include <string.h>
/* Minimal ABI from HiSilicon NV API; BSP ships the implementation in libnv.
 * See src/platforms/goldieos/README.md for the interface and key-range references. */
extern errcode_t uapi_nv_read(uint16_t, uint16_t, uint16_t *, uint8_t *);
extern errcode_t uapi_nv_write(uint16_t, const uint8_t *, uint16_t);
_Static_assert(sizeof(ai_wifi_profiles_t) <= 4060, "NV value exceeds SDK limit");
static uint16_t key(unsigned slot) { return slot ? WS_WIFI_NV_KEY1 : WS_WIFI_NV_KEY0; }
void ws_wifi_store_load(ai_wifi_profiles_t *store) {
    ai_wifi_profiles_t candidate;
    memset(store, 0, sizeof(*store));
    for (unsigned i = 0; i < 2; ++i) {
        uint16_t length = 0;
        errcode_t rc = uapi_nv_read(key(i), sizeof(candidate), &length, (uint8_t *)&candidate);
        if (!rc && length == sizeof(candidate) && ai_wifi_profiles_valid(&candidate) &&
            (!store->magic || (int32_t)(candidate.generation - store->generation) > 0))
            *store = candidate;
    }
    memset(&candidate, 0, sizeof(candidate));
}
bool ws_wifi_store_save(ai_wifi_profiles_t *next, const ai_wifi_profiles_t *current) {
    ai_wifi_profiles_t check = {0}; uint16_t length = 0;
    next->magic = 0x41495731; next->generation = current->generation + 1;
    next->checksum = ai_wifi_profiles_checksum(next);
    if (!ai_wifi_profiles_valid(next)) return false;
    uint16_t id = key(next->generation & 1);
    errcode_t rc = uapi_nv_read(id, sizeof(check), &length, (uint8_t *)&check);
    /* Do not overwrite another product's data or treat I/O errors as empty. */
    bool owned = !rc && length == sizeof(check) && check.magic == next->magic;
    bool ok = owned || rc == ERRCODE_NV_KEY_NOT_FOUND;
    if (ok) ok = uapi_nv_write(id, (const uint8_t *)next, sizeof(*next)) == 0;
    if (ok) {
        length = 0;
        ok = !uapi_nv_read(id, sizeof(check), &length, (uint8_t *)&check) &&
            length == sizeof(check) && !memcmp(&check, next, sizeof(check));
    }
    memset(&check, 0, sizeof(check));
    return ok;
}
