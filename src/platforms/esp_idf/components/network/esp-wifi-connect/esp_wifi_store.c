#include "esp_wifi_store.h"
#include "nvs.h"
#include <stdio.h>
#include <string.h>
static const char *slot(unsigned n) { return n ? "profiles1" : "profiles0"; }
bool esp_wifi_store_load(ai_wifi_profiles_t *current) {
    memset(current, 0, sizeof(*current));
    nvs_handle_t handle;
    esp_err_t rc = nvs_open("wifi", NVS_READONLY, &handle);
    if (rc == ESP_ERR_NVS_NOT_FOUND) return true;
    if (rc != ESP_OK) return false;
    ai_wifi_profiles_t value = {0};
    bool had_snapshot = false;
    for (unsigned i = 0; i < 2; ++i) {
        size_t length = sizeof(value);
        rc = nvs_get_blob(handle, slot(i), &value, &length);
        if (rc != ESP_ERR_NVS_NOT_FOUND) had_snapshot = true;
        if (rc == ESP_OK && length == sizeof(value) && ai_wifi_profiles_valid(&value) &&
            (!current->magic || (int32_t)(value.generation - current->generation) > 0))
            *current = value;
    }
    bool ok = current->magic != 0;
    if (!had_snapshot) {
        ok = true;
        for (unsigned i = 0; i < AI_WIFI_SAVED_MAX; ++i) {
            char ssid_key[16], password_key[16];
            if (i) { snprintf(ssid_key, sizeof(ssid_key), "ssid%u", i); snprintf(password_key, sizeof(password_key), "password%u", i); }
            else { strcpy(ssid_key, "ssid"); strcpy(password_key, "password"); }
            ai_wifi_profile_t item = {0};
            size_t size = sizeof(item.ssid);
            rc = nvs_get_str(handle, ssid_key, item.ssid, &size);
            if (rc == ESP_ERR_NVS_NOT_FOUND) continue;
            if (rc != ESP_OK) { ok = false; break; }
            size = sizeof(item.password);
            rc = nvs_get_str(handle, password_key, item.password, &size);
            if (rc == ESP_ERR_NVS_NOT_FOUND) continue;
            if (rc != ESP_OK || !ai_wifi_credentials(item.ssid, item.password)) { ok = false; break; }
            current->items[current->count++] = item;
            memset(&item, 0, sizeof(item));
        }
    }
    memset(&value, 0, sizeof(value));
    nvs_close(handle);
    if (!ok) memset(current, 0, sizeof(*current));
    return ok;
}
bool esp_wifi_store_save(ai_wifi_profiles_t *next, const ai_wifi_profiles_t *current) {
    next->magic = 0x41495731;
    next->generation = current->generation + 1;
    next->checksum = ai_wifi_profiles_checksum(next);
    if (!ai_wifi_profiles_valid(next)) return false;
    nvs_handle_t handle;
    if (nvs_open("wifi", NVS_READWRITE, &handle) != ESP_OK) return false;
    const char *key = slot(next->generation & 1);
    ai_wifi_profiles_t check = {0};
    size_t size = sizeof(check);
    esp_err_t rc = nvs_get_blob(handle, key, &check, &size);
    bool owned = rc == ESP_OK && size == sizeof(check) && check.magic == next->magic;
    bool ok = owned || rc == ESP_ERR_NVS_NOT_FOUND;
    if (ok) ok = nvs_set_blob(handle, key, next, sizeof(*next)) == ESP_OK;
    if (ok) ok = nvs_commit(handle) == ESP_OK;
    if (ok) {
        size = sizeof(check);
        ok = nvs_get_blob(handle, key, &check, &size) == ESP_OK &&
             size == sizeof(check) && memcmp(&check, next, sizeof(check)) == 0;
    }
    memset(&check, 0, sizeof(check));
    nvs_close(handle);
    return ok;
}
